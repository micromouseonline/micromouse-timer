/*
 * File:   stopwatch.cpp
 * Author: peterharrison
 *
 * Created on 14 June 2015, 09:11
 */

#include "stopwatch.h"
#include <Arduino.h>
Stopwatch::Stopwatch() : mState(RESET), mTime(0) {
  reset();
}

Stopwatch::~Stopwatch() = default;


void Stopwatch::start() {
  if (mState != Stopwatch::RUNNING) {
    mState = Stopwatch::RUNNING;
    reset();
  }
};

void Stopwatch::stop() {
  if (mState == Stopwatch::RUNNING) {
    mState = Stopwatch::STOPPED;
  }
  // reset();
};

void Stopwatch::restart() {
  reset();
  mState = Stopwatch::RUNNING;
}

uint32_t Stopwatch::time() {
  if (mState == Stopwatch::RUNNING) {
    mStopMillis = millis();
  }
  mTime = (mStopMillis - mStartMillis) ;
  return mTime;
}

/**
 *
 * @return lap time in microseconds, reset timer
 */
uint32_t Stopwatch::lap() {
  if (mState == Stopwatch::RUNNING) {
    mStopMillis = millis();
    mLapTime = (mStopMillis - mStartMillis) ;
    mStartMillis = mStopMillis;
  }
  return mLapTime;
}

uint32_t Stopwatch::split() {
  if (mState == Stopwatch::RUNNING) {
    mStopMillis = millis();
    mSplitTime = (mStopMillis - mStartMillis) ;
  }
  return mSplitTime;
}

void Stopwatch::reset() {
  mSplitTime = 0;
  mLapTime = 0;
  mStartMillis = millis();
  mStopMillis = mStartMillis;
  mState = RESET;
};
