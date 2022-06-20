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
#include "kerikun-timer.h"
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

const int BUTTON_ARM = A7;
const int BUTTON_START = A6;  // analogue only
const int BUTTON_GOAL = A3;   // analogue only
const int BUTTON_RESET = A2;

const int I2C_SDA = A4;
const int I2C_SCL = A5;

///////////////////////////////////////////////////////////////////
// click button on the encoder
Button encoderButton(ENC_BTN, Button::ACTIVE_LOW);
// Front panel buttons
Button startButton(BUTTON_START, Button::ACTIVE_LOW, Button::ANALOG);
Button goalButton(BUTTON_GOAL, Button::ACTIVE_LOW, Button::ANALOG);
Button armButton(BUTTON_ARM, Button::ACTIVE_LOW, Button::ANALOG);
Button resetButton(BUTTON_RESET, Button::ACTIVE_LOW, Button::ANALOG);

const uint8_t BTN_NONE = 0;
const uint8_t BTN_GREEN = 1;
const uint8_t BTN_YELLOW = 2;
const uint8_t BTN_RED = 4;
const uint8_t BTN_BLUE = 8;
const uint8_t BTN_ENCODER = 16;

volatile uint8_t button_state = BTN_NONE;

/***
 * Note that the adress of the I2C expander depends on the exact
 * chip version and the theree address links.
 * There are two common choices for the expander.
 *
 * The PCF8574T has a basic address of 0x27 with no links bridged
 * The PCF8574AT has a basic address of 0x3F with no links bridged
 *
 * The address bridges, A0..A2 clear the corresponding bits in the
 * address. Thus, bridging all the links would make the addresses
 * 0x20 and 0x38 respectively.
 *
 * Also note that the LiquidCrystal_I2C library, like many Arduino
 * sketches/libraries, uses a right-justified address.
 */
// I2C LCD pin numbers are on the extender chip - not the arduino
LiquidCrystal_I2C lcd(0x3f, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

/***
 * The PCF8563 library expects a left-justified address. Hence the
 * multiplication by two in the constructor
 */
PCF8563 rtc(0x51 * 2);

/***
 * The radio is used only in receive mode here but the SoftwareSerial requires
 * two pins for the configuration. TX is not used
 */
SoftwareSerial radio(RADIO_RX, RADIO_TX);  // RX, TX

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
const int ST_CALIBRATE = 0;  // calibrate gates,
const int ST_WAITING = 1;    // looking for mouse in start cell,
const int ST_ARMED = 2;      // Mouse seen in start cell,
const int ST_STARTING = 3;   // Run started (but not cleared start gate yet)
const int ST_RUNNING = 4;    // Run in progress,
const int ST_GOAL = 5;       // Run to centre completed (finish gete triggered)
const int ST_NEW_MOUSE = 6;  // Set up for new mouse

int contestState = ST_WAITING;

enum { CT_NONE = 0, CT_MAZE, CT_TRIAL };
int contest_type = CT_NONE;

////////////////////////////////////////////////////////////////////////////
// This is only going to work with an ATMega328 processor - take care  ////
// The ATMega328P in ther arduino nano/un has a bootloader installed at the
// top of flash. The default fuse values set aside just 515 bytes (256 words)
// for the  bootloader so it must start at FLASH_END - 0x100 = 0x7E00
// the fuses are also programmed so that the external reset vector points to
// the bootloader rather than adress 0x0000.
// The bootloader code sets the watchdog timer value to oone second. This,
// if it receives no valid instructions in that time the processor is reset
// to address 0x000 with all the registers (except MCUSR and SP) set to
// their power on values. Note that some versions of the bootloader will
// clear the MCUSR so that it is not possible to reliably detect the
// cause of a reset.
// The result of all this is that the Arduino can be reliably reset by simply
// jumping to the bootloader. Assuming there is not a programmer trying to
// send it commands of course.
// This is also a good way to start a firmware update from the existing
// firmware.
////////////////////////////////////////////////////////////////////////////
void reset_processor() {
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__)
  noInterrupts();

  asm volatile("jmp 0x7E00");  // bootloader start address
#endif
}
////////////////////////////////////////////////////////////////////////////

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
  armButton.update();
  resetButton.update();
  encoderButton.update();
  button_state = BTN_NONE;
  if (armButton.isPressed()) {
    button_state |= BTN_GREEN;
  }
  if (startButton.isPressed()) {
    button_state |= BTN_YELLOW;
  }
  if (goalButton.isPressed()) {
    button_state |= BTN_RED;
  }
  if (resetButton.isPressed()) {
    button_state |= BTN_BLUE;
  }
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

void show_trial_screen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("TRIAL AUTO   Run:"));
  lcd.setCursor(0, 1);
  lcd.print(F("Trial Time"));
  lcd.setCursor(0, 2);
  lcd.print(F("  Run Time"));
  lcd.setCursor(0, 3);
  lcd.print(F(" Best Time"));
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
  // i2cScan();
}

void i2cScan() {
  byte error, address;
  int nDevices;

  nDevices = 0;
  for (address = 1; address < 127; address++) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");

      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
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

void set_state(int new_state) {
  contestState = new_state;
  send_message(MSG_CURRENT_STATE, contestState);  // log current state to PC
}

/*********************************************** process radio data ***/

// also used for gate ID
enum ReaderState { RD_NONE, RD_WAIT, RD_HOME, RD_START, RD_GOAL, RD_TERM, RD_DONE, RD_ERROR };

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
    case ST_CALIBRATE:
      lcd.print(F("CALIBRATE"));
      break;
    case ST_NEW_MOUSE:
      lcd.print(F("INIT     "));
      break;
    case ST_WAITING:
      lcd.print(F("WAITING  "));
      break;
    case ST_ARMED:
      lcd.print(F("ARMED    "));
      break;
    case ST_STARTING:
      lcd.print(F("STARTING "));
      break;
    case ST_RUNNING:
      lcd.print(F("RUNNING  "));
      break;
    case ST_GOAL:
      lcd.print(F("GOAL     "));
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

void trial_machine(){

};

void mazeMachine() {
  if (resetButton.isPressed()) {
    set_state(ST_NEW_MOUSE);
  }

  showState();
  // Serial.println(reader_state);
  switch (contestState) {
    case ST_NEW_MOUSE:
      mazeTimer.reset();
      runTimer.reset();
      runCount = 0;
      bestTime = UINT32_MAX;
      displayInit();
      set_state(ST_WAITING);
      reader_state = RD_WAIT;
      gate_id = RD_NONE;
      break;
    case ST_WAITING:
      if (armButton.isPressed() || gate_id == RD_HOME) {
        set_state(ST_ARMED);
        reader_state = RD_WAIT;
        // gate_id = RD_NONE;
        if (runCount == 0) {
          send_maze_time(0);
          mazeTimer.restart();
        }
      }
      break;
    case ST_ARMED:  // robot in start cell, ready to run
      if (startButton.isPressed() || gate_id == RD_START) {
        send_split_time(0);
        runTimer.restart();
        runCount++;
        set_state(ST_RUNNING);
        gate_id = RD_NONE;
      }
      break;
    case ST_RUNNING:  // robot on its way to the goal
      if (goalButton.isPressed() || gate_id == RD_GOAL) {
        runTimer.stop();
        uint32_t time = runTimer.time();
        set_state(ST_GOAL);
        send_run_time(time);
        if (time < bestTime) {
          bestTime = time;
          showTime(11, 3, bestTime);
        }
        gate_id = RD_NONE;
      }
      if (armButton.isPressed() || gate_id == RD_HOME) {
        runTimer.stop();
        runTimer.reset();
        // send_message(MSG_RUN_TIME_MS,runTimer.time());
        set_state(ST_ARMED);
        gate_id = RD_NONE;
      }
      break;
    case ST_GOAL:  // root has entered the goal and could be returning or exploring
      if (armButton.isPressed() || gate_id == RD_HOME) {
        // robot is back in start cell or run is aborted
        set_state(ST_ARMED);
        gate_id = RD_NONE;
      }
      break;
    default:
      break;
  }
}

void pintest() {
  if (startButton.isPressed()) {
    Serial.println("start");
  }

  if (goalButton.isPressed()) {
    Serial.println("goal");
  }
  if (armButton.isPressed()) {
    Serial.println("touch");
  }
  if (resetButton.isPressed()) {
    Serial.println("reset");
  }
  if (encoderButton.isPressed()) {
    Serial.println("enc");
  }
}

int select_contest_type() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Contest Timer  V0.9a"));
  lcd.setCursor(0, 2);
  lcd.print(F(" GREEN: MAZE EVENT"));
  lcd.setCursor(0, 3);
  lcd.print(F("YELLOW: TIME TRIAL"));
  while (button_state == BTN_NONE) {
    delay(50);
  }
  int type = CT_MAZE;  // default
  if (button_state == BTN_GREEN) {
    type = CT_MAZE;
  } else if (button_state == BTN_YELLOW) {
    type = CT_TRIAL;
  }
  lcd.clear();
  return type;
}

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
  Serial.println(F("CONTEST_ TIMER V0.9a"));

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
  lcd.print(F("SD card ... "));
  Serial.println(F("Initialising SD card"));
  sdCardInit(SD_SELECT, SD_DETECT);
  lcd.print(F("Done"));

  lcd.setCursor(0, 1);
  lcd.print(F("RTC ...     "));
  rtc.begin();
  lcd.print(F("Done"));
  /***
   * The PCF8256 seems to have significant drift :(
   * The time will get reset every build with this line but it is
   * probably best to implement some host-mediated time setting function
   */
  // rtc.adjust(DateTime(__DATE__, __TIME__));
  char buf[32];
  Serial.println(rtc.now().tostr(buf));
  lcd.setCursor(0, 2);
  lcd.print(F("RADIO ...   "));
  radio.begin(9600);
  lcd.print(F("Done"));
  delay(1000);

  setupSystick();
  contest_type = select_contest_type();
  runTimer.reset();
  mazeTimer.reset();
  switch (contest_type) {
    case CT_MAZE:
      showMazeScreen();
      break;
    case CT_TRIAL:
      show_trial_screen();
      break;
    default:
      lcd.clear();
      lcd.print(F("NO CONTEST TYPE"));
      break;
  }

  contestState = ST_NEW_MOUSE;
  send_message(MSG_NewMouse, 0);
}

/*********************************************** main loop ******************/

void loop() {
  if (radio.available()) {
    char c = radio.read();
    reader(c);
  }

  if (Serial.available()) {
    char c = Serial.read();
    if (c == '>') {
      set_state(ST_NEW_MOUSE);
    }
  }
  if (button_state == (BTN_BLUE + BTN_GREEN)) {
    reset_processor();
  }
  if (contest_type == CT_MAZE) {
    mazeMachine();
  } else if (contest_type == CT_TRIAL) {
    trial_machine();
  } else {
    // do nothing
  }
  if (contest_type != CT_NONE && millis() > displayUpdateTime) {
    displayUpdateTime += displayUpdateInterval;
    lcd.setCursor(17, 0);
    lcd.print(runCount);
    showTime(11, 1, mazeTimer.time());
    showTime(11, 2, runTimer.time());
  }
}
