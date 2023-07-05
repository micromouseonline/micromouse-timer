/***
 * P Harrison, 2020.
 */

#include "button.h"
#include <Arduino.h>


// The dummy handler can be used for debugging
inline void dummyHandler() {
}

/***
 * Digital pin base class
 */
BasicButton::BasicButton(uint8_t buttonPin, uint8_t activeState, Type type) {
  mPin = buttonPin;
  mType = type;
  mActiveState = activeState;
  pinMode(mPin, INPUT);
  if (mType == DIGITAL) {
    pinMode(mPin, INPUT_PULLUP);
  }
  mState = 0;
}

bool BasicButton::isActive() {
  if (mType == ANALOG) {
    bool active = analogRead(mPin) < 512;
    return (mActiveState == ACTIVE_LOW) ? active : !active;
  } else {
    return digitalRead(mPin) == mActiveState;
  }
}

void BasicButton::update() {
  uint8_t newState = INACTIVE;
  if (isActive()) {
    newState = ACTIVE;
  }
  mState = newState;
}

bool BasicButton::isPressed(void) {
  return mState == ACTIVE;
}

