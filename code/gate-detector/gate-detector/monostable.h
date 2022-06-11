
#ifndef MONOSTABLE_H
#define MONOSTABLE_H

class MonoStable {
 public:
  explicit MonoStable(int timeout) { duration = timeout; }

  void start() {
    if (time <= -duration) {
      time = duration;
    }
  }

  void setTime(int timeout) { duration = timeout; }

  void update() {
    if (time > -duration) {
      time--;
    }
  }

  bool state() { return time > 0; }

 private:
  MonoStable(){};
  int duration = 0;
  int time = 0;
};


#endif