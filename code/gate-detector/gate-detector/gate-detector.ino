#include <Arduino.h>
#include "digitalWriteFast.h"
#include "SoftwareSerial.h"

/**
 * Hardware pin defines
 */


const int BATTERY_VOLTS = A7;
/****/

SoftwareSerial radio(4,5);  // RX, TX
const int sensorPin = A0;
uint32_t updateTime = 0;
uint32_t updateInterval = 1000;
int state = 0;

void analogueSetup() {
  // increase speed of ADC conversions
  // by changing the clock prescaler from 128 to 32
  bitSet(ADCSRA, ADPS0);
  bitClear(ADCSRA, ADPS1);
  bitSet(ADCSRA, ADPS2);
}

class ExpFilter {
 public:
  ExpFilter(){};

  ExpFilter(float alpha, float value = 0.0f) { begin(alpha, value); };

  void begin(float alpha, float value) {
    mAlpha = alpha;
    mValue = value;
  }

  float update(float newValue) {
    mValue += mAlpha * (newValue - mValue);
    return mValue;
  };

  float value() { return mValue; }

 private:
  float mAlpha = 1.0f;
  float mValue = 0;
};

class MonoStable {
 public:
  explicit MonoStable(int timeout) { duration = timeout; }

  void start() {
    if (time <= -duration) {
      time = duration;
    }
  }

  void setTime(int timeout) { duration = timeout; }

  void update() {
    if (time > -duration) {
      time--;
    }
  }

  bool state() { return time > 0; }

 private:
  MonoStable(){};
  int duration = 0;
  int time = 0;
};

ExpFilter longTerm(0.0002);
ExpFilter slow(0.02);
ExpFilter fast(0.20);
ExpFilter ambient(0.1);
MonoStable triggered(250);

void setupSystick() {
  // set the mode for timer 2
  bitClear(TCCR2A, WGM20);
  bitSet(TCCR2A, WGM21);
  bitClear(TCCR2B, WGM22);
  // set divisor to 128 => timer clock = 125kHz
  bitSet(TCCR2B, CS22);
  bitClear(TCCR2B, CS21);
  bitSet(TCCR2B, CS20);
  // set the timer frequency for 1700Hz
  OCR2A = (F_CPU / 128 / 1700) - 1; // 72

  // enable the timer interrupt
  bitSet(TIMSK2, OCIE2A);
}


float diff;
float maxDiff;
float minDiff;
void updateWallSensor() {
  float sensor = analogRead(sensorPin);
  ambient.update(sensor);
  longTerm.update(sensor);
  float slowV = 1.0 * slow.update(sensor);
  float fastV = 1.0 * fast.update(sensor);
  diff = -(fastV - longTerm.value());
  maxDiff = max(diff, maxDiff);
  minDiff = min(diff, minDiff);
  if (diff > 7.0) {
    triggered.start();
  }
  triggered.update();
  digitalWriteFast(LED_BUILTIN, triggered.state())
}

// the systick event is an ISR attached to Timer 2
ISR(TIMER2_COMPA_vect) {
  updateWallSensor();
}


void setup() {
  Serial.begin(115200);
  radio.begin(9600);
  analogueSetup();
  setupSystick();

}

void loop() {
  if (millis() > updateTime) {
    updateTime += updateInterval;
    Serial.print(ambient.value(),4);
    Serial.print('\t');
    Serial.print(slow.value(),4);
    Serial.print('\t');
    Serial.print(fast.value(),4);
    Serial.print('\t');

    Serial.print(diff,4);
    Serial.print('\t');
    Serial.print(minDiff,4);
    Serial.print('\t');
    Serial.print(maxDiff,4);
    Serial.print('\t');
    Serial.print(longTerm.value(),4);
    Serial.print('\t');

    Serial.println();
    radio.print('XXX');
    minDiff = 0;
    maxDiff = 0;

  }
}