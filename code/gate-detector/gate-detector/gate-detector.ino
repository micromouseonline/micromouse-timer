#include <Arduino.h>
#include "SoftwareSerial.h"
#include "digitalWriteFast.h"

#define DEBUG 0
const int sideSensorPin = A0;
const int endSensorPin = A1;
const int SIDE_GATE = 0;
const int END_GATE = 1;

const int RADIO_DATA = 4;
const int RADIO_NC = 5;
const int RADIO_PDN = 2;
const int RADIO_TX = 3;

#define GATE_ID_PIN0 9
#define GATE_ID_PIN1 8
#define GATE_ID_PIN2 7
#define GATE_ID_PIN3 6

int gateID = 0;

const int PACKET_REPEATS = 3;
const int PACKET_REPEAT_INTERVAL = 50;
////////////////////////////////////////////////////////////////////////

/***
 * The exponential moving average (EMS) filter is a simple and
 * comutationally inexpensive low pass filter.
 *
 * small values of alpha give low cut-off frequencies and long
 * time constants.
 *
 * For a signal sampled at a frequency F, and filtered with a
 * filter constant alpha, the time constant is
 *
 *  tau = 1/(F*alpha)
 *
 * In this example, the slow filter has
 *
 *     F = 1300
 * alpha = 0.001
 *   tau = 1/1.3 = 0.77 seconds.
 *
 * For a step change, the filter will take 3*tau seconds to get to
 * 95% of the new output value. Here that equates to the slow filter
 * being ready in about 2.5 seconds
 *
 *
 */
class ExpFilter {
 public:
  ExpFilter(){};

  explicit ExpFilter(float alpha, float value = 0.0f) { begin(alpha, value); };

  void begin(float alpha, float value = 0) {
    mAlpha = alpha;
    mValue = value;
  }

  float update(float newValue) {
    mValue += mAlpha * (newValue - mValue);
    return mValue;
  };

  void set_alpha(float alpha) { mAlpha = alpha; }

  float value() { return mValue; }

  void set_value(float value) { mValue = value; }

  float operator()() { return mValue; }

 private:
  volatile float mAlpha = 1.0f;
  volatile float mValue = 0;
};

////////////////////////////////////////////////////////////////////////
class GateSensor {
  const uint32_t LOCKOUT_PERIOD = 50;

 public:
  explicit GateSensor(int pin) : mSensorPin(pin) {
    slow.begin(0.000769);                  // tau = 1.000 seconds
    fast.begin((1.0 / (1300.0 * 0.002)));  // tau = 0.002 seconds
  }

  void update() {
    mInput = analogRead(mSensorPin);
    if (not mInterrupted) {
      slow.update(mInput);
    }
    fast.update(mInput);
    if (slow.value() < 10) {
      return;
    }
    mDiff = slow.value() - fast.value();
    if (fast.value() < 0.25 * slow.value()) {
      mInterrupted = true;
    }
    if (fast.value() > 0.75 * slow.value()) {
      arm();
      mInterrupted = false;
      mMessageSent = false;
    }
  }

  void arm() { mArmed = true; }

  void disarm() { mArmed = false; }

  bool armed() { return mArmed; }

  int mSensorPin;
  volatile float mInput;
  volatile float mDiff;
  ExpFilter slow;
  ExpFilter fast;
  volatile bool mInterrupted = false;
  volatile bool mMessageSent = false;
  volatile bool mArmed = true;
};

////////////////////////////////////////////////////////////////////////

SoftwareSerial radio(RADIO_NC, RADIO_DATA);  // RX, TX

const uint32_t LOCKOUT_TIME = 200;
const uint8_t HOME_BASE = 0x41;
const uint8_t START_BASE = 0x61;
const uint8_t GOAL_BASE = 0x30;
// assume this is the start gate detector
uint8_t side_sensor_base = HOME_BASE;
uint8_t end_sensor_base = START_BASE;

GateSensor endSensor(endSensorPin);
GateSensor sideSensor(sideSensorPin);

void analogueInit() {
  // increase speed of ADC conversions
  // by changing the clock prescaler from 128 to 32
  bitSet(ADCSRA, ADPS0);
  bitClear(ADCSRA, ADPS1);
  bitSet(ADCSRA, ADPS2);
}

// the systick event is an ISR attached to Timer 2
void systickInit() {
  // set the mode for timer 2
  bitClear(TCCR2A, WGM20);
  bitSet(TCCR2A, WGM21);
  bitClear(TCCR2B, WGM22);
  // set divisor to 128 => timer clock = 125kHz
  bitSet(TCCR2B, CS22);
  bitClear(TCCR2B, CS21);
  bitSet(TCCR2B, CS20);
  // set the timer frequency for 1300Hz
  OCR2A = (F_CPU / 128 / 1300) - 1;  // 47
  bitSet(TIMSK2, OCIE2A);            // enable the timer interrupt
}

ISR(TIMER2_COMPA_vect) {
  endSensor.update();
  if (gateID == 0) {
    sideSensor.update();
  }
}

/***
 * In an attempt to get reliable timing form the software serial
 * strings are sent by calling directly the write method.
 *
 * At a baud rate of 5000 Baud, each character should take 2ms
 * to go out. Placing a 1ms delay between characters will let
 * the receiver calculate, from the character received, the time
 * that the first character was sent since they should be arriving
 * every 3ms.
 *
 * Software serial turns off interrupts while it is sending a byte
 * in order to get good timing.
 *
 * Here, interrupts are turned off for the duration of the entire
 * string to try and get good inter-character timing as well.
 *
 * Even so, there will be some jitter. Observations indicate that
 * this amounts to less than 250us variation between characters.
 *
 * Although a longer interval might make the system more resistant
 * to interference, it seems a good idea to get the entire packet
 * out in a short space of time. Packets contain 13 characters and
 * so should take up about 40ms transmission time.
 *
 * If messages are missed it might be a good idea to implement some
 * kind of repeat although that would considerably complicate the
 * implied timing scheme used here.
 *
 */
void sendString(char* s) {
  uint8_t oldSREG = SREG;
  cli();
  while (char c = *s++) {
    radio.write(c);
    delayMicroseconds(1000);  // actual transmission is 11 bits?
  }
  SREG = oldSREG;
}

/***
 * The trigger message consists of a synchronising byte
 * followed by 6 data bytes and finished with a pair of
 * terminating bytes.
 *
 * The synchronising byte is used only to help the
 * receiver wake up. The receiver ignores that byte and
 * so it represents a fixed delay in the response.
 *
 */

uint32_t last_trigger_time = millis();
void send_trigger(uint8_t base) {
  digitalWriteFast(LED_BUILTIN, 1);
  digitalWrite(RADIO_DATA, 1);
  digitalWrite(RADIO_PDN, 1);
  digitalWrite(RADIO_TX, 1);
  // allow the transmitter to stabilise
  delayMicroseconds(500);
  switch (base) {
    case GOAL_BASE:
      sendString("U0123456789##");
      break;
    case HOME_BASE:
      sendString("UABCDEFGHIJ##");
      break;
    case START_BASE:
      sendString("Uabcdefghij##");
      break;
  }
  digitalWrite(RADIO_TX, 0);
  digitalWrite(RADIO_PDN, 0);
  digitalWriteFast(LED_BUILTIN, 0);
  uint32_t now = millis();
  uint32_t elapsed = millis() - last_trigger_time;
  last_trigger_time = now;
  Serial.println(elapsed);
}

uint32_t next_update_time = millis();
uint32_t debug_update_interval = 50;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RADIO_PDN, OUTPUT);
  pinMode(RADIO_TX, OUTPUT);
  pinMode(RADIO_DATA, OUTPUT);
  pinMode(GATE_ID_PIN0, INPUT_PULLUP);
  pinMode(GATE_ID_PIN1, INPUT_PULLUP);
  pinMode(GATE_ID_PIN2, INPUT_PULLUP);
  pinMode(GATE_ID_PIN3, INPUT_PULLUP);
  digitalWrite(RADIO_PDN, 0);
  digitalWrite(RADIO_TX, 0);
  digitalWrite(RADIO_PDN, 0);
  digitalWrite(RADIO_TX, 1);
  digitalWrite(RADIO_PDN, 1);
  Serial.begin(115200);
  radio.begin(5000);
  gateID = digitalRead(GATE_ID_PIN0) << 3;
  gateID += digitalRead(+GATE_ID_PIN1) << 2;
  gateID += digitalRead(+GATE_ID_PIN2) << 1;
  gateID += digitalRead(GATE_ID_PIN3);
  analogueInit();
  systickInit();
  delay(20);
  Serial.print(F("GATE ID: "));
  Serial.println(gateID);
  while (endSensor.mDiff < -0.25 or sideSensor.mDiff < -0.25) {
    digitalWrite(LED_BUILTIN, 1);
    delay(100);
    digitalWrite(LED_BUILTIN, 0);
    delay(100);
  }
  if (gateID == 0) {  // the start and home gates
    side_sensor_base = HOME_BASE;
    end_sensor_base = START_BASE;
  } else {
    side_sensor_base = GOAL_BASE;  // not used here
    end_sensor_base = GOAL_BASE;
  }
}

void loop() {
  if (DEBUG) {
    if (millis() - next_update_time > debug_update_interval) {
      next_update_time += debug_update_interval;
      // Serial.print(endSensor.slow.value(), 1);
      Serial.print(endSensor.slow.value(), 1);
      Serial.print('\t');
      Serial.print(endSensor.fast.value(), 1);
      if (gateID == 0) {
        Serial.print('\t');
        Serial.print(sideSensor.slow.value(), 1);
        Serial.print('\t');
        Serial.print(sideSensor.fast.value(), 1);
      }
      Serial.println();
    }
  }
  if (endSensor.armed() && endSensor.mInterrupted) {
    endSensor.disarm();
    if (not endSensor.mMessageSent) {
      send_trigger(end_sensor_base);
      endSensor.mMessageSent = true;
    }

  } else if (sideSensor.armed() && sideSensor.mInterrupted) {
    sideSensor.disarm();
    if (not sideSensor.mMessageSent) {
      send_trigger(side_sensor_base);
      sideSensor.mMessageSent = true;
    }
  }
}