# Bench Validation Matrix

This is the practical bridge between SeedBox's repo-side proof and a real unit
on a desk. Use it after assembly, after wiring changes, and before any demo
where hardware behavior matters.

Repo-side tests and goldens prove a lot. They do not prove your exact soldering,
power, wiring, panel feel, or peripheral chain. That is what this matrix is for.

## How to use this page

- Run the repo-side proof first where applicable.
- Then run the bench ritual on the physical unit.
- Keep a receipt: serial log, scope capture, photo, or short written note.
- If something fails, link the result from [`docs/troubleshooting_log.md`](troubleshooting_log.md).
- Use [`docs/BenchReceiptTemplate.md`](BenchReceiptTemplate.md) if you want a
  copy/paste signoff format.

## Current matrix

| Surface | Repo-side proof already in the repo | Bench ritual | Receipt to keep | Current posture |
| --- | --- | --- | --- | --- |
| Firmware build + flag sanity | `pio run -e teensy40`; build flag docs in [`platformio.ini`](../platformio.ini) and [`docs/builder_bootstrap.md`](builder_bootstrap.md) | Build and flash the exact unit firmware you plan to test | Build command + git revision in lab notes | `Required per unit / per firmware change` |
| Audio boot path / 48 kHz baseline | [`tests/test_app/test_audio_defaults.cpp`](../tests/test_app/test_audio_defaults.cpp) and calibration notes in [`docs/calibration_guide.md`](calibration_guide.md) | Power the unit, confirm codec wake, then compare simulator baseline against audible hardware output | Short bench note, plus any gain/noise observations | `Bench validation still required` |
| OLED/UI snapshot truth | Shared snapshot path described in [`src/README.md`](../src/README.md) and onboarding docs | Boot the unit and compare visible OLED state against the expected UI flow during page changes/reseeds | Photo or handwritten state notes | `Bench validation still required` |
| Encoders and buttons | App/UI behavior is exercised in [`tests/test_app/`](../tests/test_app) and calibration guide sections | Twist every encoder and press every button, checking directionality and latch behavior against the documented panel assumptions | Short pass/fail table per control | `Bench validation still required` |
| External clock / transport dominance | [`tests/test_app/test_external_midi_priority.cpp`](../tests/test_app/test_external_midi_priority.cpp) plus [`docs/hardware/trs_clock_sync/README.md`](hardware/trs_clock_sync/README.md) | Build with `SEEDBOX_DEBUG_CLOCK_SOURCE=1`, feed TRS clock, and confirm serial logs + transport behavior | Serial log capture | `Bench validation strongly recommended` |
| USB/TRS MIDI wiring | Bootstrap pin map and TRS clock sync guide | Confirm the physical jack wiring, then send known MIDI traffic through the intended path | Wiring photo and serial log | `Bench validation still required` |
| Granular hardware fan-out | [`tests/test_hardware/test_granular_teensy.cpp`](../tests/test_hardware/test_granular_teensy.cpp) and [`docs/hardware/granular_probes/README.md`](hardware/granular_probes/README.md) | Run `pio test -e teensy40 --filter test_hardware`, then do the manual headphone/line-out probe | Unity serial output and any listening notes | `Has hardware-targeted repo proof, still needs unit check` |
| Wiring correctness | [`docs/wiring_gallery.md`](wiring_gallery.md), [`docs/hardware_bill_of_materials.md`](hardware_bill_of_materials.md), and bootstrap pin table | Inspect the actual harness against the documented pin map before closing the enclosure | Top/side/underside photos | `Bench validation still required` |
| End-to-end calibration pass | [`docs/calibration_guide.md`](calibration_guide.md) | Run the full calibration ritual after assembly or servicing | Dated calibration entry in [`docs/troubleshooting_log.md`](troubleshooting_log.md) | `Best current canonical bench receipt` |

## Minimum pre-demo checklist

If you only have time for one fast hardware pass, do this:

1. Build and flash the intended firmware.
2. Confirm boot, OLED, and audio output.
3. Check encoder direction and button actions.
4. Run one external-clock test if that demo depends on sync.
5. Save a dated note with anything odd before the unit leaves the bench.

## TODOs

- TODO: add a small "known-good bench captures" index once real photos/logs are
  curated into the docs tree.
