#ifndef GATESENSOR_H
#define GATESENSOR_H

#include "Arduino.h"
#include "filters.h"

class GateSensor {
 public:


  explicit GateSensor(int pin) : mSensorPin(pin) {
    mFiltered.begin(0.02);  //   n samples to 99%
    mIn.begin(0.2);  //   n samples to 99%
  }

  void update() {
    mInput = analogRead(mSensorPin);
    mFiltered.update(mInput);
    mIn.update(mInput);
    mDiff = mIn()-mFiltered();
    if(mDiff < -mThreshold){
      mInterrupted = true;
    }
  }

  void setThreshold(float threshold){

  }

  float input(){
    return mIn();
  }

  bool interrupted(){
    return mInterrupted;
  }

  void reset(){
    mInterrupted = false;
  }

  //  private:
  int mSensorPin;
  float mInput;
  float mDiff;
  ExpFilter mIn;
  ExpFilter mFiltered;
  bool mInterrupted = false;
  float mThreshold = 16.0f;
};

#endif