/*
  Software serial multple serial test

 Receives from the hardware serial, sends to software serial.
 Receives from software serial, sends to hardware serial.

 The circuit:
 * RX is digital pin 10 (connect to TX of other device)
 * TX is digital pin 11 (connect to RX of other device)

 */
#include <Arduino.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(10, 11);  // RX, TX

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  // set the data rate for the SoftwareSerial port
  mySerial.begin(9600);
}

uint32_t updateInterval = 10;
uint32_t updateTime;
int state = 0;
void loop() {  // run over and over
  mySerial.println("UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU"); // effectively a 9600 Hz squre wave
  if (millis() > updateTime) {
    updateTime += updateInterval;
    state = 1 - state;
    digitalWrite(LED_BUILTIN, state);
  }
  if (mySerial.available()) {
    Serial.write(mySerial.read());
  }
  if (Serial.available()) {
    mySerial.write(Serial.read());
  }
}
