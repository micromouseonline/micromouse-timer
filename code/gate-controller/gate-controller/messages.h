#include <Arduino.h>

// clang-format off
// clang-format does not know about .ino files?
/***
 Author: David Hannaford & Ian Butterworth  Date: 25 September 2017  Version: 1.1

 Arduino and Visual Basic: Test code for mouse timing system
 Timing performed by Arduino and data passed to VisualBasic PC software
 Overall supervision performed by VisualBasic and passed to the Arduino

 Messages in the format <message_type,value>CrLf
 Supported message types:
    VAL  TYPE        DIR  TX FREQENCY   COMMENTS
    1    MazeTime    OUT  100 msec      time in 100 millisecond units that the current mouse has been
                                        active in the maze
    2    SplitTime   OUT  10 msec       time in 10 millisecond units for the current mouse on its
                                        current run (only used to display current run time)
    3    RunTime     OUT  Event         time in 10 millisecond units for a run that has just completed
                                        (definitive time used to calculate score time)
    4    Stateval    OUT                Value of timer state (0 to 5)
                                              0 calibrate gates,
                                              1 looking for mouse in start cell,
                                              2 Mouse seen in start cell,
                                              3 Run started (but not cleared start gate yet)
                                              4 Run in progress,
                                              5 Run to centre completed (finish gete triggered)
    5    RunTimeMs   OUT  Event         time in 1 millisecond units for a run that has just completed
                                        (definitive time used to calculate score time)
    6    SplitToRun  OUT  Event         Asks the supervisor to use the current split time as a run time
                                        - supervisor timing (value argument will always be passed as 0)
    71   STrigger    OUT  Event         New value of Start Gate trigger (Valid values: ON, OFF)
    72   FTrigger    OUT  Event         New value of Finish Gate trigger (Valid values: ON, OFF)
    73   CTrigger    OUT  Event         New value of Mouse in Start Cell trigger (Valid values: ON, OFF
    81   SGLevel     OUT  100 msec      Intensity level being received by Start Gate phototransistor
    82   SGPot       OUT  100 msec      Value read from Start Gate potentiometer
    83   FGLevel     OUT  100 msec      Intensity level being received by Finish Gate phototransistor
    84   FGPot       OUT  100 msec      Value read from Finish Gate potentiometer
    85   SCLevel     OUT  100 msec      Intensity level being received by Mouse in Start Cell phototransistor
    86   SCPot       OUT  100 msec      Value read from Mouse in Start Cell potentiometer
    98   NewMouse     IN  Event         A new mouse has been selected in the windows supervision application
                                        (value argument will always be passed as 0)
    99   SetMode      IN  Event         Controls the Arduino mode
                                        Valid values:
                                              TIMER (normal timing mode),
                                              CALIBRATION (start returning calibration data)
*/
// clang-format on

#define MSG_MAZE_TIME 1
#define MSG_SPLIT_TIME 2
#define MSG_RUN_TIME 3
#define MSG_STATE_VAL 4
#define MSG_RUN_TIME_MS 5
#define MSG_SPLIT_TO_RUN 6
#define MSG_F_TRIGGER 72
#define MSG_S_TRIGGER 71
#define MSG_C_TRIGGER 73
#define MSG_SG_LEVEL 81
#define MSG_SG_POT 82
#define MSG_FG_LEVEL 83
#define MSG_G_POT 84
#define MSG_SC_LEVEL 85
#define MSG_SC_POT 86
#define MSG_NEW_MOUSE 98
#define MSG_SET_MODE 99

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

inline void sendMessage(int type, unsigned long value) {
  // Serial.print(msg[type]);
  Serial.print('<');
  Serial.print(type);
  Serial.print(',');
  Serial.print(value);
  Serial.print('>');
  Serial.println();
}
