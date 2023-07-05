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

#include <SPI.h>
#include <SoftwareSerial.h>
#include "SdFat.h"
#include "sdios.h"
/// Pin Assignments

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

uint32_t displayUpdateInterval = 100;
uint32_t displayUpdateTime;
int state = 0;
uint32_t animUpdateTime = 0;
int frameCounter;
uint32_t systime;
uint32_t startTime;
// serial output steam
ArduinoOutStream cout(Serial);
/***************************************************   SD Card */

/*
 * Set DISABLE_CHIP_SELECT to disable a second SPI device.
 * For example, with the Ethernet shield, set DISABLE_CHIP_SELECT
 * to 10 to disable the Ethernet controller.
 */
const int8_t DISABLE_CHIP_SELECT = -1;
// store error strings in flash to save RAM
#define error(s) sd.errorHalt(F(s))
SdFat sd;
#define FILE_BASE_NAME "Data"
const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
char fileName[13] = FILE_BASE_NAME "00.csv";
uint32_t logTime;
int logLines = 0;
// Log file.
SdFile logFile;
const uint8_t ANALOG_COUNT = 4;

//------------------------------------------------------------------------------
/*
 * date/time values for debug
 * normally supplied by a real-time clock or GPS
 */
// date 1-Oct-14
uint16_t year = 2014;
uint8_t month = 10;
uint8_t day = 1;

// time 20:30:40
uint8_t hour = 20;
uint8_t minute = 30;
uint8_t second = 40;
//------------------------------------------------------------------------------
/*
 * User provided date time callback function.
 * See SdFile::dateTimeCallback() for usage.
 */
void dateTime(uint16_t* date, uint16_t* time) {
  // User gets date and time from GPS or real-time
  // clock in real callback function

  // return date using FAT_DATE macro to format fields
  *date = FAT_DATE(year, month, day);

  // return time using FAT_TIME macro to format fields
  *time = FAT_TIME(hour, minute, second);
}
//------------------------------------------------------------------------------
/*
 * Function to print all timestamps.
 */
void printTimestamps(SdFile& f) {
  dir_t d;
  if (!f.dirEntry(&d)) {
    error("f.dirEntry failed");
  }

  cout << F("Creation: ");
  f.printFatDate(d.creationDate);
  cout << ' ';
  f.printFatTime(d.creationTime);
  cout << endl;

  cout << F("Modify: ");
  f.printFatDate(d.lastWriteDate);
  cout << ' ';
  f.printFatTime(d.lastWriteTime);
  cout << endl;

  cout << F("Access: ");
  f.printFatDate(d.lastAccessDate);
  cout << ' ';
  f.printFatTime(d.lastWriteTime);
  cout << endl;
}
//------------------------------------------------------------------------------
void dateTest() {
  SdFile file;
  // remove files if they exist
  cout << F("Removing old files\n");
  sd.remove("callback.txt");
  sd.remove("default.txt");
  sd.remove("stamp.txt");
  // create a new file with default timestamps
  if (!file.open("default.txt", O_WRONLY | O_CREAT)) {
    error("open default.txt failed");
  }
  cout << F("\nOpen with default times\n");
  printTimestamps(file);

  // close file
  file.close();
  /*
   * Test the date time callback function.
   *
   * dateTimeCallback() sets the function
   * that is called when a file is created
   * or when a file's directory entry is
   * modified by sync().
   *
   * The callback can be disabled by the call
   * SdFile::dateTimeCallbackCancel()
   */
  // set date time callback function
  SdFile::dateTimeCallback(dateTime);

  // create a new file with callback timestamps
  if (!file.open("callback.txt", O_WRONLY | O_CREAT)) {
    error("open callback.txt failed");
  }
  cout << ("\nOpen with callback times\n");
  printTimestamps(file);

  // change call back date
  year += 1;
  day += 1;
  // must add two to see change since FAT second field is 5-bits
  second += 2;

  // modify file by writing a byte
  file.write('t');

  // force dir update
  file.sync();

  cout << F("\nTimes after write\n");
  printTimestamps(file);

  // close file
  file.close();
  /*
   * Test timestamp() function
   *
   * Cancel callback so sync will not
   * change access/modify timestamp
   */
  SdFile::dateTimeCallbackCancel();

  // create a new file with default timestamps
  if (!file.open("stamp.txt", O_WRONLY | O_CREAT)) {
    error("open stamp.txt failed");
  }
  // set creation date time
  if (!file.timestamp(T_CREATE, 2014, 11, 10, 1, 2, 3)) {
    error("set create time failed");
  }
  // set write/modification date time
  if (!file.timestamp(T_WRITE, 2014, 11, 11, 4, 5, 6)) {
    error("set write time failed");
  }
  // set access date
  if (!file.timestamp(T_ACCESS, 2014, 11, 12, 7, 8, 9)) {
    error("set access time failed");
  }
  cout << F("\nTimes after timestamp() calls\n");
  printTimestamps(file);

  file.close();
  cout << F("\ndateTest() done\n");
  return;
}
//------------------------------------------------------------------------------

// global for card size
uint32_t cardSize;

// global for card erase size
uint32_t eraseSize;
//------------------------------------------------------------------------------
// store error strings in flash
#define sdErrorMsg(msg) sd.errorPrint(F(msg));
//------------------------------------------------------------------------------
uint8_t cidDump() {
  cid_t cid;
  if (!sd.card()->readCID(&cid)) {
    sdErrorMsg("readCID failed");
    return false;
  }
  cout << F("\nManufacturer ID: ");
  cout << hex << int(cid.mid) << dec << endl;
  cout << F("OEM ID: ") << cid.oid[0] << cid.oid[1] << endl;
  cout << F("Product: ");
  for (uint8_t i = 0; i < 5; i++) {
    cout << cid.pnm[i];
  }
  cout << F("\nVersion: ");
  cout << int(cid.prv_n) << '.' << int(cid.prv_m) << endl;
  cout << F("Serial number: ") << hex << cid.psn << dec << endl;
  cout << F("Manufacturing date: ");
  cout << int(cid.mdt_month) << '/';
  cout << (2000 + cid.mdt_year_low + 10 * cid.mdt_year_high) << endl;
  cout << endl;
  return true;
}

/***************************************************   SD Card End */

/******************************************************** SETUP *****/
int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
void setup() {
  pinMode(SD_SELECT, OUTPUT);
  pinMode(SD_DETECT, INPUT_PULLUP);
  pinMode(BUTTON_RESET_BACK, INPUT_PULLUP);
  pinMode(BUTTON_TOUCH_OK, INPUT_PULLUP);
  pinMode(BUTTON_GOAL_DN, INPUT_PULLUP);
  pinMode(BUTTON_START_UP, INPUT_PULLUP);

  Serial.begin(57600);
  while (!Serial) {
    SysCall::yield();
  }

  // use uppercase in hex and use 0X base prefix
  // cout << ios::uppercase << showbase << endl;
  // cout. flags(ios::uppercase);
  cout << uppercase << showbase << endl;

  // F stores strings in flash to save RAM
  cout << F("SdFat version: ") << SD_FAT_VERSION << endl;
  sdInit();
  dateTest();
  SdFile::dateTimeCallback(dateTime);
  radio.begin(10000);
  Serial.print(F("-SRAM left="));
  Serial.println(freeRam());
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

//------------------------------------------------------------------------------
// Write data header.
void writeHeader(SdFile & file) {
  file.print(F("micros"));
  for (uint8_t i = 0; i < ANALOG_COUNT; i++) {
    file.print(F(",adc"));
    file.print(i, DEC);
  }
  file.println();
}
//------------------------------------------------------------------------------
// Log a data record.
void logData(SdFile & file) {
  uint16_t data[ANALOG_COUNT];

  // Read all channels to avoid SD write latency between readings.
  for (uint8_t i = 0; i < ANALOG_COUNT; i++) {
    data[i] = analogRead(i);
  }
  // Write data to file.  Start with log time in micros.
  file.print(logTime);

  // Write ADC data to CSV record.
  for (uint8_t i = 0; i < ANALOG_COUNT; i++) {
    file.write(',');
    file.print(data[i]);
  }
  file.println();
}

void createFile() {
  // Find an unused file name.
  if (BASE_NAME_SIZE > 6) {
    error("FILE_BASE_NAME too long");
  }
  while (sd.exists(fileName)) {
    if (fileName[BASE_NAME_SIZE + 1] != '9') {
      fileName[BASE_NAME_SIZE + 1]++;
    } else if (fileName[BASE_NAME_SIZE] != '9') {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
    } else {
      error("Can't create file name");
    }
  }
  if (!logFile.open(fileName, O_WRONLY | O_CREAT | O_EXCL)) {
    error("file.open");
  }
  // Read any Serial data.
  do {
    delay(10);
  } while (Serial.available() && Serial.read() >= 0);

  Serial.print(F("Logging to: "));
  Serial.println(fileName);
  Serial.println(F("Type any character to stop"));
}

void rootDir() {
  SdFile root;
  SdFile file;
  // SdFile file;
  if (!root.open("/")) {
    sd.errorHalt("open root failed");
  }
  int fileCount = 0;
  // Open next file in root.
  // Warning, openNext starts at the current directory position
  // so a rewind of the directory may be required.
  while (file.openNext(&root, O_RDONLY)) {
    fileCount++;
    file.printFileSize(&Serial);
    Serial.write(' ');
    file.printModifyDateTime(&Serial);
    Serial.write(' ');
    file.printName(&Serial);
    if (file.isDir()) {
      // Indicate a directory.
      Serial.write('/');
    }
    Serial.println();
    file.close();
  }
  if (root.getError()) {
    Serial.println("openNext failed");
  }
  cout << F("\nTotal ") << fileCount << endl;
  root.close();
}
/*********************************************** SD CARD ******************/
void sdInit() {
  uint32_t t = millis();
  Serial.print(F("Card Initialising ..."));
  // Initialize at the highest speed supported by the board that is
  // not over 50 MHz. Try a lower speed if SPI errors occur.
  if (!sd.cardBegin(SD_SELECT, SD_SCK_MHZ(50))) {
    sdErrorMsg("cardBegin failed");
    return;
  }
  t = millis() - t;
  cardSize = sd.card()->cardSize();
  if (cardSize == 0) {
    sdErrorMsg("cardSize failed");
    return;
  }
  // cout << F("\ninit time: ") << t << " ms" << endl;
  cout << F(" Card type: ");
  switch (sd.card()->type()) {
    case SD_CARD_TYPE_SD1:
      cout << F("SD1");
      break;

    case SD_CARD_TYPE_SD2:
      cout << F("SD2");
      break;

    case SD_CARD_TYPE_SDHC:
      if (cardSize < 70000000) {
        cout << F("SDHC");
      } else {
        cout << F("SDXC");
      }
      break;

    default:
      cout << F("Unknown");
  }
  Serial.print(F(" ... "));

  if (!sd.fsBegin()) {
    sdErrorMsg("\nFile System initialization failed.\n");
    return;
  }
  Serial.println(F("File system initialised"));
  // cidDump();
  sd.ls(LS_R | LS_DATE | LS_SIZE);
  createFile();
  rootDir();
}

/*********************************************** main loop ******************/
void loop() {  // run over and over
  static bool logging = true;

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
    if (!logging) {
      return;
    }
    logLines++;
    cout << logLines << ',';
    if (logLines > 10) {
      logFile.close();
      Serial.println(F("Finished"));
      logging = false;
    } else {
      logData(logFile);
      // Force data to SD and update the directory entry to avoid data loss.
      if (!logFile.sync() || logFile.getWriteError()) {
        error("write error");
      }
    }
  }
}
