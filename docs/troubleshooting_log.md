# Troubleshooting logbook

Stuff breaks. This logbook makes sure every weird failure leaves breadcrumbs for
the next builder. Each section starts with an intent-first checklist, then gives
you space to record the fix, the test that proved it, and any firmware knobs you
had to twist. Keep the tone punk but the notes precise.

## Boot failures & silent rigs

- [ ] Confirm the bring-up sequence in [`src/main.cpp`](../src/main.cpp) actually
      runs — missed SGTL5000 enables or clock init are the usual culprits.
- [ ] Re-seat USB and power, then run `pio test -e native --filter
      test_simulator_audio_reports_48k` to prove the firmware can still lock 48 kHz
      in the simulator ([`test/test_app/test_audio_defaults.cpp`](../test/test_app/test_audio_defaults.cpp)).
- [ ] Capture an `AppState::DisplaySnapshot` (see
      [`src/app/AppState.cpp`](../src/app/AppState.cpp)) and paste the OLED metrics
      below so future readers can compare baseline text.
- [ ] Document the fix: which solder joints, cables, or boot order mattered?

| Date | Symptom | Root cause | Fix | Test receipt |
| --- | --- | --- | --- | --- |
| | | | | |

## Clock + transport weirdness

- [ ] Follow the bench script in
      [`docs/hardware/trs_clock_sync/README.md`](hardware/trs_clock_sync/README.md)
      to reproduce the issue with structured wiring steps.
- [ ] Run `pio test -e native --filter test_external_clock_priority` to see if the
      simulator matches the observed behaviour
      ([`test/test_app/test_external_midi_priority.cpp`](../test/test_app/test_external_midi_priority.cpp)).
- [ ] If MN42 controllers are in the loop, replay
      `test_mn42_follow_clock_mode` from
      [`test/test_app/test_mn42_control.cpp`](../test/test_app/test_mn42_control.cpp)
      and note whether the handshake flags flipped.
- [ ] Add any serial logs or scope captures to `artifacts/` and link them here.

| Date | Symptom | Root cause | Fix | Test receipt |
| --- | --- | --- | --- | --- |
| | | | | |

## Engine silence or off-by-one timbres

- [ ] Trigger known-good seeds via the simulator (`AppState::primeSeeds` in
      [`src/app/AppState.cpp`](../src/app/AppState.cpp)) and compare the playback
      to the seed stories in [`docs/roadmaps/resonator.md`](roadmaps/resonator.md).
- [ ] Run `pio test -e native --filter test_resonator_maps_seed_into_voice_plan`
      to verify the modal math still aligns with
      [`test/test_engine/test_resonator_voice_pool.cpp`](../test/test_engine/test_resonator_voice_pool.cpp).
- [ ] For granular or sampler issues, cross-check the coverage in
      [`test/test_app/test_granular_source_toggle.cpp`](../test/test_app/test_granular_source_toggle.cpp)
      and note which seed presets misbehaved.
- [ ] When you find the fix, add a short summary plus commit hash here and mirror
      it in the calibration doc if it requires future rigs to adjust.

| Date | Symptom | Root cause | Fix | Test receipt |
| --- | --- | --- | --- | --- |
| | | | | |
