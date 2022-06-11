#include "Arduino.h"

class Timer {
 public:
  Timer(){};
  void start() {
    mRunning = true;
    mStartTime = millis();
  };
  void stop() { mRunning = false; }
  bool running() { return mRunning; }
  bool expired(uint32_t timeoutTime) {
    if (!mRunning) {
      return true;
    }
    const uint32_t elapsed = millis() - mStartTime;
    bool expired = elapsed >= timeoutTime;
    if (expired) {
      stop();
    }
    return expired;
  }

 private:
  bool mRunning = false;
  uint32_t mStartTime;
};