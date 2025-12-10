# JUCE build + host wiring guide

Welcome to the desktop lane. This is the lab notebook page for anyone slinging
SeedBox inside a DAW or a headless JUCE harness. The vibe: ship something useful
fast, narrate every lever you pull, and keep the Arduino-only headers out of the
picture.

## TL;DR build flags

- Flip `-DSEEDBOX_JUCE=1` when you build. It keeps the Arduino headers on the
  bench and lights up the JUCE-specific glue.
- Leave `SEEDBOX_HW` and `SEEDBOX_SIM` at `0` in this mode — the new flag already
  routes you down the native path and the config header will scream if you try
  to combine them.

## Audio plumbing checklist

1. Instantiate `seedbox::juce_bridge::JuceHost` with your `AppState`.
2. Call `initialiseWithDefaults()` to open the default stereo output + MIDI
   pair, or `configureForTests(sampleRate, blockSize)` if you just want CI to
   exercise the wiring without touching CoreAudio/ASIO/JACK.
3. Let JUCE's `audioDeviceAboutToStart` tell us the real sample rate and block
   size. We mirror those straight into `hal::audio::configureHostStream`, so the
   engines prep against the exact numbers your host picked instead of assuming
   a hard-coded 48 kHz / 128 frame world.
4. We render directly into the host-provided float buffers via
   `hal::audio::renderHostBuffer`, clamping to stereo (if JUCE only hands us
   mono we mirror the left channel). Latency conversations get honest because we
   never resample or re-block behind JUCE's back.

## MIDI routing notes

- The JUCE backend is a first-class `MidiRouter` backend. Inbound messages ride
  a tiny queue inside `JuceMidiBackend` and flush during the audio callback so
  clock/transport stay in lockstep with your DAW.
- Outbound messages mirror the router's guard rails (channel maps, panic logic)
  and flow through `juce::MidiOutput`, so you can still demo the MN42 handshake
  story without a Teensy on the table.

## Latency + host assumptions

- We trust whatever block size the host hands us. The HAL's `framesPerBlock()`
  will match the JUCE device setting, so unit tests and tempo math can peek at
  the same truth your DAW sees.
- Sample rate comes straight from `AudioDeviceManager`. No covert resampling,
  no "maybe it's 44.1 k" hand-waving. If you need to force a rate for a demo,
  pass it into `configureForTests` and watch `hal::audio::sampleRate()` reflect
  it immediately.
- Headless CI sanity lives in `src/juce/JuceHostTest.cpp` — a JUCE UnitTest that
  boots the host shim, asserts the sample rate/block size handshake, and walks
  the sample clock forward with a fake render pass.

## Punk-rock gotchas to remember

- Keep the Teensy-only includes under `#if SEEDBOX_HW`. The JUCE build never
  pulls `HardwarePrelude` or SGTL5000 headers; the shared engines stay agnostic.
- Call `midi.poll()` somewhere in your host loop if you swap out `JuceHost` for
  custom glue — the router queues events until you explicitly flush them.
- Treat this guide like a studio diary. If you change the handshake or add a new
  host quirk, jot it down here so the next person doesn't have to guess.
