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
