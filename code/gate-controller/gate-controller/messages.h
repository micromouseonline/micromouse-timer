#include <Arduino.h>

// clang-format off
/***
Author: David Hannaford & Ian Butterworth  Date: 25 September 2017  Version: 6
Modified: Peter Harrison                   Date:  7 June      2022  Version: 7.0
Arduino and Visual Basic: Test code for mouse timing system
Timing performed by Arduino and data passed to VisualBasic PC software
Overall supervision performed by VisualBasic and passed to the Arduino
Messages in the format <message_type,value>CrLf
Implemented message types:
   ENCODED  MESSAGE TYPE      DIRECTION      TX FREQENCY     COMMENTS
   98       MSG_NewMouse      PC to Arduino  Event Driven    A new mouse has been selected in the host application 
                                                             (value argument will always be passed as 0)
   4        MSG_CURRENT_STATE Arduino to PC                  Value of timer state (0 to 5) 
                                                                  0 calibrate gates, 
                                                                  1 looking for mouse in start cell,
                                                                  2 Mouse seen in start cell, 
                                                                  3 Run started (but not cleared start gate yet)
                                                                  4 Run in progress, 
                                                                  5 Run to centre completed (finish gete triggered)
                                                                  6 New Mouse
 
   12       MSG_C1SplitTime   Arduino to PC  Event Driven    Time in milliseconds for the current mouse on its current run 
                                                             (only sent as zero to start host counter)
   13       MSG_C1RunTime     Arduino to PC  Event Driven    Time in milliseconds for a run that has just completed 
                                                             (definitive time used to calculate score time - sent twice)
   30       MSG_CourseTimeMs  Arduino to PC  Event Driven    Time in milliseconds that the current mouse has been active 
                                                             in the maze (only sent as zero to reset host counter)

   71       MSG_STrigger      Arduino to PC  Event Driven    New value of Start Gate trigger (Valid values: 1, 0)
   72       MSG_FTrigger      Arduino to PC  Event Driven    New value of Finish Gate trigger (Valid values: 1, 0)
   73       MSG_CTrigger      Arduino to PC  Event Driven    New value of Mouse in Start Cell trigger (Valid values: 1,0)

   81       MSG_SGLevel       Arduino to PC  100 msec        Intensity level being received by Start Gate phototransistor
   82       MSG_SGPot         Arduino to PC  100 msec        Value read from Start Gate potentiometer
   83       MSG_FGLevel       Arduino to PC  100 msec        Intensity level being received by Finish Gate phototransistor
   84       MSG_FGPot         Arduino to PC  100 msec        Value read from Finish Gate potentiometer
   85       MSG_SCLevel       Arduino to PC  100 msec        Intensity level being received by Mouse in Start Cell phototransistor
   86       MSG_SCPot         Arduino to PC  100 msec        Value read from Mouse in Start Cell potentiometer

   99       MSG_SetMode       PC to Arduino  Event Driven    Controls the Arduino mode 
                                                             Valid values: 
                                                                  TIMER       (normal timing mode), 
                                                                  CALIBRATION (start returning calibration data)

***/
// clang-format on


// Message Valid Values
// clang-format off
const int MSG_NewMouse       = 98;
const int MSG_CURRENT_STATE  =  4;
const int MSG_C1SplitTime    = 12;
const int MSG_C1RunTime      = 13;
const int MSG_CourseTimeMs   = 30;

const int MSG_SetMode        = 99;  // not implemented

const int MSG_SGLevel        = 81;
const int MSG_SGPot          = 82;
const int MSG_FGLevel        = 83;
const int MSG_FGPot          = 84;
const int MSG_SCLevel        = 85;
const int MSG_SCPot          = 86;
const int MSG_STrigger       = 71;
const int MSG_FTrigger       = 72;
const int MSG_CTrigger       = 73;
// clang-format on

// clang-format off
const char* const msg[] PROGMEM = {
    "  none             ",
    "  MSG_MAZE_TIME    ",
    "  MSG_SPLIT_TIME   ",
    "  MSG_RUN_TIME     ",
    "  MSG_STATE_VAL    ",
    "  MSG_RUN_TIME_MS  ",
    "  MSG_SPLIT_TO_RUN ",
};
// clang-format on

inline void send_message(int type, unsigned long value) {
  // Serial.print(msg[type]);
  Serial.print('<');
  Serial.print(type);
  Serial.print(',');
  Serial.print(value);
  Serial.print('>');
  Serial.println();
}

void send_run_time(unsigned long time) {
  send_message(MSG_C1RunTime, time);
  delay(20);
  send_message(MSG_C1RunTime, time);
}

void send_maze_time(unsigned long time) {
  send_message(MSG_CourseTimeMs, time);
}

void send_split_time(unsigned long time) {
  send_message(MSG_C1SplitTime, time);
}