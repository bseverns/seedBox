# SeedBox 0.2 Public Proof Pass

Date: 2026-04-25

Git revision: `8faa24444f9f03a779ce6b7646e2338b7938d09e`

This pass is intentionally stabilization-first. No feature work was added here.
The goal was to make the current system harder to dispute by refreshing the
deterministic proof surfaces, the firmware build, and the desktop lane.

## Command log

| Command | Status | Notes |
| --- | --- | --- |
| `pio test -e native` | PASS | `72` tests, `1` skipped, `71` succeeded. |
| `pio test -e native_golden` | PASS | `72` tests succeeded. |
| `pio run -e teensy40` | PASS | Firmware image built successfully. |
| `cmake --build build/juce --target SeedboxApp SeedboxVST3_VST3 seedbox_juce_vst3_probe` | PARTIAL FAIL | `SeedboxApp` and `SeedboxVST3_VST3` built, but `make` reported no rule for `seedbox_juce_vst3_probe` in that combined invocation. |
| `cmake --build build/juce --target seedbox_juce_vst3_probe` | PASS | Probe executable built successfully. |
| `./build/juce/seedbox_juce_vst3_probe --plugin build/juce/SeedboxVST3_artefacts/VST3/SeedBox.vst3 --output build/juce_probe/seedbox_vst3_probe.wav` | PASS | Probe rendered a non-silent WAV. |
| `python3 scripts/compute_golden_hashes.py --write` | PASS | Golden manifest refreshed and header regenerated. |
| `pio test -e teensy40 --filter test_hardware` | PARTIAL | Teensy compile/size step completed, but no physical PASS/FAIL receipt landed in this workspace. |

## Regenerated golden timestamp

- `tests/native_golden/golden.json` generated at `2026-04-25T18:09:05Z`

## Artifact hashes

| Artifact | SHA-256 |
| --- | --- |
| `build/juce/SeedboxVST3_artefacts/VST3/SeedBox.vst3/Contents/MacOS/SeedBox` | `f6bf95aaf433bb396af189aa095ffe78833dff9ce3b098db68f33c91a40f8696` |
| `build/juce/SeedboxApp_artefacts/SeedBox Standalone.app/Contents/MacOS/SeedBox Standalone` | `244906a822b472c6eb89bfa58c839a7b2ef2e34029a8157800c122a07afb83a8` |
| `build/juce/seedbox_juce_vst3_probe` | `585917795017df9cf8574ea30d4ec2935e7dc3211125e01fc4f7d99bc352a9e1` |
| `build/juce_probe/seedbox_vst3_probe.wav` | `8363e9cf98e84d54e2043e4fe0807664d4425cc5558492c92e4f95f0d1e74680` |

## JUCE probe result

- Probe path: `build/juce/SeedboxVST3_artefacts/VST3/SeedBox.vst3`
- Output WAV: `build/juce_probe/seedbox_vst3_probe.wav`
- Seconds rendered: `3`
- Sample rate: `48000`
- Block size: `512`
- Peak: `0.0490013`
- RMS: `0.0346585`

## Hardware-facing result

The only hardware-facing command I could complete in this workspace was the
Teensy build lane.

- `pio run -e teensy40` passed and produced a firmware image.
- `pio test -e teensy40 --filter test_hardware` advanced through compile and
  size checks, reporting:
  - `FLASH: code:171012, data:18648, headers:8992, free for files:1832964`
  - `RAM1: variables:26016, code:168436, padding:28172, free for local variables:301664`
  - `RAM2: variables:270976, free for malloc/new:253312`
- I did not capture a physical-unit serial PASS/FAIL receipt in this workspace,
  so this is a build-facing hardware result, not a full bench receipt.

## Caveats

- The repo still does not contain a dated, unit-specific hardware bench receipt.
- The combined JUCE build command in the request referenced `seedbox_juce_vst3_probe`
  in the same invocation as the app and VST3 targets; in this checkout that probe
  target needed its own build invocation.
- `pio test -e native` emitted two unused-constant warnings from
  `src/app/AppState.cpp`, but the test suite still passed.
- This pass proves the repo-side and desktop surfaces much more strongly than it
  proves a physically wired bench.

## Bottom line

This is a valid "current as of now" proof pass for the repo-side lanes:

- native tests are green
- native golden tests are green
- Teensy firmware builds
- JUCE app, VST3, and headless probe build
- JUCE probe renders audible non-silent audio
- golden manifest was refreshed

The missing piece is still a dated, physical hardware receipt.
