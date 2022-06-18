#pragma once

/***
 * Timer from https://github.com/kerikun11/m5stack-micromouse-timer
 */

#include "Arduino.h"
struct Window {
  struct Track {
    int count;
    int time_ms;
    bool valid;
  };
  struct Contents {
    int running_time_ms = 0;
    int remain_time_ms = 0;
    int try_count = 0;
    Track tracks[10];
  };
  void update(const Contents &contents, const bool clean = false) {
    // draw times
  }
  void begin() {}
};

class KeriTimer {
 public:
  enum State {
    IDLE,   //< before begining
    BEGIN,  //< waiting for mouse started, time control started
    START,  //< between start and goal, searching for goal
    GOAL,   //< reached goal, searching additionally
  };

 public:
  KeriTimer() {}
  void init() {
    w.begin();
    // s.begin();
  }
  void update() {
    /* update running time */
    if (start_time_ms > 0)
      wc.running_time_ms = millis() - start_time_ms;
    /* update remaining time */
    wc.remain_time_ms = competition_limit_time_ms + begin_time_ms - millis();
    if (prev_remain_time_ms > 0 && wc.remain_time_ms <= 0) {
      prev_remain_time_ms = wc.remain_time_ms;
      begin_time_ms -= 1000;  //< correction of what is actually displayed
    }
    /* draw time */
    w.update(wc);
  }
  void begin() {
    // s.play(Sound::Music::MUSIC_BEGIN);
    begin_time_ms = millis();
    start_time_ms = -1;
    wc.try_count = 1;
    state = BEGIN;
  }
  void start_sensor() {
    switch (state) {
      case BEGIN:
      case START:
        // s.play(Sound::Music::MUSIC_START);
        start_time_ms = millis();
        state = START;
        break;
      case GOAL:
        try_increment();
        start_time_ms = millis();
        state = START;
        break;
      default:
        break;
    }
  }
  void goal_sensor() {
    switch (state) {
      case BEGIN:
        if (start_time_ms < 0) {
          // s.play(Sound::Music::MUSIC_EMERGENCY);
          break;
        }
      case START:
        // s.play(Sound::Music::MUSIC_GOAL);
        wc.tracks[wc.try_count] = {wc.try_count, (int)millis() - start_time_ms,
                                   wc.remain_time_ms > 0};
        start_time_ms = -1;
        state = GOAL;
        break;
      default:
        break;
    }
  }
  void touch() {
    switch (state) {
      case BEGIN:
      /***
       * cancel current run
       */
        // wc.tracks.push_back({wc.try_count, -1,0});
        try_increment();
        break;
      case START:
        start_time_ms = -1;
        wc.running_time_ms = 0;
        break;
      case GOAL:
        try_increment();
        break;
      default:
        break;
    }
    state = BEGIN;
  }

  /***
   * undo the last run
   */
  void erase() {
    if (wc.try_count == 1)
      return;
    // wc.tracks.pop_back(); // remove the entry
    wc.try_count--;
    wc.running_time_ms = 0;
    start_time_ms = -1;
  }

  /***
   * Alow the user to select the contest time limit
   */
  void time_select() {
    /* draw time */
    wc.remain_time_ms = competition_limit_time_ms + 501;  //< + 501: to show colon
    w.update(wc, true);
    /* draw button label */
    // M5.Lcd.setTextFont(4);
    // M5.Lcd.setTextSize(2);
    // M5.Lcd.setTextColor(TFT_MAGENTA);
    // M5.Lcd.setTextDatum(CC_DATUM);
    // M5.Lcd.drawString("-", M5.Lcd.width() / 2 - 90, M5.Lcd.height() - 24);
    // M5.Lcd.drawString("+", M5.Lcd.width() / 2 + 96, M5.Lcd.height() - 24);
    // M5.Lcd.setTextSize(1);
    // M5.Lcd.drawString("begin", M5.Lcd.width() / 2, M5.Lcd.height() - 24);
    /* time select */
    while (1) {
      yield();
      // M5.update();
      w.update(wc);
      /* decrement */
      // if (M5.BtnA.wasPressed()) {
      //   competition_limit_time_ms -= 1 * 60 * 1000;
      //   if (competition_limit_time_ms < 0)
      //     competition_limit_time_ms = 0;
      //   wc.remain_time_ms = competition_limit_time_ms + 501;  //< + 501: to show colon
      // }
      /* confirm */
      // if (M5.BtnB.wasPressed())
      //   break;
      /* increment */
      // if (M5.BtnC.wasPressed()) {
      //   competition_limit_time_ms += 1 * 60 * 1000;
      //   if (competition_limit_time_ms >= 60 * 60 * 1000)
      //     competition_limit_time_ms = 59 * 60 * 1000;
      //   wc.remain_time_ms = competition_limit_time_ms + 501;  //< + 501: to show colon
      // }
    }
  }

 private:
  Window w;
  Window::Contents wc;
  State state = IDLE;
  long int competition_limit_time_ms = 5 * 60 * 1000L;
  int begin_time_ms = -1;
  int start_time_ms = -1;
  long int prev_remain_time_ms = 100 * 60 * 1000L;

  void try_increment() {
    start_time_ms = -1;
    wc.running_time_ms = 0;
    wc.try_count++;
  }
};