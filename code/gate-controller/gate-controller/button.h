#ifndef Button_h
#define Button_h

#include <Arduino.h>

/***
 * Not all analogue pins can be read as digital inputs.
 * On the arduino Nano, these are ADC6 and ADC7 which are
 * not connected to digital ports.
 *
 * Consequently, they must be read as analogue values and
 * then rounded up or down to digital equivalents.
 *
 *
 *
 */

typedef void (*EventHandler)();


class Button {
 public:
  enum ActiveState { ACTIVE_LOW = 0, ACTIVE_HIGH = 1 };
  enum State { INACTIVE = 0, ACTIVE = 1 };
  enum Type { DIGITAL = 0, ANALOG = 1};

  Button(uint8_t buttonPin, uint8_t buttonMode = ACTIVE_LOW, Type type = DIGITAL);

  void pullup(uint8_t buttonMode);
  void pulldown();
  void update();
  bool isPressed();
  bool isActive();

  void setLongPressTime(unsigned int holdTime);

  void registerPressHandler(EventHandler handler);
  void registerClickHandler(EventHandler handler);
  void registerLongPressHandler(EventHandler handler);


 private:
  uint8_t mPin;
  uint8_t mActiveState;
  volatile uint8_t mState;
  Type mType;
  uint32_t mPressStartTime;
  uint32_t mLongPressTime;
  EventHandler mPressHandler;
  EventHandler mClickHandler;
  EventHandler mLongPressHandler;
  bool triggeredHoldEvent;
};


#endif
