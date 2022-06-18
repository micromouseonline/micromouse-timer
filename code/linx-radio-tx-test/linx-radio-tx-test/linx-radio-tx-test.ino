
/***
 * Code for the gate detector to to report signal levels from the
 * two sensors.
 *
 * A0 has the mid-cell sensor
 * A1 has the end-cell sensor
 *
 */
#include <Arduino.h>
#include <SoftwareSerial.h>
#include "digitalWriteFast.h"
#define LINX_PDN_PIN 2
#define LINX_TX_PIN 3
#define LINX_DATA_PIN 4

#define GATE_ID_PIN0 9
#define GATE_ID_PIN1 8
#define GATE_ID_PIN2 7
#define GATE_ID_PIN3 6
int gateID = 0;

uint32_t updateInterval = 200;
uint32_t updateTime;
int state = 0;

/***
 * Note that the hardware serial is not suitable for sending over the radio
 * without some extra work because transmission happens in the background
 * and you don't know when to turn off the radio.
 *
 * In any case, the hardware serial on a typical Arduino is used for
 * programming.
 */
SoftwareSerial radio(5, 4);  // RX, TX

void setup() {
  pinMode(GATE_ID_PIN0, INPUT_PULLUP);
  pinMode(GATE_ID_PIN1, INPUT_PULLUP);
  pinMode(GATE_ID_PIN2, INPUT_PULLUP);
  pinMode(GATE_ID_PIN3, INPUT_PULLUP);
  gateID = digitalRead(GATE_ID_PIN3) << 3;
  gateID += digitalRead(+GATE_ID_PIN2) << 2;
  gateID += digitalRead(+GATE_ID_PIN1) << 1;
  gateID += digitalRead(GATE_ID_PIN0);
  pinMode(LINX_TX_PIN, OUTPUT);
  pinMode(LINX_PDN_PIN, OUTPUT);
  pinMode(LINX_DATA_PIN, OUTPUT);
  digitalWrite(LINX_TX_PIN, 0);
  digitalWrite(LINX_PDN_PIN, 0);
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println(gateID);
  // set the data rate for the SoftwareSerial port
  radio.begin(9600);
}

long readVcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);             // Wait for Vref to settle
  ADCSRA |= _BV(ADSC);  // Convert
  while (bit_is_set(ADCSRA, ADSC))
    ;
  result = ADCL;
  result |= ADCH << 8;
  result = 1104000L / result;  // Back-calculate AVcc in mV
  return result;
}

/***
 * Software Serial has no transmit buffering
 * When a call to write() returns, the data has been sent
 */
void sendString(char* s) {
  digitalWrite(LINX_DATA_PIN, 1);
  digitalWrite(LINX_PDN_PIN, 1);
  digitalWrite(LINX_TX_PIN, 1);
  // allow the transmitter to stabilise
  delayMicroseconds(500);
  while (char c = *s++) {
    radio.write(c);
  }
  // do we need to wait a little while before shutting down?
  digitalWrite(LINX_TX_PIN, 0);
  digitalWrite(LINX_PDN_PIN, 0);
}

char tx_buf[32];
char c = 'a';
void loop() {  // run over and over
  if ((millis() - updateTime) >= updateInterval) {
    digitalWrite(LED_BUILTIN, 1);
    updateTime += updateInterval;
    sprintf(tx_buf, "%4d  %4d\n", analogRead(A0), analogRead(A1));
    Serial.print(tx_buf);
    radio.print(tx_buf);
    digitalWrite(LED_BUILTIN, 0);
  }
}
