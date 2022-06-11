
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
#include "digitalWriteFast.h"
#define LINX_PDN_PIN 2
#define LINX_TX_PIN 3
#define LINX_DATA_PIN 4

#define GATE_ID_PIN0 9
#define GATE_ID_PIN1 8
#define GATE_ID_PIN2 7
#define GATE_ID_PIN3 6
int gateID = 0;
/***
 * Note that the hardware serial is not suitable for sending over the radio
 * without some extra work because transmission happens in the background
 * and you don't know when to turn off the radio
 */
SoftwareSerial radio(5, 4);  // RX, TX

void u_loop() {  // run over and over
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
  // set the data rate for the SoftwareSerial port
  radio.begin(10000);
}

/***
 *
 * The transmission sequence begins by setting the radio data pin high so that carrier is
 * transmitted immediately after the transmitter is woken up.
 *
 * To wake up the radio, it must first be powered up and then switched into transmit mode.
 *
 *
 * A short delay is needed for the transmitter to stabilise. According to the datasheet for
 * the TRM-433-LT module, the carrier should be ready within 500 microseconds.
 *
 * Delaying too long after enabling the transmitter can cause receive errors.
 *
 * The receiver detector appears to require regular transitions for reliability. This is
 * mentioned in the Linx Application Note AN-00160
 *
 * https://linxtechnologies.com/wp/wp-content/uploads/an-00160.pdfote
 *
 * To help the reciver set its levels, transmission starts with the synchronising character '*'.
 *
 * The ASCII value for '*' is 0b00101010 so that there are several transitions to allow the
 * receiver to get itself sorted out.
 *
 * An alternative sync character might be 'U' (0b01010101) but the '*' seems marginally
 * more effective.
 *
 * Data transmission should begin immediately. For reliable transmission, there should be no
 * delays between successive characters.
 *
 * The baud rate of 10000 causes successive character to be sent at accurate 1ms intervals
 * because of the 8N1 default serial protocol using 10 bits per character.
 *
 * As soon as the gate is 'broken' - the beam is interrupted - a string is transmitted. The
 * string consists of the synchronising character followed by a sequence of characters that
 * provides both timing and identification. Identification is given by the characters used.
 *
 * The message string contains several items of information and has the general format:
 *
 *   "*GNSSSBBC#"
 *
 * where
 *
 *  - 'G'    is an ADCII character representing the gate ID
 *           Each gate has an ID in the range 0-15, set with jumpers on pins D6-D9. The ID is read
 *           at system startup and transmitted as an ASCII character in the range 'A' - 'P'
 *           Generally, the gate detector circuit handles two detection circuits although
 *           only one is used except in the start square.
 *           If the second detector is the source of the message, the gate identifier
 *           will be the lower case letter 'a' - 'p'.
 *
 *  - 'N'    is an ASCI digit in the range '0' - '9' representing the sequence number. As
 *           soon as a gate is broken, the first packet is sent. After that, nine more
 *           packets are sent with the same information but incrementing sequence numbers.
 *           The packets are sent at accurate 50ms intervals so that the receiver can
 *           examine a message packet and determine the time at which the gate was actually
 *           broken. The receiver may act upon the first valid packet and ignore subsequent ones
 *           or it may choose to combine packets for reliability.
 *           Sustained interference lasting more than half a second will cause the event to
 *           be missed.
 *           The receiver may register the next event in several ways
 *             - employ a lockout delay so that no packets will be registered for some period
 *             - only repond to another packet if the sequence number is less or equal to the last one
 *             - ignore subsequent packets from the same gate ID.
 *
 *  - 'SSS' is three digits repreenting the numerical value of the gate sensor steady state reading.
 *          This can be used to identify faulty or unreliable gates or potential interference from
 *          ambient illumination.
 *
 *  - 'BB'  is the battery voltage in tenths of a volt.
 *          This will allow the controller to identify gates that are experiencing power loss.
 *
 *  - 'C'   is a single character check digit as a simple means of error detection. Each of the
 *          preceding characters in the packet is XORed into a byte. That byte is then
 *          reduced to a range of 0-15 and used to generate a character in the range 'a' to 'o'.
 *          Not the most reliable check digit in the world but better than nothing.
 *
 *  - '#'   is a terminating character used as a visual and coding aid when unpacking a
 *          packet.
 *
 *
 * There can be several goal gates in a system since they will not transmit simultaneously
 * and are all equivalent in terms of the timing function in a contest.
 *
 * All gates should have a unique identifier. Currently that limits the number of gates to 16.
 *
 * If multiple contests use the same gate and controller system, there is a small chance that
 * two gates will transmit simultaneously. The probability is low and could be essentially
 * eliminated if the message packets were sent at pseudo random intervals or if a gate
 * uses shared propogation tachniques like those used in ethernet.
 *
 * Once the '*' has been received, the controller records the time and then checks the following
 * character(s) to find out which gate has sent the message. Any of the subsequent characters will
 * serve as positive identifiers.
 *
 */

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

uint32_t updateInterval = 1000;
uint32_t updateTime;
int state = 0;


void sendString(char* s) {
  digitalWrite(LINX_DATA_PIN, 1);
  digitalWrite(LINX_PDN_PIN, 1);
  digitalWrite(LINX_TX_PIN, 1);
  // allow the transmitter to stabilise
  delayMicroseconds(500);
  while (char c = *s++) {
    radio.write(c);
  }
  digitalWrite(LINX_TX_PIN, 0);
  digitalWrite(LINX_PDN_PIN, 0);
  digitalWrite(LED_BUILTIN, state);
  delay(20);  // ensure that there i a gap between transmissions
}
void sendBreak() {
  char s[] = {'*', 'G', 'N', 'S', 'S', 'S', 'V', 'V', 'C', '#', ' ', ' ', '\r', '\n', '\0'};
  uint32_t triggerTime = millis();
  ;
  int sensorA = analogRead(A0);
  int sensorB = analogRead(A1);
  int battery = readVcc() / 100;

  for (int i = 0; i < 10; i++) {
    while (millis() <= triggerTime) {
      // do nothing;
    }
    char check = 0;
    triggerTime += 50;
    s[1] = 'A' + gateID;
    s[2] = '0' + i;
    int sensor = sensorA;
    s[5] = sensor % 10 + '0';
    sensor /= 10;
    s[4] = sensor % 10 + '0';
    sensor /= 10;
    s[3] = sensor % 10 + '0';
    int batt = battery;
    s[7] = batt % 10 + '0';
    batt /= 10;
    s[6] = batt % 10 + '0';
    for (int i = 0; i < 8; i++) {
      check ^= s[i];
    }
    s[8] = check % 16 + 'a';
    sendString(s);
  }
  Serial.println(sensorA);
}

void loop() {  // run over and over
  if (millis() >= updateTime) {
    state = 1 - state;
    sendBreak();
    updateTime += updateInterval;
  }
  if (radio.available()) {
    // Serial.write(radio.read());
  }
}
