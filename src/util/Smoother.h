#pragma once

//
// Single-pole leaky integrator.
// -----------------------------
// `smooth` is the canonical "teach a filter in five lines" example.  Feed it the
// raw control value (`in`), park the running state in `z`, and pick an `alpha`
// between 0 (no movement) and 1 (follow immediately).  Because the state lives
// outside the function students can watch how different alpha values lag inputs
// in real time — great for oscilloscope demos or serial plots.
inline float smooth(float in, float &z, float alpha) {
  z += alpha * (in - z);
  return z;
}

// One-pole smoother with explicit state. Call `process` once per control tick.
struct OnePoleSmoother {
  float state{0.0f};
  float alpha{0.15f};

  void reset(float value) { state = value; }

  void setAlpha(float value) {
    alpha = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
  }

  float process(float target) {
    state += alpha * (target - state);
    return state;
  }
};

// Slew limiter with separate rise/fall steps (units per control tick).
struct SlewLimiter {
  float state{0.0f};
  float riseStep{0.01f};
  float fallStep{0.02f};

  void reset(float value) { state = value; }

  void setSteps(float rise, float fall) {
    riseStep = rise < 0.0f ? 0.0f : rise;
    fallStep = fall < 0.0f ? 0.0f : fall;
  }

  float process(float target) {
    const float delta = target - state;
    if (delta > riseStep) {
      state += riseStep;
    } else if (delta < -fallStep) {
      state -= fallStep;
    } else {
      state = target;
    }
    return state;
  }
};
