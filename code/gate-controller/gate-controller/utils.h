#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include "pins.h"

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


#endif