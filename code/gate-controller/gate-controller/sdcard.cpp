#include "sdcard.h"


/***************************************************   SD Card */

const int chipSelect = 10;
const int cardDetect = 9;

void sdCardInit(int chipSelectPin, int cardDetectPin) {
  return;
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
  return;
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