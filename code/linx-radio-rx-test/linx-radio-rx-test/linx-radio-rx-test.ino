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
#include <ctype.h>

const int RADIO_DATA = 7;
const int RADIO_NC = 8;  // an unconnected arduio pin

SoftwareSerial radio(RADIO_DATA, RADIO_NC);  // RX, TX

/***
 * On the receiver, the radio is powered and listening at
 * all times becaus ethe appropriate pins are set to the
 * correct states in hardware. The code need only listen
 * on the data pin for activity.
 *
 * Optionally, the receiver could monitor the data
 * pin to tell if there is any transmission but this seems
 * not tobe necessary and may cause issues if there is
 * local interference.
 */
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RADIO_DATA, INPUT);
  radio.begin(9600);
}

// also used for gate ID
enum ReaderState { RD_NONE, RD_WAIT, RD_HOME, RD_START, RD_GOAL, RD_TERM, RD_DONE, RD_ERROR };

/*** TODO
 * it may be necessary for the decoder to see two gate
 * characters for confirmation.
 */
ReaderState reader_state = RD_WAIT;
uint32_t rx_time;
int gate_id = 0;
void reader(char c) {
  uint32_t time = millis();
  switch (reader_state) {
    case RD_WAIT:
      gate_id = RD_NONE;
      if (c >= 0x30 and c <= 0x37) {
        reader_state = RD_GOAL;
        rx_time = time - 2 * (c - 0x30);
      } else if (c >= 0x40 and c <= 0x47) {
        reader_state = RD_HOME;
        rx_time = time - 2 * (c - 0x40);
      } else if (c >= 0x60 and c <= 0x67) {
        reader_state = RD_START;
        rx_time = time - 2 * (c - 0x60);
      }
      break;
    case RD_GOAL:
      if (c == '#') {
        reader_state = RD_DONE;
        gate_id = RD_GOAL;
      }
      break;
    case RD_START:
      if (c == '#') {
        reader_state = RD_DONE;
        gate_id = RD_START;
      }
      break;
    case RD_HOME:
      if (c == '#') {
        reader_state = RD_DONE;
        gate_id = RD_HOME;
      }
      break;
    case RD_DONE:
      reader_state = RD_WAIT;
      break;
    default:
      reader_state = RD_ERROR;
      gate_id = RD_NONE;
      break;
  }
}

void loop() {
  if (radio.available()) {
    char c = radio.read();
    reader(c);
    switch (gate_id) {
      case RD_HOME:
        Serial.print("HOME   ");
        Serial.print(rx_time);
        Serial.println();
        reader_state = RD_WAIT;
        break;
      case RD_START:
        Serial.print("START  ");
        Serial.print(rx_time);
        Serial.println();
        reader_state = RD_WAIT;
        break;
      case RD_GOAL:
        Serial.print("GOAL   ");
        Serial.print(rx_time);
        Serial.println();
        reader_state = RD_WAIT;
        break;
      case RD_ERROR:
        Serial.println("BAD PACKET");
        reader_state = RD_WAIT;
        break;
    }
  }
  return;
}
