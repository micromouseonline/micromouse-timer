
#ifndef FILTERS_H
#define FILTERS_H


class ExpFilter {
 public:
  ExpFilter(){};

  explicit ExpFilter(float alpha, float value = 0.0f) { begin(alpha, value); };

  void begin(float alpha, float value = 0) {
    mAlpha = alpha;
    mValue = value;
  }

  float update(float newValue) {
    mValue += mAlpha * (newValue - mValue);
    return mValue;
  };

  float value() { return mValue; }

  float operator()() { return mValue; }

 private:
  float mAlpha = 1.0f;
  float mValue = 0;
};


#endif