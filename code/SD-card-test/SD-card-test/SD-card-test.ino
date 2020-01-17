/***
 * Micromouse Timer
 * Gate Controller
 * (c) Peter Harrison 2020
 * Libraries:
 * https://bitbucket.org/fmalpartida/new-liquidcrystal/wiki/Home
 * https://platformio.org/lib/show/136/LiquidCrystal
 *
 * https://github.com/NeiroNx/RTCLib
 * https://platformio.org/lib/show/1197/RTCLib
 *
 *
 *
 */
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <SoftwareSerial.h>
/// Pin Assignments
const int LED_STATUS = 2;
const int LED_2 = 2;
const int LED_3 = 3;
const int LED_4 = 4;
const int DISP_SELECT = 5;

const int RADIO_RX = 7;
const int RADIO_TX = 8;
const int SD_DETECT = 9;
const int SD_SELECT = 10;
const int SD_MOSI = 11;
const int SD_MISO = 12;
const int SD_SCK = 13;

const int BUTTON_RESET_BACK = A0;
const int BUTTON_TOUCH_OK = A1;
const int BUTTON_GOAL_DN = A2;
const int BUTTON_START_UP = A3;

const int I2C_SDA = A4;
const int I2C_SCL = A5;

///////////////////////////////////////////////////////////////////

/***
 * The radio is used only in receive mode here but the SoftwareSerial requires
 * two pins for the configuration. TX is not used
 */
SoftwareSerial radio(7, 8);  // RX, TX

const uint32_t ONE_SECOND = 1000L;
const uint32_t ONE_MINUTE = 60 * ONE_SECOND;
const uint32_t ONE_HOUR = 60 * ONE_MINUTE;

uint32_t displayUpdateInterval = 1000;
uint32_t displayUpdateTime;
int state = 0;
uint32_t animUpdateTime = 0;
int frameCounter;
uint32_t systime;
uint32_t startTime;

/***************************************************   SD Card */

const int chipSelect = 10;
const int cardDetect = 9;

void sdCardInit() {
  pinMode(chipSelect, OUTPUT);
  pinMode(cardDetect, INPUT_PULLUP);
  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
 if (!SD.begin(chipSelect)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println(F("Card present"));
}

void cardInfo() {
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File myFile = SD.open("TEST.TXT", FILE_WRITE);

  // if the file opened okay, write to it:
  if (myFile) {
    Serial.print(F("Writing to test.txt..."));
    myFile.println(F("testing 1, 2, 3, 4."));
    // close the file:
    myFile.close();
    Serial.println(F("done."));
  } else {
    // if the file didn't open, print an error:
    Serial.println(F("error opening test.txt"));
  }

  // re-open the file for reading:
  myFile = SD.open("TEST.TXT");
  if (myFile) {
    Serial.println("test.txt:");

    // read from the file until there's nothing else in it:
    while (myFile.available()) {
      Serial.write(myFile.read());
    }
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt");
  }

  // Serial.println(F("\nFiles found on the card (name, date and size in bytes): "));
  // root.openRoot(volume);
  myFile = SD.open("/");
  printDirectory(myFile, 0);
  myFile.close();
}

void printDirectory(File dir, int numTabs) {
  // Begin at the start of the directory
  dir.rewindDirectory();
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      // no more files
      Serial.println(F("**nomorefiles**"));
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');  // we'll have a nice indentation
    }
    // Print the 8.3 name
    Serial.print(entry.name());
    // Recurse for directories, otherwise print the file size
    if (entry.isDirectory()) {
      Serial.println('/');
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}
/***************************************************   SD Card End */


/***************************************************   7-SEGMENT LED START */

#include <SPI.h>
const static byte charTable[] PROGMEM = {
    B01111110,  // 0
    B00110000,  // 1
    B01101101,  // 2
    B01111001,  // 3
    B00110011,  // 4
    B01011011,  // 5
    B01011111,  // 6
    B01110000,  // 7
    B01111111,  // 8
    B01111011,  // 9

    B01110111,  // A
    B00011111,  // B
    B01001110,  // C
    B00111101,  // D
    B01001111,  // E
    B01000111,  // F
    B00000001,  // -

    B01111101,  // a
    B00011111,  // b
    B00001101,  // c
    B00111101,  // d
    B01101111,  // e
    B01000111,  // F
    B01011110,  // G
    B00010111,  // h
    B00000110,  // l
    B00111100,  // J
    B00001110,  // L
    B00010101,  // n
    B00011101,  // o
    B01100111,  // P
    B01100110,  // r
    B00001111,  // t
    B00111110,  // U
    B00110111,  // X
    B00111011,  // Y

    B01000000,  // spinner
    B00100000,  // spinner
    B00010000,  // spinner
    B00001000,  // spinner
    B00000100,  // spinner
    B00000010,  // spinner

};


class LedDisplay {
 private:
  const int REG_DECODEMODE = 9;
  const int REG_BRIGHTNESS = 10;
  const int REG_SCANLIMIT = 11;
  const int REG_SHUTDOWN = 12;
  const int REG_DISPLAYTEST = 15;
  uint8_t mCS = NOT_A_PIN;
  uint8_t mLastDigit = 7;
  void writeRegister(uint8_t reg, uint8_t val) {
    if (!mCS) {
      return;
    }
    digitalWrite(mCS, LOW);  // digit 0
    SPI.transfer(reg);
    SPI.transfer(val);
    digitalWrite(mCS, HIGH);
  }

 public:
  LedDisplay(int cs_pin = NOT_A_PIN) : mCS(cs_pin) {}

  void begin() {
    SPI.setClockDivider(SPI_CLOCK_DIV2);
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    digitalWrite(mCS, HIGH);
    pinMode(mCS, OUTPUT);
    SPI.begin();
    wakeUp();
    setDecode(0x00);
    setDigitLimit(7);
    for (int i = 1; i < 9; i++) {  // set all digits OFF
      writeRegister(i, 0x0A);
    }
  }
  void setBrightness(uint8_t bright) { writeRegister(REG_BRIGHTNESS, bright & 0x0F); }
  void setDigitLimit(uint8_t limit) {
    mLastDigit = limit;
    writeRegister(REG_SCANLIMIT, mLastDigit);
  }
  void wakeUp() { writeRegister(REG_SHUTDOWN, 1); }
  void shutDown() { writeRegister(REG_SHUTDOWN, 0); }
  void setDecode(uint8_t digits) { writeRegister(REG_DECODEMODE, digits); };
  void setDisplayTest(bool state) { writeRegister(REG_DISPLAYTEST, state ? 1 : 0); }
  void writeDigit(uint8_t digit, uint8_t value) {
    byte tableValue;
    tableValue = pgm_read_byte_near(charTable + value);
    writeRegister(digit + 1, tableValue);
  }

  void writeDigits(uint8_t digits[]) {
    for (int i = 0; i < 8; i++) {
      uint8_t data = pgm_read_byte_near(charTable + digits[i]);
      if ((i == 3) || (i == 5)) {
        data |= 0x80;  // DP on
      }
      writeRegister(i + 1, data);
    }
  }

  void clear() {
    for (int i = 1; i < 9; i++) {
      byte tableValue;
      tableValue = pgm_read_byte_near(charTable + 16);
      writeRegister(i, tableValue);
    }
  }
};
#define CSpin 5
LedDisplay ledDisplay(5);
uint8_t xxx = 0;
uint8_t yyy = 50;

void displayCounter() {
  byte digits[8];
  uint32_t elapsed = millis() - startTime;
  uint32_t seconds = (elapsed / ONE_SECOND) % 60;
  uint32_t minutes = (elapsed / ONE_MINUTE);
  elapsed %= 1000;
  for (int i = 0; i < 3; i++) {
    digits[i] = elapsed % 10;
    elapsed /= 10;
  }
  digits[3] = seconds % 10;
  digits[4] = (seconds / 10) % 10;
  digits[5] = minutes % 10;
  digits[6] = 0x10;
  xxx++;
  if (xxx >= sizeof(charTable)) {
    xxx = 0;
  }
  digits[7] = xxx;
  ledDisplay.writeDigits(digits);
}

void setupLED() {
  ledDisplay.begin();
  ledDisplay.setBrightness(1);
}

/*************************************************************** LED END*/



/******************************************************** SETUP *****/
void setup() {
  pinMode(LED_STATUS, OUTPUT);
  pinMode(LED_3, OUTPUT);
  pinMode(LED_4, OUTPUT);
  pinMode(DISP_SELECT, OUTPUT);

  pinMode(SD_DETECT, INPUT_PULLUP);
  pinMode(BUTTON_RESET_BACK, INPUT_PULLUP);
  pinMode(BUTTON_TOUCH_OK, INPUT_PULLUP);
  pinMode(BUTTON_GOAL_DN, INPUT_PULLUP);
  pinMode(BUTTON_START_UP, INPUT_PULLUP);

  Serial.begin(9600);
  while (!Serial) {
    ;  // Needed for native USB port only
  }
  // sdCardInit();
  // cardInfo();
  setupLED();

  radio.begin(10000);
}

/*********************************************** process radio data ***/
int errorCount = 0;
int firstMisses = 0;
uint32_t lockoutTime;

char getRadioChar() {
  while (radio.available() == 0) {
  }
  return radio.read();
}

/*********************************************** main loop ******************/
void loop() {  // run over and over
  if (radio.available()) {
    if (radio.read() == '*') {
    }
  }
  if (Serial.available()) {
    Serial.write(Serial.read());
  }
  if (millis() > displayUpdateTime) {
    displayUpdateTime += displayUpdateInterval;
    state = 1 - state;
  }

  if (millis() > animUpdateTime) {
    animUpdateTime += 200;
    digitalWrite(DISP_SELECT, LOW);  // digit 0
    SPI.transfer(1);
    SPI.transfer(millis()%10);
    digitalWrite(DISP_SELECT, HIGH);
  }
  digitalWrite(LED_4, digitalRead(BUTTON_TOUCH_OK));
  digitalWrite(LED_3, digitalRead(BUTTON_GOAL_DN));
  digitalWrite(LED_2, digitalRead(BUTTON_START_UP));
}
