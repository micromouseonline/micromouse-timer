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
#include <SoftwareSerial.h>
#include <Wire.h>
#include <sdcard.h>
#include "RTClib.h"
#include "basic-timer.h"
#include "button.h"
#include "messages.h"
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
// click button on the encoder
Button encoderButton(ENC_BTN, Button::ACTIVE_LOW);
// Front panel buttons
Button startButton(BUTTON_START, Button::ACTIVE_LOW, Button::ANALOG);
Button goalButton(BUTTON_GOAL, Button::ACTIVE_LOW, Button::ANALOG);
Button touchButton(BUTTON_TOUCH, Button::ACTIVE_LOW, Button::ANALOG);
Button resetButton(BUTTON_RESET, Button::ACTIVE_LOW, Button::ANALOG);

// I2C LCD pin numbers are on the extender chip - not the arduino
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// TODO: why does the PCF8563 need its address doubled here?
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

// Some used defined characters for the LCD display
const char c0[] PROGMEM = {0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x4};
const char c1[] PROGMEM = {0x4, 0x4, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0};
const char c2[] PROGMEM = {0x0, 0x0, 0x4, 0x4, 0x4, 0x0, 0x0, 0x0};
const char c3[] PROGMEM = {0x0, 0x0, 0x0, 0x0, 0x4, 0x4, 0x4, 0x0};
const char c4[] PROGMEM = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x4};
const char c5[] PROGMEM = {0x0, 0x0, 0xa, 0x1f, 0x1f, 0xe, 0x4, 0x0};  // closed heart
const char c6[] PROGMEM = {0x0, 0x0, 0xa, 0x15, 0x11, 0xa, 0x4, 0x0};  // open heart
const char c7[] PROGMEM = {0x0, 0x1, 0x3, 0x16, 0x1c, 0x8, 0x0, 0x0};  // tick

/***
 * The display only needs to be updated about 10-15 times per second
 * The interval is chosen to avoid aliasing of the counts when they are
 * displayed. If it were exactly 100ms, one of the digits may appear to
 * count slowly. Choose a prime number for least liklihood of aliasing.
 * The update interval does not affect any of the timing resolution or accuracy
 */
const uint32_t displayUpdateInterval = 83;  // milliseconds
uint32_t displayUpdateTime;

// divisors for unpacking millisecond timestamps
const uint32_t ONE_SECOND = 1000L;
const uint32_t ONE_MINUTE = 60 * ONE_SECOND;
const uint32_t ONE_HOUR = 60 * ONE_MINUTE;

// Multi purpose states for the various contest state machines
enum ContestState {
  INIT = 0,
  IDLE,
  ARMED,
  RUNNING,
  GOAL,
  RETURN,
};

ContestState contestState = IDLE;

/*
David's states

enum SystemState {
  STATE_POWER_UP = 0,
  STATE_NO_MOUSE = 1,
  STATE_MOUSE_IN_START = 2,
  STATE_PASS_START_GATE = 3,
  STATE_MOUSE_RUNNING = 4,
  STATE_WAIT_FORCLEAR_GOAL_GATE = 5,
  STATE_MOUSE_RETURNING = 6,
  NUM_STATES
};
*/

/***************************************************   Encoder */
int8_t gEncoderValue = 0;
int8_t gEncoderClicks = 0;
const int ENC_COUNTS_PER_CLICK = 4;
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
void flashLeds(int count) {
  while (count--) {
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

void showWelcomeScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("     Micromouse     "));
  lcd.setCursor(0, 1);
  lcd.print(F("   Contest Timer    "));
  lcd.setCursor(0, 2);
  lcd.print(F("--------------------"));
  lcd.setCursor(0, 3);
  lcd.print(F("  P Harrison 2020  "));
}

void showMazeScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("MAZE AUTO   Run:"));
  lcd.setCursor(0, 1);
  lcd.print(F("Maze Time"));
  lcd.setCursor(0, 2);
  lcd.print(F(" Run Time"));
  lcd.setCursor(0, 3);
  lcd.print(F("Best Time"));
}

// test functions for encoder button
void encoderPress() {
  Serial.println(F("press"));
}
void encoderClick() {
  Serial.println(F("click"));
}

void encoderLongPress() {
  Serial.println(F("Long press"));
}
/*********************************************** UTILITIES END***************/

/*********************************************** systick ******************/
/***
 * Systick runs every 2ms. It is used to sample the encoder so it must run
 * frequently enough to catch all the encoder states even if the user
 * spins the knob quickly.
 */
void setupSystick() {
  // set the mode for timer 2 as regulr interrupts
  bitClear(TCCR2A, WGM20);
  bitSet(TCCR2A, WGM21);
  bitClear(TCCR2B, WGM22);
  // set divisor to  => timer clock, Fclk = 125kHz
  bitSet(TCCR2B, CS22);
  bitClear(TCCR2B, CS21);
  bitSet(TCCR2B, CS20);
  // timer interval is (n+1)/Fclk seconds
  OCR2A = 249;             // 0.002 * 125000 - 1 = 249
  bitSet(TIMSK2, OCIE2A);  // enable the timer interrupt
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

/******************************************************** SETUP *****/
void setup() {
  pinMode(LED_2, OUTPUT);
  pinMode(LED_3, OUTPUT);
  pinMode(LED_4, OUTPUT);
  pinMode(LED_5, OUTPUT);

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);

  encoderButton.registerClickHandler(encoderClick);
  encoderButton.registerPressHandler(encoderPress);
  encoderButton.registerLongPressHandler(encoderLongPress);

  Serial.begin(9600);
  while (!Serial) {
    ;  // Needed for native USB port only
  }
  digitalWrite(LED_2, 1);
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
  digitalWrite(LED_3, 1);
  sdCardInit(SD_SELECT, SD_DETECT);
  cardInfo();

  /***
   * The PCF8256 seems to have significant drift :(
   * The time will get reset every build with this line but it is
   * probably best to implement some host-mediated time setting function
   */
  digitalWrite(LED_4, 1);
  rtc.begin();
  rtc.adjust(DateTime(__DATE__, __TIME__));
  radio.begin(10000);

  digitalWrite(LED_5, 1);

  lcd.clear();
  lcd.print(F("HELLO!"));
  delay(500);
  flashLeds(4);
  showWelcomeScreen();
  runTimer.reset();
  mazeTimer.reset();
  setupSystick();
  while (!encoderButton.isPressed() && !resetButton.isPressed()) {
  }

  showMazeScreen();

  digitalWrite(LED_2, 0);
  digitalWrite(LED_3, 0);
  digitalWrite(LED_4, 0);
  digitalWrite(LED_5, 0);
  contestState = INIT;
  sendMessage(MSG_NEW_MOUSE, 0);
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

/***
 * display timestamp as minutes, seconds and milliseconds - MM:SS.SSS
 * input time is in milliseconds
 * no validity checks are made
 */
void showTime(int column, int line, uint32_t time) {
  uint32_t ms = (time % 1000);
  uint32_t seconds = (time / ONE_SECOND) % 60;
  uint32_t minutes = (time / ONE_MINUTE) % 60;
  char lineBuffer[10] = {0};
  char* p = lineBuffer;
  *p++ = '0' + minutes / 10;
  *p++ = '0' + minutes % 10;
  *p++ = ':';
  *p++ = '0' + seconds / 10;
  *p++ = '0' + seconds % 10;
  *p++ = '.';
  *p++ = '0' + (ms / 100) % 10;
  *p++ = '0' + (ms / 10) % 10;
  *p++ = '0' + (ms / 1) % 10;
  lcd.setCursor(column, line);
  lcd.print(lineBuffer);
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
  switch (contestState) {
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
    contestState = INIT;
  }
  showState();
  switch (contestState) {
    case INIT:
      mazeTimer.reset();
      runTimer.reset();
      runCount = 0;
      bestTime = UINT32_MAX;
      displayInit();
      contestState = IDLE;
      break;
    case IDLE:
      if (startButton.isPressed()) {
        contestState = ARMED;
      }
      if (touchButton.isPressed()) {
        contestState = ARMED;
      }
      break;
    case ARMED:  // robot in start cell, ready to run
      if (startButton.isPressed()) {
        if (!mazeTimer.running()) {
          mazeTimer.restart();
          sendMessage(MSG_MAZE_TIME, 0);
        }
        sendMessage(MSG_SPLIT_TIME, 0);
        runTimer.restart();
        runCount++;
        contestState = RUNNING;
      }
      break;
    case RUNNING:  // robot on its way to the goal
      if (goalButton.isPressed()) {
        runTimer.stop();
        uint32_t time = runTimer.time();
        sendMessage(MSG_RUN_TIME_MS, time);
        sendMessage(MSG_SPLIT_TIME, 0);
        if (time < bestTime) {
          bestTime = time;
          showTime(11, 3, bestTime);
        }
        contestState = GOAL;
      }
      if (touchButton.isPressed()) {
        runTimer.stop();
        // sendMessage(MSG_RUN_TIME_MS,runTimer.time());
        contestState = ARMED;
      }
      break;
    case GOAL:  // root has entered the goal and could be returning or exploring
      if (touchButton.isPressed()) {
        // robot is back in start cell or run is aborted
        contestState = ARMED;
      }
      break;
    case RETURN:
      // could be used to time additional exploration timing
      break;
    default:
      break;
  }
}
/*********************************************** main loop ******************/

void loop() {
  if (radio.available()) {
    if (radio.read() == '*') {
      handlePacket();
    }
  }
  mazeMachine();
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '>') {
      contestState = INIT;
    }
  }
  if (millis() > displayUpdateTime) {
    displayUpdateTime += displayUpdateInterval;
    lcd.setCursor(17, 0);
    lcd.print(runCount);
    showTime(11, 1, mazeTimer.time());
    showTime(11, 2, runTimer.time());
  }
}
