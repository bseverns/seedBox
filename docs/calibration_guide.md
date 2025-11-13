# SeedBox calibration circuit

This guide exists for the moment a fresh build leaves the solder mat and needs
its behaviour lined up with the firmware’s expectations. Treat it like a lab
ritual: we surface intent, run targeted calibrations, and log anything gnarly so
the next crew can replay the session without guesswork.

## Pre-flight ritual

- [ ] Clone the latest `main`, run `git status`, and note any local hardware
      hacks before you begin so this log stays honest.
- [ ] Skim [`docs/builder_bootstrap.md`](builder_bootstrap.md) for wiring sanity
      and confirm your pin map still mirrors
      [`include/HardwareConfig.h`](../include/HardwareConfig.h).
- [ ] Run `pio pkg install` once to refresh toolchains, then tag your bench
      notebook with today’s date and the unit’s serial.

## Audio chain calibration

- [ ] Power the rig and confirm the SGTL5000 wake flow matches the bring-up
      path in [`src/main.cpp`](../src/main.cpp) — headphone hiss or missing
      output usually means codec enable lines aren’t latched.
- [ ] Boot the native simulator with `pio test -e native --filter
      test_simulator_audio_reports_48k` to prove the 48 kHz baseline still
      matches [`hal::audio::sampleRate()`](../test/test_app/test_audio_defaults.cpp).
- [ ] Trigger a resonator seed via the sim or hardware UI and compare the voice
      metrics to the expectations captured in
      [`test/test_engine/test_resonator_voice_pool.cpp`](../test/test_engine/test_resonator_voice_pool.cpp).
- [ ] If gains drift, snapshot the OLED metrics (`AppState::captureDisplaySnapshot`
      lives in [`src/app/AppState.cpp`](../src/app/AppState.cpp)) and stash the
      before/after numbers in your lab notes.

## Clock + transport calibration

- [ ] Follow the wiring choreography in
      [`docs/hardware/trs_clock_sync/README.md`](hardware/trs_clock_sync/README.md)
      so TRS clock edges actually hit `Serial7`.
- [ ] With hardware attached, flash a build compiled with
      `-DSEEDBOX_DEBUG_CLOCK_SOURCE=1` (see the clock logging notes in
      [`AppState::onExternalTransportStart`](../src/app/AppState.cpp)).
- [ ] Fire external clock pulses and confirm the serial log mirrors the
      simulator flow locked down by
      [`test/test_app/test_external_midi_priority.cpp`](../test/test_app/test_external_midi_priority.cpp).
- [ ] If TRS loses to USB, capture the debug log and append it to the
      troubleshooting table below so the next calibration starts with context.

## Encoder + UI sweep

- [ ] Twist every encoder and confirm directionality matches the table in
      [`builder_bootstrap.md`](builder_bootstrap.md#hardware-wiring--test-points);
      reversed direction means the A/B channels were flipped.
- [ ] Mash transport buttons and confirm the mode bits exposed via the MN42 map
      behave like [`test_mn42_transport_latch_behavior`](../test/test_app/test_mn42_control.cpp).
- [ ] Run through seed reseeds on the simulator and ensure
      `AppState::setSeedEngine` honours the CC hygiene documented near
      `applyMn42ModeBits` in [`src/app/AppState.cpp`](../src/app/AppState.cpp).

## Post-calibration drop

- [ ] Commit a calibration note in `docs/troubleshooting_log.md` with today’s
      date, observed quirks, and which tests you re-ran.
- [ ] Kick off `pio test -e native` overnight; annotate the results with any
      deviations from the expected clock dominance or voice stats.
- [ ] Archive scope captures, audio renders, and serial logs in `artifacts/` and
      link them directly from the troubleshooting log for future hunts.
