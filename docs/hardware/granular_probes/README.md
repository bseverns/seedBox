# Granular bench probes (Teensy 4.0)

Hardware sandboxes are loud, messy, and perfect for proving that our planning docs
actually match the copper traces. This bench note keeps the granular roadmap honest
by turning the "check later" items into executable rituals.

## Quick wins (automated)

1. Flash the PlatformIO unit test bundle to a Teensy 4.0:
   ```sh
   pio test -e teensy40 --filter test_hardware
   ```
   * `test_teensy_granular_effect_traits` statically verifies that the stock
     `AudioEffectGranular` exposes `beginPitchShift()` (and *not*
     `setGrainLength()`), so our fallback wiring keeps compiling as Teensy
     releases evolve.
   * `test_teensy_granular_assigns_dsp_handles` exercises `GranularEngine::init`
     on real silicon, proving that every voice slot maps to a unique DSP handle
     before any grains start flying.
   * `test_teensy_granular_triggers_span_mixer_fanout` drives a dozen seeds
     across the hardware mixer lattice to ensure we actually patch beyond the
     first four-voice group. If the wiring ever regresses, this test is the first
     siren.

2. Capture the UART log while the tests run. Unity spits out a PASS/FAIL banner
   that you can tuck into bug reports or design notes.

## Manual pokes (semi-automated)

Once the automated tests are green, plug in headphones (or loop the line-outs
into your interface) and run this minimal sketch:

```cpp
#include <Audio.h>
#include "engine/Granular.h"

AudioControlSGTL5000 audioShield;
GranularEngine engine;

void setup() {
  AudioMemory(160);
  audioShield.enable();
  audioShield.volume(0.4f);
  engine.init(GranularEngine::Mode::kHardware);
  engine.setMaxActiveVoices(12);
  engine.registerSdClip(1, "kick.raw");
  engine.registerSdClip(2, "snare.raw");
}

void loop() {
  static uint8_t id = 1;
  Seed seed{};
  seed.id = id++;
  seed.pitch = 0.0f;
  seed.granular.stereoSpread = 0.5f;
  seed.granular.source = static_cast<uint8_t>(GranularEngine::Source::kSdClip);
  seed.granular.sdSlot = 1 + (id % 2);
  seed.granular.grainSizeMs = 80.0f;
  engine.trigger(seed, AudioStream::samples_per_msec(10) * id);
  delay(200);
}
```

Listen for grains hopping across the stereo field. If the right channel fades
once you spawn the fifth grain, the mixer fan-out needs attention.

## Reporting

Screenshot the Unity output (or copy the serial log) and stash it in the
`docs/hardware/granular_probes` folder when you uncover quirks. Treat it like a
lab notebook—scrappy notes encouraged, but make the reproduction steps explicit
so the next hacker can follow the breadcrumbs.

## Serial HUD logger

Need to watch the granular density without hunching over the OLED? Drop the
snippet from `granular_perf_hud.py` into your bench sketch so the Teensy prints
the same `GV/SD/GP` meter plus the `S|P|F` histogram/fan-out HUD the PERF page
now shows. Then run:

```sh
python docs/hardware/granular_probes/granular_perf_hud.py /dev/ttyACM0
```

and you get timestamped telemetry while you sweep clips or stress the submix
fan-out. Zero oscilloscopes, maximum visibility.

