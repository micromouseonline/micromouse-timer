#include <Arduino.h>
#include "SoftwareSerial.h"
#include "digitalWriteFast.h"

#define DEBUG 1
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

  float operator()() { return mValue; }

 private:
  float mAlpha = 1.0f;
  float mValue = 0;
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
    mDiff = slow.value() - fast.value();
    if (fast.value() < 0.25 * slow.value()) {
      mInterrupted = true;
    }
    if (fast.value() > 0.75 * slow.value()) {
      mInterrupted = false;
      mMessageSent = false;
    }
  }

  int mSensorPin;
  float mInput;
  float mDiff;
  ExpFilter slow;
  ExpFilter fast;
  bool mInterrupted = false;
  bool mMessageSent = false;
};

////////////////////////////////////////////////////////////////////////

SoftwareSerial radio(RADIO_NC, RADIO_DATA);  // RX, TX

const uint32_t LOCKOUT_TIME = 200;
const uint8_t HOME_BASE = 0x40;
const uint8_t START_BASE = 0x60;
const uint8_t GOAL_BASE = 0x30;
// assume this is the start gate detector
uint8_t side_sensor_base = HOME_BASE;
uint8_t end_sensor_base = START_BASE;

GateSensor endSensor(endSensorPin);
GateSensor sideSensor(sideSensorPin);

void sendString(char* s) {
  // digitalWrite(LED_BUILTIN,1);
  digitalWrite(RADIO_DATA, 1);
  digitalWrite(RADIO_PDN, 1);
  digitalWrite(RADIO_TX, 1);
  // allow the transmitter to stabilise
  delayMicroseconds(500);
  while (char c = *s++) {
    radio.write(c);
  }
  digitalWrite(RADIO_TX, 0);
  digitalWrite(RADIO_PDN, 0);
}

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
  sideSensor.update();
}

/***
 * Send a single byte and ensure the appropriate time
 * passes before return. micros() overflows every 71 minutes.
 * Individual bytes are paced so that each takes the same
 * amount of time to send. In this way, the receiver can
 * deduce the time relative to the packet start simply by
 * examining a single valid byte in the received message.
 * The default pacing is one character every 3ms. With a
 * baud rate of 9600, a single byte sould normally take
 * 1.04ms to send. Higher baud rates are likely to be
 * less reliable over longer distances.
 */
void send_byte(uint8_t c, uint32_t time = 2000L) {
  uint32_t start = micros();
  radio.write(c);
  while (micros() - start < time) {
    // do nothing
  }
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
 * A fixed delay does not affect the timing repeatability.
 *
 * The complete message uses 1+6+2 = 9 bytes and so,
 * with the default pacing and baud rate will take
 * just 18ms to transmit
 *
 */
void send_trigger(uint8_t base) {
  digitalWriteFast(LED_BUILTIN, 1);
  digitalWrite(RADIO_DATA, 1);
  digitalWrite(RADIO_PDN, 1);
  digitalWrite(RADIO_TX, 1);
  // allow the transmitter to stabilise
  delayMicroseconds(500);
  // Send the receiver a few transitions to
  // get itself in sync. Finish with carrier on
  // because the serial transmission will begin with a
  // zero start bit.
  send_byte(0b10101011);
  for (uint8_t i = 0; i < 6; i++) {
    send_byte(base + i);
  }
  send_byte('#');
  send_byte('#');
  // do we need to wait a little while before shutting down?
  digitalWrite(RADIO_TX, 0);
  digitalWrite(RADIO_PDN, 0);
  digitalWriteFast(LED_BUILTIN, 0);
}

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
  radio.begin(9600);
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

uint32_t next_update_time = millis();
uint32_t update_interval = 50;

void loop() {
  if (DEBUG) {
    if (millis() - next_update_time > update_interval) {
      next_update_time += update_interval;
      // Serial.print(endSensor.slow.value(), 1);
      Serial.print(endSensor.slow.value(), 1);
      Serial.print('\t');
      Serial.print(endSensor.fast.value(), 1);
      Serial.print('\t');
      Serial.print(endSensor.mInterrupted, 1);
      Serial.print('\t');
      Serial.print(endSensor.mMessageSent, 1);
      Serial.print('\t');
      Serial.print(sideSensor.slow.value(), 1);
      Serial.print('\t');
      Serial.print(sideSensor.fast.value(), 1);
      Serial.print('\t');
      Serial.print(sideSensor.mInterrupted, 1);
      Serial.print('\t');
      Serial.print(sideSensor.mMessageSent, 1);
      Serial.print('\t');
      Serial.println();
    }
  }
  if (endSensor.mInterrupted) {
    if (not endSensor.mMessageSent) {
      send_trigger(end_sensor_base);
      endSensor.mMessageSent = true;
    }
  } else if (sideSensor.mInterrupted) {
    if (not sideSensor.mMessageSent) {
      send_trigger(side_sensor_base);
      sideSensor.mMessageSent = true;
    }
  }
}