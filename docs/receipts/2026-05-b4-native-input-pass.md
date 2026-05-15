# SeedBox Native Input Receipt — `b4.wav`

Date: 2026-05-14

Git revision: `56a635baa75398b1f7c50374cc3db0689f3d3827`

This pass runs a real external audio file through the native host render lane
instead of a synthetic tone or unit test buffer. The goal is to leave behind a
dated live-input-style receipt before the Teensy bench pass lands.

## Command log

| Command | Status | Notes |
| --- | --- | --- |
| `cmake --build build/juce --config Release --target seedbox_native_input_probe` | PASS | Built the new headless native input probe target. |
| `./build/juce/seedbox_native_input_probe --input /Users/bseverns/Desktop/b4.wav --output build/receipts/2026-05-14-b4-native-output.wav --status-json build/receipts/2026-05-14-b4-native-status.json --summary-json build/receipts/2026-05-14-b4-native-summary.json --block-size 256 --sample-rate 48000` | PASS | Rendered the full 224-second file through `AppState::initJuceHost` + `hal::audio::renderHostBuffer`. |
| `env PLATFORMIO_CORE_DIR=/private/tmp/pio-home pio test -e native -f test_app` | PASS | `65` app-focused native tests passed. |
| `env PLATFORMIO_CORE_DIR=/private/tmp/pio-home pio test -e native_golden -f test_app` | PASS | `65` app-focused native-golden tests passed. |
| `env PLATFORMIO_CORE_DIR=/private/tmp/pio-home pio run -e teensy40` | PASS | Firmware build and size pass succeeded. |

## Input artifact

- Path: `/Users/bseverns/Desktop/b4.wav`
- SHA-256: `53b7773b039b9e66f8baa979d370e4ad9525df8ba2856029a410295f35a01190`
- Format: mono PCM16 WAV
- Source sample rate: `22050`
- Source frames: `4939200`
- Duration: `224.0` seconds
- Source peak: `0.961060`
- Source RMS: `0.537559`

## Native render path

- Bootstrap: `AppState::initJuceHost(48000, 256)`
- Input feed: block-by-block `setDryInputFromHost(...)`
- Audio render: `hal::audio::renderHostBuffer(...)`
- Output sample rate: `48000`
- Output channels: stereo
- Rendered frames: `10752000`
- Duration: `224.0` seconds
- Resampler: linear, mono `22050` -> `48000`

## Output artifacts

- WAV: `build/receipts/2026-05-14-b4-native-output.wav`
- WAV SHA-256: `7b13006d486d140e2e2ec3335895bd208dbd5c5a942594c9ee350b912bac7b42`
- Status JSON: `build/receipts/2026-05-14-b4-native-status.json`
- Status JSON SHA-256: `6e8e2ee0cb6ff2be3f419adee9aaac197abedb3dc9fe12b735e5bfa2ca2aff4f`
- Summary JSON: `build/receipts/2026-05-14-b4-native-summary.json`
- Summary JSON SHA-256: `e9f4c6b73d07ec5b7508cf1b35e56dfd270b08748b2a0c41f10553d638e5b2a7`

## Render stats

- Resampled input peak: `0.930888`
- Resampled input RMS: `0.535440`
- Output left peak: `0.116346`
- Output left RMS: `0.042455`
- Output right peak: `0.482816`
- Output right RMS: `0.189916`
- Mean absolute diff between resampled input and output left: `0.464553`

That diff value is the useful proof here: the native stack did not just hand the
dry file straight back untouched. The render materially transformed it.

## Final status snapshot

Final `captureStatusJson()` result:

```json
{"mode":"HOME","page":"Seeds","masterSeed":1592635569,"activePresetId":1592635569,"activePresetSlot":"default","bpm":120.000,"schedulerTick":0,"hostDiagnostics":{"midiDroppedCount":0,"oversizeBlockDropCount":0,"lastOversizeBlockFrames":0,"preparedScratchFrames":0},"externalClockDominant":false,"followExternalClockEnabled":false,"waitingForExternalClock":false,"quietMode":false,"globalSeedLocked":false,"focusSeedLocked":false,"focusSeed":{"present":true,"index":0,"id":0,"engineId":1,"engineName":"Granular"}}
```

## Caveats

- This pass exercises the native app/host render lane directly, not the JUCE
  shell. That means `hostDiagnostics` remains zero by construction here because
  the JUCE-side oversize/drop counters are not in play.
- The pass proves a full-length external audio file can move through the native
  stack and emit a transformed stereo render. It does not yet prove:
  - extended real-time desktop playback under a live device callback
  - Teensy hardware behavior
  - long physical live-input monitoring on the actual instrument
- The probe currently uses linear resampling for non-48k inputs. That is good
  enough for this receipt, but it is not a production-quality offline SRC.

## Bottom line

This is a valid native live-input-style receipt:

- a real external file entered the host/native stack
- the full 224-second pass completed
- the output stayed non-silent and materially different from the dry input
- the repo-side native and Teensy build surfaces remained green alongside it

The next missing receipt is still the real hardware bench pass with sustained
live input on the Teensy path.
