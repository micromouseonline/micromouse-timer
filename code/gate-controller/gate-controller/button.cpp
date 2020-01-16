/***
 * P Harrison, 2020.
 */

#include "Button.h"
#include <Arduino.h>

#define MAX_CLICK_TIME 300
#define DEFAULT_LONG_PRESS_TIME 1500

// The dummy handler can be used for debugging
inline void dummyHandler() {
}
/***
 * Digital pin base class
 */
Button::Button(uint8_t buttonPin, uint8_t activeState, Type type) {
  mPin = buttonPin;
  mType = type;
  mActiveState = activeState;
  pinMode(mPin, INPUT);
  if (mType == DIGITAL) {
    pinMode(mPin, INPUT_PULLUP);
  }
  mState = 0;
  mLongPressTime = DEFAULT_LONG_PRESS_TIME;
  mPressHandler = dummyHandler;
  mClickHandler = dummyHandler;
  mLongPressHandler = dummyHandler;
  triggeredHoldEvent = true;
}

bool Button::isActive() {
  if (mType == ANALOG) {
    bool active = analogRead(mPin) < 512;
    return (mActiveState == ACTIVE_LOW) ? active : !active;
  } else {
    return digitalRead(mPin) == mActiveState;
  }
}

void Button::update() {
  uint8_t newState = INACTIVE;
  if (isActive()) {
    newState = ACTIVE;
  }
  if (newState != mState) {
    if (newState == ACTIVE) {
      mPressHandler();
      mPressStartTime = millis();
      triggeredHoldEvent = false;
    } else {
      uint32_t pressTime = millis() - mPressStartTime;
      if (pressTime < MAX_CLICK_TIME) {
        mClickHandler();
      }
      mPressStartTime = 0;
    }
  } else {
    if (mPressStartTime != 0 && !triggeredHoldEvent) {
      if (millis() - mPressStartTime > mLongPressTime) {
        if (mLongPressHandler) {
          mLongPressHandler();
          triggeredHoldEvent = true;
        }
      }
    }
  }
  mState = newState;
}

bool Button::isPressed(void) {
  return mState == ACTIVE;
}

void Button::setLongPressTime(unsigned int holdTime) {
  mLongPressTime = holdTime;
}

void Button::registerPressHandler(EventHandler handler) {
  mPressHandler = handler ? handler : dummyHandler;
}

void Button::registerClickHandler(EventHandler handler) {
  mClickHandler = handler ? handler : dummyHandler;
}

void Button::registerLongPressHandler(EventHandler handler) {
  mLongPressHandler = handler ? handler : dummyHandler;
}
