#pragma once
#include "Seed.h"
#include <array>
#include <cstdint>

#ifdef SEEDBOX_HW
#include <Audio.h>
#include <memory>
#include <vector>
#endif

// Sampler owns a deterministic voice pool shared between hardware and the
// native simulator. The intent: **any seed rendered in rehearsal behaves the
// same on stage**. This header is deliberately chatty so the DSP class can be
// taught straight from the source code.
//
// Major ideas baked into this sampler:
// - "Voice" means one currently playing sample. We only keep four to make it
//   painfully obvious when voice stealing happens.
// - Every attribute comes from the Seed genome (`Seed::env*`, `Seed::tone`,
//   etc.), so deterministic reseeding is as easy as re-running the scheduler.
// - Hardware and native builds share the same bookkeeping. The hardware build
//   wires a Teensy Audio graph; the native build keeps the data-only shell so
//   unit tests and notebooks can inspect state without an SGTL5000 nearby.
class Sampler {
public:
  static constexpr uint8_t kMaxVoices = 4;

  struct VoiceState {
    // Is the slot currently reserved by a voice? Determined by sampler logic,
    // not the downstream audio nodes.
    bool active{false};
    // Monotonic handle that bumps every trigger. Helpful for diffing state
    // captures in tests.
    uint32_t handle{0};
    // Sample-accurate time (relative to audio callback zero) that this voice
    // was scheduled to launch. The scheduler owns that timeline.
    uint32_t startSample{0};
    // Which entry in the seed sample table do we intend to play?
    uint8_t sampleIndex{0};
    // Multiplicative playback rate derived from the seed's pitch (semitones).
    float playbackRate{1.0f};
    struct {
      // All envelope times live in seconds; we convert to milliseconds on the
      // Teensy side because the Audio library expects that unit.
      float attack{0.0f};
      float decay{0.0f};
      float sustain{1.0f};
      float release{0.0f};
    } envelope;
    // "Tilt" tone control — 0.0f is dark, 1.0f is bright.
    float tone{0.5f};
    // 0.0f = mono, 1.0f = hard left/right pan. We store the raw 0-1 and the
    // derived gains for clarity when inspecting state dumps.
    float spread{0.0f};
    // true => voice is streaming from SD instead of the RAM preload bucket.
    bool usesSdStreaming{false};
    // Calculated via constant-power panning so center position stays loud.
    float leftGain{0.0f};
    float rightGain{0.0f};
  };

  // Reset the voice pool and (on hardware) spin up the Teensy Audio graph.
  void init();
  // Reserve a voice for the provided seed and schedule it to launch at
  // `whenSamples`. All pitch/envelope/tone settings get baked into the voice
  // record immediately.
  void trigger(const Seed& s, uint32_t whenSamples);

  // Count how many voices are flagged active. Handy for UI + tests.
  uint8_t activeVoices() const;
  // Introspect a specific voice slot. Out-of-range requests return a default
  // constructed VoiceState.
  VoiceState voice(uint8_t index) const;

private:
  struct VoiceInternal {
    // These fields mirror VoiceState but keep their raw values for internal
    // math. Any change here should bubble out via `voice()` so tests stay in
    // sync with the hardware paths.
    bool active{false};
    uint32_t handle{0};
    uint32_t startSample{0};
    uint8_t sampleIndex{0};
    float playbackRate{1.0f};
    float envA{0.0f};
    float envD{0.0f};
    float envS{1.0f};
    float envR{0.0f};
    float tone{0.5f};
    float spread{0.0f};
    bool usesSdStreaming{false};
    float leftGain{0.0f};
    float rightGain{0.0f};
  };

  uint8_t allocateVoice(uint32_t whenSamples);
  void configureVoice(VoiceInternal& voice, uint8_t index, const Seed& seed, uint32_t whenSamples);
  static float pitchToPlaybackRate(float semitones);
  static float clamp01(float value);

private:
  std::array<VoiceInternal, kMaxVoices> voices_{};
  uint32_t nextHandle_{1};

#ifdef SEEDBOX_HW
  struct HardwareVoice {
    // RAM-resident sample player (Teensy Audio library).
    AudioPlayMemory ramPlayer;
    // SD-card streaming player. Both RAM + SD feed the same mixer with mute
    // gains so we can swap sources per voice.
    AudioPlaySdWav sdPlayer;
    AudioMixer4 sourceMixer;
    // Teensy ADSR envelope. We drive it with Seed-provided values.
    AudioEffectEnvelope envelope;
    // Simple tone control (tilt EQ stand-in).
    AudioFilterStateVariable toneFilter;
  };

  std::array<HardwareVoice, kMaxVoices> hwVoices_{};
  AudioMixer4 voiceMixerLeft_;
  AudioMixer4 voiceMixerRight_;
  AudioOutputI2S output_;
  std::vector<std::unique_ptr<AudioConnection>> patchCables_;
#endif
};
