#include <Arduino.h>
#include "SoftwareSerial.h"
#include "digitalWriteFast.h"
#include "filters.h"
#include "gatesensor.h"
#include "monostable.h"

#define DEBUG 1
const int sideSensorPin = A0;
const int endSensorPin = A1;
SoftwareSerial radio(4, 5);  // RX, TX

const uint32_t LOCKOUT_TIME = 200;
int gateID = 0;

GateSensor endSensor(endSensorPin);
GateSensor sideSensor(sideSensorPin);
MonoStable triggered(LOCKOUT_TIME);

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
  // set the timer frequency for 1700Hz
  OCR2A = (F_CPU / 128 / 1300) - 1;  // 72
  // enable the timer interrupt
  bitSet(TIMSK2, OCIE2A);
}

ISR(TIMER2_COMPA_vect) {
  sideSensor.update();
  endSensor.update();
  triggered.update();
}

void setup() {
  Serial.begin(57600);
  radio.begin(9600);
  analogueInit();
  endSensor.setThreshold(16.0f);
  sideSensor.setThreshold(16.0f);
  systickInit();
  delay(200);
}

void loop() {
  if (DEBUG) {
    Serial.print(endSensor.input(), 4);
    Serial.print('\t');
    Serial.print(endSensor.mDiff, 4);
    Serial.print('\t');
    Serial.print(sideSensor.input(), 4);
    Serial.print('\t');
    Serial.print(sideSensor.mDiff, 4);
    Serial.print('\t');
    Serial.print(triggered.state() ? 3 : 0);
    Serial.print('\t');
    Serial.print(100);
    Serial.print('\t');
    Serial.print(-100);
    Serial.println();
  }
  if (sideSensor.interrupted() || endSensor.interrupted()) {
    sideSensor.reset();
    endSensor.reset();
    triggered.start();
  }
  digitalWriteFast(LED_BUILTIN, triggered.state());
}