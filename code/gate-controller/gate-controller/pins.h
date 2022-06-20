#ifndef PINS_H
#define PINS_H

#include <Arduino.h>

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


#endif