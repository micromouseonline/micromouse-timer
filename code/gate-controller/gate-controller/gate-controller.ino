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
#include <LiquidCrystal_I2C.h>
#include <SD.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include "RTClib.h"
#include "button.h"
#include "stopwatch.h"

/// Pin Assignments
const int LED_2 = 3;
const int LED_3 = 4;
const int LED_4 = 5;
const int LED_5 = 6;

const int RADIO_RX = 7;
const int RADIO_TX = 8;
const int SD_DETECT = 9;
const int SD_SELECT = 10;
const int SD_MOSI = 11;
const int SD_MISO = 12;
const int SD_SCK = 13;

const int ENC_BTN = 2;
const int ENC_A = A0;
const int ENC_B = A1;

const int BUTTON_START = A2;
const int BUTTON_GOAL = A3;
const int BUTTON_TOUCH = A6;
const int BUTTON_RESET = A7;

const int I2C_SDA = A4;
const int I2C_SCL = A5;
///////////////////////////////////////////////////////////////////

Button encoderButton(ENC_BTN, Button::ACTIVE_LOW);
Button startButton(BUTTON_START, Button::ACTIVE_LOW, Button::ANALOG);
Button goalButton(BUTTON_GOAL, Button::ACTIVE_LOW, Button::ANALOG);
Button touchButton(BUTTON_TOUCH, Button::ACTIVE_LOW, Button::ANALOG);
Button resetButton(BUTTON_RESET, Button::ACTIVE_LOW, Button::ANALOG);

// I2C LCD pin numbers are on the extender chip - not the arduino
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
PCF8563 rtc(0x51 * 2);
/***
 * The radio is used only in receive mode here but the SoftwareSerial requires
 * two pins for the configuration. TX is not used
 */
SoftwareSerial radio(7, 8);  // RX, TX


Stopwatch mazeTimer;
Stopwatch runTimer;
uint32_t mazeTimeTime = 0;
uint32_t bestTime = UINT32_MAX;
int runCount = 0;

/***
 * LCD local storage
 */


const char c0[] PROGMEM = {0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x4};
const char c1[] PROGMEM = {0x4, 0x4, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0};
const char c2[] PROGMEM = {0x0, 0x0, 0x4, 0x4, 0x4, 0x0, 0x0, 0x0};
const char c3[] PROGMEM = {0x0, 0x0, 0x0, 0x0, 0x4, 0x4, 0x4, 0x0};
const char c4[] PROGMEM = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x4};
const char c5[] PROGMEM = {0x0, 0x0, 0xa, 0x1f, 0x1f, 0xe, 0x4, 0x0};  // close heart
const char c6[] PROGMEM = {0x0, 0x0, 0xa, 0x15, 0x11, 0xa, 0x4, 0x0};  // open heart
const char c7[] PROGMEM = {0x0, 0x1, 0x3, 0x16, 0x1c, 0x8, 0x0, 0x0};  // tick
char anim[] = {0, 1, 2, 3, 4, 5, 6, 5, 6};

const uint32_t ONE_SECOND = 1000L;
const uint32_t ONE_MINUTE = 60 * ONE_SECOND;
const uint32_t ONE_HOUR = 60 * ONE_MINUTE;

uint32_t updateInterval = 29;
uint32_t updateTime;
int state = 0;
uint32_t animUpdateTime = 0;
int frameCounter;
uint32_t systime;
uint32_t startTime;
enum TimerState {
  INIT = 0,
  IDLE,
  ARMED,
  RUNNING,
  GOAL,
  RETURN,
};

TimerState timerState = IDLE;

/***************************************************   Timer */
class Timer {
 public:
  Timer(){};
  void start() {
    mRunning = true;
    mStartTime = millis();
  };
  void stop() { mRunning = false; }
  bool running() { return mRunning; }
  bool expired(uint32_t timeoutTime) {
    if (!mRunning) {
      return true;
    }
    const uint32_t elapsed = millis() - mStartTime;
    bool expired = elapsed >= timeoutTime;
    if (expired) {
      stop();
    }
    return expired;
  }

 private:
  bool mRunning = false;
  uint32_t mStartTime;
};
/***************************************************   Timer end */
/***************************************************   Encoder */
Timer pressTimer;
Timer longTimer;
bool gLongPress = false;
bool gShortPress = false;
int8_t gEncoderValue = 0;
int8_t gEncoderClicks = 0;
const int ENC_COUNTS_PER_CLICK = 4;
const int DEBOUNCE_PERIOD = 20;
const int LONG_PERIOD = 250;
const int8_t deltas[16] PROGMEM = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
void encoderUpdate() {
  static uint8_t bits = 0;
  bits <<= 2;
  bits |= digitalRead(ENC_B) ? 2 : 0;
  bits |= digitalRead(ENC_A) ? 1 : 0;
  bits &= 0x0f;
  gEncoderValue += pgm_read_word(deltas + bits);
  gEncoderClicks = gEncoderValue / ENC_COUNTS_PER_CLICK;
}

/***************************************************   Encoder end */
/***************************************************   SD Card */

const int chipSelect = 10;
const int cardDetect = 9;

void sdCardInit() {
  pinMode(chipSelect, OUTPUT);
  pinMode(cardDetect, INPUT_PULLUP);
  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!SD.begin(chipSelect)) {
    Serial.println(F("initialization failed!"));
    return;
  }
  Serial.println(F("Card present"));
}

void cardInfo() {
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  /*
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
  */
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

/*********************************************** BUTTON ******************/

void buttonsUpdate() {
  startButton.update();
  goalButton.update();
  touchButton.update();
  resetButton.update();
  encoderButton.update();
}
/*********************************************** BUTTONS END ******************/

/*********************************************** UTILITIES ******************/
void flashLeds(int n) {
  while (n--) {
    digitalWrite(LED_2, 1);
    digitalWrite(LED_3, 1);
    digitalWrite(LED_4, 1);
    digitalWrite(LED_5, 1);
    delay(100);
    digitalWrite(LED_2, 0);
    digitalWrite(LED_3, 0);
    digitalWrite(LED_4, 0);
    digitalWrite(LED_5, 0);
    delay(100);
  }
}
/*********************************************** UTILITIES END***************/

/*********************************************** systick ******************/

/***
 * If you are interested in what all this does, the ATMega328P datasheet
 * has all the answers but it is not easy to follow until you have some
 * experience. For now just use the code as it is.
 */
void setupSystick() {
  // set the mode for timer 2
  bitClear(TCCR2A, WGM20);
  bitSet(TCCR2A, WGM21);
  bitClear(TCCR2B, WGM22);
  // set divisor to 128 => timer clock = 125kHz
  bitSet(TCCR2B, CS22);
  bitClear(TCCR2B, CS21);
  bitSet(TCCR2B, CS20);
  // set the timer frequency
  OCR2A = 249;  // (16000000/128/500)-1 = 249
  // enable the timer interrupt
  bitSet(TIMSK2, OCIE2A);
}

inline void systick() {
  encoderUpdate();
  buttonsUpdate();
}

// the systick event is an ISR attached to Timer 2
ISR(TIMER2_COMPA_vect) {
  systick();
}
/*********************************************** systick  end******************/

void press(){
  Serial.println(F("press"));
}
void click(){
  Serial.println(F("click"));
}

void longPress(){
  Serial.println(F("Long press"));
}

/******************************************************** SETUP *****/
void setup() {
  encoderButton.registerClickHandler(click);
  encoderButton.registerPressHandler(press);
  encoderButton.registerLongPressHandler(longPress);
  pinMode(LED_2, OUTPUT);
  pinMode(LED_3, OUTPUT);
  pinMode(LED_4, OUTPUT);
  pinMode(LED_5, OUTPUT);
  pinMode(SD_DETECT, INPUT_PULLUP);

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);

  digitalWrite(LED_2, 1);
  Serial.begin(57600);
  while (!Serial) {
    ;  // Needed for native USB port only
  }
  digitalWrite(LED_3, 1);
  Wire.begin();
  lcd.begin(20, 4);  //(backlight is on)
  lcd.createChar(0, c0);
  lcd.createChar(1, c1);
  lcd.createChar(2, c2);
  lcd.createChar(3, c3);
  lcd.createChar(4, c4);
  lcd.createChar(5, c5);
  lcd.createChar(6, c6);
  lcd.createChar(7, c7);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Initialising SD card"));
  sdCardInit();
  cardInfo();

  /***
   * The PCF8256 seems to have significant drift :(
   * The time will get reset every build with this line but it is
   * probably best to implement some host-mediated time setting function
   */
  digitalWrite(LED_3, 1);
  rtc.begin();
  rtc.adjust(DateTime(__DATE__, __TIME__));
  radio.begin(10000);
  digitalWrite(LED_4, 1);
  lcd.clear();
  lcd.print(F("HELLO!"));
  delay(500);
  flashLeds(4);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("     Micromouse     "));
  lcd.setCursor(0, 1);
  lcd.print(F("   Contest Timer    "));
  lcd.setCursor(0, 2);
  lcd.print(F("--------------------"));
  lcd.setCursor(0, 3);
  lcd.print(F("  P Harrison 2020  "));
  startTime = millis();

  runTimer.reset();
  mazeTimer.reset();
  digitalWrite(LED_5, 1);
  setupSystick();
  while (!encoderButton.isPressed() && !resetButton.isPressed()) {
    // cli();
    // Serial.print(analogRead(BUTTON_START));
    // Serial.print(" ");
    // Serial.print(analogRead(BUTTON_GOAL));
    // Serial.print(" ");
    // Serial.print(analogRead(BUTTON_TOUCH));
    // Serial.print(" ");
    // Serial.print(analogRead(BUTTON_RESET));
    // Serial.println(" ");
    // sei();
  }
  lcd.clear();
  /// Writing all four lines takes 500us
  lcd.setCursor(0, 0);
  lcd.print(F("MAZE AUTO   Run:"));
  lcd.setCursor(0, 1);
  lcd.print(F("Maze Time"));
  lcd.setCursor(0, 2);
  lcd.print(F(" Run Time"));
  lcd.setCursor(0, 3);
  lcd.print(F("Best Time"));
  lcd.setCursor(0, 3);

  digitalWrite(LED_2, 0);
  digitalWrite(LED_3, 0);
  digitalWrite(LED_4, 0);
  digitalWrite(LED_5, 0);
  timerState = INIT;
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

void handlePacket() {
  char packet[20] = {' '};
  packet[19] = 0;
  for (int i = 0; i < 19; i++) {
    char c = getRadioChar();
    // Serial.print(c);
    if (c == '#') {
      break;
    }
    packet[i] = c;
  }
  // return;
  while (radio.available()) {
    radio.read();
  }

  char check = '*';
  for (int i = 0; i < 7; i++) {
    check ^= packet[i];
  }
  check = check % 16 + 'a';
  if (packet[7] != check) {
    Serial.println(F("--------"));
    lcd.setCursor(10, 2);
    lcd.print('!');
    errorCount++;
    return;
  }
  lcd.setCursor(0, 3);
  lcd.print(errorCount);

  lcd.setCursor(5, 3);
  lcd.print(firstMisses);

  if (millis() > lockoutTime) {
    int sequence = int(packet[1]) - '0';
    lockoutTime = millis() + 600 - 50 * sequence;
    if (sequence > 0) {
      firstMisses++;
    }
    Serial.println(packet);
    lcd.setCursor(0, 2);
    lcd.print(packet);
    lcd.setCursor(10, 2);
    lcd.print(char(7));
  }
}

/*********************************************** time display functions ***/
// these could be more RAM efficient. look at the dividex code
void showTime(int column, int line, uint32_t time) {
  uint32_t seconds = (time / ONE_SECOND);
  uint32_t minutes = (time / ONE_MINUTE);
  uint32_t ms = (time % 1000);
  char lineBuffer[10];
  sprintf(lineBuffer, "%02ld:%02ld.%03ld", minutes % 60, seconds % 60, ms);
  lcd.setCursor(column, line);
  lcd.print(lineBuffer);
}

void showElapsedTime(int column, int line) {
  uint32_t time = millis() - startTime;
  showTime(column, line, time);
}

void showTimer(int column, int line, Stopwatch& stopwatch) {
  showTime(column, line, stopwatch.time());
}

void showSystemTime(int column, int line) {
  DateTime now = rtc.now();
  char lineBuffer[24];
  strncpy(lineBuffer, "DD.MM.YYYY  hh:mm:ss\0", sizeof(lineBuffer));
  lcd.setCursor(column, line);
  lcd.print(now.format(lineBuffer));
}

/*********************************************** maze state machine *********/
void showState() {
  lcd.setCursor(0, 0);
  switch (timerState) {
    case INIT:
      lcd.print(F("INIT     "));
      break;
    case IDLE:
      lcd.print(F("IDLE     "));
      break;
    case ARMED:
      lcd.print(F("ARMED    "));
      break;
    case RUNNING:
      lcd.print(F("RUNNING  "));
      break;
    case GOAL:
      lcd.print(F("GOAL     "));
      break;
    case RETURN:
      lcd.print(F("RETURN   "));
      break;
    default:
      lcd.print(F("---------"));
      break;
  }
}

void displayInit() {
  lcd.setCursor(11, 3);
  lcd.print(F("--:--.---"));
  lcd.setCursor(17, 0);
  lcd.print(F("  "));
  lcd.setCursor(10, 0);
  lcd.write('-');
}
void mazeMachine() {
  if (resetButton.isPressed()) {
    timerState = INIT;
  }
  showState();
  switch (timerState) {
    case INIT:
      mazeTimer.reset();
      runTimer.reset();
      runCount = 0;
      bestTime = UINT32_MAX;
      displayInit();
      timerState = IDLE;
      break;
    case IDLE:
      if (startButton.isPressed()) {
        // mazeTimer.restart();
        // runTimer.restart();
        timerState = ARMED;
      }
      if (touchButton.isPressed()) {
        timerState = ARMED;
      }
      break;
    case ARMED:   // robot in start cell, ready to run
      if (startButton.isPressed()) {
        if(!mazeTimer.running()){
          mazeTimer.restart();
        }
        runTimer.restart();
        runCount++;
        timerState = RUNNING;
      }
      break;
    case RUNNING:
      if (goalButton.isPressed()) {
        runTimer.stop();
        if (runTimer.time() < bestTime) {
          bestTime = runTimer.time();
          showTime(11, 3, bestTime);
        }
        timerState = GOAL;
      }
      if (touchButton.isPressed()) {
        runTimer.reset();
        timerState = ARMED;
      }
      break;
    case GOAL:
      if (startButton.isPressed()) {
        timerState = ARMED;
      }
      if (touchButton.isPressed()) {
        timerState = ARMED;
      }
      break;
    case RETURN:
      // could be used to time additional exploration
      break;
    default:
      break;
  }
}
/*********************************************** main loop ******************/

void loop() {
  mazeMachine();
  if (radio.available()) {
    if (radio.read() == '*') {
      handlePacket();
    }
  }
  if (Serial.available()) {
    Serial.write(Serial.read());
  }
  if (millis() > updateTime) {
    updateTime += updateInterval;
    lcd.setCursor(17, 0);
    lcd.print(runCount);
    showTimer(11, 1, mazeTimer);
    showTimer(11, 2, runTimer);
  }
}
