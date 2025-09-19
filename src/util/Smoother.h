#pragma once
inline float smooth(float in, float &z, float alpha) { z += alpha * (in - z); return z; }
