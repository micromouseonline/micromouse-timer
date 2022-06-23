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
#include "button.h"
#include "messages.h"
#include "pins.h"
#include "stopwatch.h"
#include "utils.h"

///////////////////////////////////////////////////////////////////
// click button on the encoder
BasicButton encoderButton(ENC_BTN, BasicButton::ACTIVE_LOW);
// Front panel buttons
BasicButton startButton(BUTTON_START, BasicButton::ACTIVE_LOW, BasicButton::ANALOG);
BasicButton goalButton(BUTTON_GOAL, BasicButton::ACTIVE_LOW, BasicButton::ANALOG);
BasicButton armButton(BUTTON_ARM, BasicButton::ACTIVE_LOW, BasicButton::ANALOG);
BasicButton resetButton(BUTTON_RESET, BasicButton::ACTIVE_LOW, BasicButton::ANALOG);

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
uint32_t g_maze_time;
uint32_t g_run_time;
uint32_t g_run_start_time;
uint32_t g_maze_start_time;
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
 * The update interval should not affect any of the timing resolution or
 * accuracy though it takes a bit of time for each update
 */
const uint32_t displayUpdateInterval = 23;  // milliseconds
uint32_t displayUpdateTime;

// divisors for unpacking millisecond timestamps
const uint32_t ONE_SECOND = 1000L;
const uint32_t ONE_MINUTE = 60 * ONE_SECOND;
const uint32_t ONE_HOUR = 60 * ONE_MINUTE;

const uint32_t TIME_TRIAL_LOCKOUT = 2000;
// Multi purpose states for the various contest state machines
const int ST_CALIBRATE = 0;  // calibrate gates,
const int ST_WAITING = 1;    // looking for mouse in start cell,
const int ST_ARMED = 2;      // Mouse seen in start cell,
const int ST_STARTING = 3;   // Run started (but not cleared start gate yet)
const int ST_RUNNING = 4;    // Run in progress,
const int ST_GOAL = 5;       // Run to centre completed (finish gete triggered)
const int ST_NEW_MOUSE = 6;  // Set up for new mouse

int contestState = ST_WAITING;

enum { CT_NONE = 0, CT_MAZE, CT_TRIAL, CT_RADIO };
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

void show_trial_screen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("TRIAL      Run:"));
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
  lcd.print(F("MAZE       Run:"));
  lcd.setCursor(0, 1);
  lcd.print(F("Maze Time"));
  lcd.setCursor(0, 2);
  lcd.print(F(" Run Time"));
  lcd.setCursor(0, 3);
  lcd.print(F("Best Time"));
}

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
enum ReaderState { RD_NONE, RD_WAIT, RD_CONFIRM, RD_HOME, RD_START, RD_GOAL, RD_TERM, RD_TRIGGERED, RD_ERROR };

volatile ReaderState reader_state = RD_WAIT;
uint32_t gate_message_time;
int retry_count;
int gate_id = 0;
char last_char;
void gate_reader(char c) {
  if (not isprint(c)) {
    return;
  }
  uint32_t time = millis();
  switch (reader_state) {
    case RD_WAIT:
      retry_count = 0;
      if (c >= '0' and c < '0' + 10) {
        last_char = c;
        gate_id = RD_GOAL;
        reader_state = RD_CONFIRM;
      } else if (c >= 'a' and c < 'a' + 10) {
        last_char = c;
        gate_id = RD_START;
        reader_state = RD_CONFIRM;

      } else if (c >= 'A' and c < 'A' + 10) {
        last_char = c;
        gate_id = RD_HOME;
        reader_state = RD_CONFIRM;
      }
      break;
    case RD_CONFIRM:

      if (c >= last_char && c < last_char + 6) {
        if (gate_id == RD_GOAL) {
          gate_message_time = time - 2 * (c - '0');
        } else if (gate_id == RD_START) {
          gate_message_time = time - 2 * (c - 'a');
        } else if (gate_id == RD_HOME) {
          gate_message_time = time - 2 * (c - 'A');
        } else {
          gate_message_time = time;  // this is an error
        }
        reader_state = RD_TERM;
      } else {
        if (++retry_count > 6) {
          gate_id = RD_NONE;
          reader_state = RD_WAIT;
        }
      }
    case RD_TERM:
      // if (c == '#') {
      if (gate_id == RD_HOME) {
        reader_state = RD_HOME;
      } else if (gate_id == RD_START) {
        reader_state = RD_START;
      } else if (gate_id == RD_GOAL) {
        reader_state = RD_GOAL;
      } else {
        reader_state = RD_WAIT;
      }
      break;
      // }
    case RD_HOME:
    case RD_START:
    case RD_GOAL:
      // stay in this state until acknowledged by the timer state machine
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
}

///////////////////////////////////////////////////////////////////

int old_state = 0;
void trial_machine() {
  if (resetButton.isPressed()) {
    set_state(ST_NEW_MOUSE);
    send_message(MSG_NewMouse, 0);
    send_maze_time(0);
  }

  int gate = RD_NONE;
  if (reader_state == RD_HOME) {
    gate = RD_HOME;
    reader_state = RD_WAIT;
  }

  if (reader_state == RD_START) {
    gate = RD_START;
    reader_state = RD_WAIT;
  }
  if (reader_state == RD_GOAL) {
    gate = RD_GOAL;
    reader_state = RD_WAIT;
  }

  switch (contestState) {
    case ST_NEW_MOUSE:
      mazeTimer.reset();
      runTimer.reset();
      runCount = 0;
      bestTime = UINT32_MAX;
      displayInit();
      set_state(ST_ARMED);
      showState();
      break;

    case ST_ARMED:  // robot in start cell, ready to run
      if (startButton.isPressed() || gate == RD_START) {
        bestTime = UINT32_MAX;
        if (runCount == 0) {
          send_maze_time(0);
          mazeTimer.restart();
          g_maze_start_time = millis();
        }
        send_split_time(0);
        runTimer.restart();
        runCount++;
        set_state(ST_RUNNING);
        showState();
      }
      break;
    case ST_RUNNING:  // robot on its way to the goal
      if (runTimer.time() < TIME_TRIAL_LOCKOUT) {
        break;
      }
      if (startButton.isPressed() || gate == RD_START) {
        runTimer.stop();
        g_run_time = runTimer.time();
        if (g_run_time < bestTime) {
          bestTime = g_run_time;
        }
        runCount++;
        runTimer.restart();
        send_run_time(g_run_time);
        send_split_time(0);
        set_state(ST_GOAL);
        showState();
      }
      if (armButton.isPressed()) {
        runTimer.restart();
        set_state(ST_ARMED);
        showState();
      }
      break;
    case ST_GOAL:  // transient state to record run time
      // manual timing may see the button held down for a while
      if (not startButton.isPressed()) {
        // runTimer.restart();
        // g_run_start_time = millis();
        // send_split_time(0);
        set_state(ST_RUNNING);
        showState();
      }
    default:
      break;
  }
};

///////////////////////////////////////////////////////////////////

void mazeMachine() {
  if (resetButton.isPressed()) {
    set_state(ST_NEW_MOUSE);
    send_message(MSG_NewMouse, 0);
    send_maze_time(0);
  }
  int gate = 0;
  if (reader_state == RD_HOME) {
    gate = 1;
    reader_state = RD_WAIT;
  }

  if (reader_state == RD_START) {
    gate = 2;
    reader_state = RD_WAIT;
  }
  if (reader_state == RD_GOAL) {
    gate = 3;
    reader_state = RD_WAIT;
  }

  switch (contestState) {
    case ST_NEW_MOUSE:
      mazeTimer.reset();
      runTimer.reset();
      runCount = 0;
      bestTime = UINT32_MAX;
      displayInit();
      set_state(ST_WAITING);
      showState();
      reader_state = RD_WAIT;
      gate_id = RD_NONE;
      break;
    case ST_WAITING:
      // if (armButton.isPressed() || (reader_state == RD_HOME)) {
      if (armButton.isPressed() || gate == 1) {
        if (runCount == 0) {
          send_maze_time(0);
          mazeTimer.restart();
        }
        set_state(ST_ARMED);
        showState();
        reader_state = RD_WAIT;
      }
      break;
    case ST_ARMED:  // robot in start cell, ready to run
      // if (startButton.isPressed() || reader_state != RD_NONE) {
      if (startButton.isPressed() || gate == 2) {
        send_split_time(0);
        runTimer.restart();
        runCount++;
        set_state(ST_RUNNING);
        showState();
        reader_state = RD_WAIT;
      }
      break;
    case ST_RUNNING:  // robot on its way to the goal
      // if (goalButton.isPressed() || (reader_state == RD_GOAL)) {
      if (goalButton.isPressed() || gate == 3) {
        runTimer.stop();
        uint32_t time = runTimer.time();
        send_run_time(time);
        if (time < bestTime) {
          bestTime = time;
          showTime(11, 3, bestTime);
        }
        set_state(ST_GOAL);
        showState();
        reader_state = RD_WAIT;
      }
      // if (armButton.isPressed() || (reader_state == RD_HOME)) {
      if (armButton.isPressed() || gate == 1) {
        runTimer.stop();
        runTimer.reset();
        set_state(ST_ARMED);
        showState();
        reader_state = RD_WAIT;
      }
      break;
    case ST_GOAL:
      // if (armButton.isPressed() || (reader_state == RD_HOME)) {
      if (armButton.isPressed() || gate == 1) {
        // robot is back in start cell or run is aborted
        set_state(ST_ARMED);
        showState();
        reader_state = RD_WAIT;
      }
      break;
    default:
      break;
  }
}

void radio_test(char c) {
  switch (reader_state) {
    case RD_HOME:
      Serial.println(F("HOME    "));
      lcd.setCursor(0, 1);
      lcd.print(F("HOME    "));
      reader_state = RD_WAIT;
      break;

    case RD_START:
      Serial.println(F("START    "));
      lcd.setCursor(0, 1);
      lcd.print(F("START    "));
      reader_state = RD_WAIT;
      break;
    case RD_GOAL:
      Serial.println(F("GOAL    "));
      lcd.setCursor(0, 1);
      lcd.print(F("GOAL    "));
      reader_state = RD_WAIT;
      break;
    default:
      // Serial.println("----");
      // reader_state = RD_WAIT;
      break;
  }
}

int select_contest_type() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("CONTEST TIMER V0.2"));
  lcd.setCursor(0, 1);
  lcd.print(F("   RED: RADIO TEST"));
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

  } else if (button_state == BTN_RED) {
    lcd.clear();
    lcd.print(F("RADIO TEST"));
    type = CT_RADIO;
  }
  lcd.clear();
  while (button_state != BTN_NONE) {
    delay(100);
  }
  delay(500);
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

  Serial.begin(9600);
  while (!Serial) {
    ;  // Needed for native USB port only
  }
  Serial.println(F("CONTEST_ TIMER V0.2"));

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
  char buf[32];
  Serial.println(rtc.now().tostr(buf));
  lcd.setCursor(0, 2);
  lcd.print(F("RADIO ...   "));
  radio.begin(5000);
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

  while (radio.available()) {
    radio.read();  // flush the input buffer
  }
  contestState = ST_NEW_MOUSE;
  send_message(MSG_NewMouse, 0);
}

/*********************************************** main loop ******************/
int display_phase = 0;
void loop() {
  char c;
  if (radio.available()) {
    c = radio.read();
    gate_reader(c);
  }

  if (Serial.available()) {
    char c = Serial.read();
    if (c == '>') {
      set_state(ST_NEW_MOUSE);
    }
  }
  if (button_state == (BTN_BLUE + BTN_GREEN)) {
    while (button_state != BTN_NONE) {
      // delay(10);
    }
    lcd.clear();
    reset_processor();
  }
  if (contest_type == CT_MAZE) {
    mazeMachine();
  } else if (contest_type == CT_TRIAL) {
    trial_machine();
  } else if (contest_type == CT_RADIO) {
    radio_test(c);
  } else {
    // do nothing
  }
  if (contest_type != CT_NONE && millis() > displayUpdateTime) {
    displayUpdateTime += displayUpdateInterval;
    switch (display_phase++) {
      case 0:
        lcd.setCursor(17, 0);
        lcd.print(runCount);
        break;
      case 1:
        showTime(11, 1, mazeTimer.time());
        break;
      case 2:
        showTime(11, 2, runTimer.time());
        break;
      case 3:
        if (bestTime < UINT32_MAX) {
          showTime(11, 3, bestTime);
        }
        break;
      default:
        display_phase = 0;
        break;
    }
    // display_phase++;
  }
}
