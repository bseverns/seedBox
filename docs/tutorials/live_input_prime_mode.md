# Live-input prime mode — capture the ghost mic

The roadmap promised `SeedPrimeMode::kLiveInput` would tag seeds so the granular
engine could munch the live input path. This tutorial gives you the receipts on
the simulator: we’ll reseed into the live lane, arm the virtual I²S tap, and grab
the planned grain metadata that proves the "live-in" alias is wired even when the
codec is nowhere in sight.

## TL;DR

- `buildLiveInputSeeds` reuses the LFSR genome but stamps every seed with the
  live source tag and the reserved slot zero lineage.【F:src/app/AppState.cpp†L1074-L1082】
- `AppState::armGranularLiveInput(true)` re-arms the granular engine in the native
  build so `GranularEngine::resolveSource` keeps the live input path instead of
  downgrading to SD clips.【F:src/app/AppState.h†L109-L120】【F:src/app/AppState.cpp†L1780-L1789】
- `test_live_input_prime_triggers_live_voice_in_sim` spins the simulator long
  enough to trigger a grain, then inspects the planned voice and the mock
  hardware shim to confirm the live buffer alias fired.【F:tests/test_app/test_seed_prime_modes.cpp†L55-L90】

## 1. Map the pipeline

`AppState::seedPageReseed(masterSeed, SeedPrimeMode::kLiveInput)` flows through
`buildLiveInputSeeds`, which clones the LFSR genomes, tags `Seed::source` as
`kLiveInput`, and forces the granular params to point at slot 0 (`"live-in"`).【F:src/app/AppState.cpp†L1007-L1082】
When the scheduler fires that seed, `GranularEngine::planGrain` resolves the
source slot and leaves the path string set to `"live-in"` so both the Teensy graph
and the simulator know they should sip the live input mixer.【F:src/engine/Granular.cpp†L247-L276】

**Coverage receipts.** When you zoom out from the live lane, every prime mode
now has an explicit regression trail in the seed-system roadmap. After this lab,
flip to the roadmap's regression section to see the tap-tempo lineage check and
preset-restore test that landed in the same suite — that way future students can
jump from prose to Unity proof without spelunking the repo history.【F:docs/roadmaps/seed_system.md†L74-L94】

## 2. Run the regression

The new regression in `tests/test_app/test_seed_prime_modes.cpp` walks the whole
story. Kick it off directly when you need receipts:

```bash
pio test -e native --filter test_app/test_seed_prime_modes.cpp
```

You’ll see the `test_live_input_prime_triggers_live_voice_in_sim` case tick the
scheduler ~96 frames, then assert that `GranularEngine::voice(0)` is active,
`source == kLiveInput`, and the mock SD player never tried to stream a clip.【F:tests/test_app/test_seed_prime_modes.cpp†L55-L90】

## 3. Capture the mock buffer

Want to script it yourself? The snippet below mirrors the regression but keeps
things inline for a lab notebook:

```c++
AppState app;
app.initSim();
app.armGranularLiveInput(true);               // flip the sim into "mic" mode
app.seedPageReseed(app.masterSeed(),          // reseed with the current master seed
                   AppState::SeedPrimeMode::kLiveInput);
app.setSeedEngine(0, EngineRouter::kGranularId);

const Seed& seed0 = app.seeds().front();
AppState::SeedNudge boost{};
boost.densityDelta = 24.f;                    // fire every tick
boost.probabilityDelta = 1.f - seed0.probability;
boost.jitterDeltaMs = -seed0.jitterMs;        // kill jitter so timing is clean
app.seedPageNudge(0, boost);

for (int i = 0; i < 96; ++i) {
  app.tick();                                 // drive the internal clock
}

const auto voice = app.debugGranularVoice(0);
// voice.source == GranularEngine::Source::kLiveInput
// voice.sourcePath -> "live-in"
#if !SEEDBOX_HW
const auto hw = app.debugGranularSimVoice(0);
// hw.sdPlayerPlayCalled == false  (no SD clip touch)
#endif
```

Stash that in a test harness, pump `hal::audio::mockPump` if you need the audio
callback to run, and you’ve got a repeatable proof that the live-input prime mode
latches the simulator’s live buffer path without any Teensy hardware in the loop.
