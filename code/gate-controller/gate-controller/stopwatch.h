/*
 * File:   stopwatch.h
 * Author: peterharrison
 * Created from http://playground.arduino.cc/Code/StopWatchClass
 * Created on 14 June 2015, 09:11
 */

#ifndef STOPWATCH_H
#define STOPWATCH_H

#include <Arduino.h>

class Stopwatch {
 public:
  enum State { STOPPED, RUNNING, RESET };
  Stopwatch();

  virtual ~Stopwatch();
  void start();
  void stop();
  void restart();
  void reset();
  bool running() { return mState == RUNNING; };
  uint32_t time();
  uint32_t lap();
  uint32_t split();

 private:
  enum State mState;
  uint32_t cyclesPerMicrosecond;
  uint32_t mStartMillis;
  uint32_t mStopMillis;
  uint32_t mTime;
  uint32_t mLapTime;
  uint32_t mSplitTime;
};

#endif /* STOPWATCH_H */
