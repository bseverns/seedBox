# Native golden pipeline — receipts for every render

This roadmap is the companion piece to `tests/native_golden`. The goal is to
keep simulator renders honest by capturing both the raw audio artifacts and a
manifest of hashes that tests can diff without golden-ear guesswork.

## Current footprint

- **Render target:** `tests/native_golden/test_main.cpp` now prints a dozen
  audio fixtures (including the quadraphonic `quad-bus.wav`, the 6-lane
  `surround-bus.wav`, and the expanded reseed suite) and a matching set of
  control logs when `ENABLE_GOLDEN=1`. Every WAV now drops a
  `build/fixtures/<fixture>-control.txt` sibling (think `sampler-grains-control`
  and `quad-bus-control`) that captures the deterministic seed schedule or MIDI
  automation the render consumed. In addition to the 110 Hz mono drone, sampler
  chord stack, and resonator tail collage we now ship a stereo granular wash
  (`granular-haze.wav`, rendered by `render_granular_fixture()`), a stereo
  master-bus composite (`mixer-console.wav`, blended by `render_mixer_fixture()`),
  a quad bus (`quad-bus.wav`), a six-channel mid/side surround layout
  (`surround-bus.wav`), and four reseed passes. The original A/B cues still
  prove the event log math, while `reseed-C.wav` (132 BPM / four passes) leans
  into higher-density swing to sniff out tempo-locked bugs, and
  `reseed-poly.wav` bolsters the stem list with the "tape rattle" sampler lane
  plus a resonator "clank shimmer" bus so we can study overlapping plucks
  without sacrificing determinism. Euclid,
  Burst, and reseed event transcripts still tag along so reviewers can diff
  timing logic beside the WAVs. PlatformIO bakes the absolute project root into
  `SEEDBOX_PROJECT_ROOT_HINT`, the runner honors a `SEEDBOX_PROJECT_ROOT`
  override, and it still climbs the filesystem looking for `platformio.ini` if
  all else fails. No matter how you launch the tests, the renders end up in
  `<repo>/build/fixtures` instead of hiding inside `.pio/`.
  The freshest entry, `stage71-bus.wav`, lives in `tests/native_golden/wav_helpers.cpp`:
  the helper `render_stage71_scene()` synthesizes an eight-lane (L,R,C,LFE,Ls,Rs,Lrs,Rrs)
  pass with a companion control log that literally annotates each bus. That
  capture proves the mixer can chew on a labeled 7.1 layout even though the
  helper lives outside `test_main.cpp`, which keeps the stage routing reusable.
  The newest entry, `modulated-sampler.wav`, is deliberately noisy: the helper
  `render_modulated_sampler_fixture()` mixes the sampler and granular engines,
  sweeps tone + spread automation every single frame, and prints a matching
  `modulated-sampler-control.txt` automation log so reviewers can see the exact
  modulation lanes that carved those wobbles. That stress case makes it obvious
  when downstream automation plumbing drifts from the C++ reference.
  Hot on its heels is `engine-hybrid-stack.wav`, minted by
  [`render_engine_hybrid_fixture()`](../../tests/native_golden/wav_helpers.cpp) and
  registered via `test_render_engine_hybrid_stack_golden()` in
  [`tests/native_golden/test_main.cpp`](../../tests/native_golden/test_main.cpp).
  It hammers the sampler, resonator, and granular engines with the same Euclid +
  Burst schedule, then logs **every** automation lane (brightness, drive,
  bloom/feedback, grain density, macro pan) to
  `engine-hybrid-stack-control.txt`. Think of it as a deterministic track sheet:
  if a PR nudges the Euclid mask, Burst spacing, or any of the modulation waves,
  the log diff spells out exactly what changed without ever leaving the CLI.
  The new sibling, `engine-macro-orbits.wav`, uses the same engines but leans
  harder into modulation pedagogy: [`render_engine_macro_orbits_fixture()`](../../tests/native_golden/wav_helpers.cpp)
  bakes in macro pan "orbit" math, sampler contour/crunch sweeps, resonator
  damping/spark curves, and granular spray lanes, then dumps a 14k-line
  `engine-macro-orbits-control.txt` ledger so reviewers can diff every Euclid /
  Burst hit plus the automation snapshot that sculpted it. The Unity harness
  wires it up via `test_render_engine_macro_orbits_golden()` so `pio test -e`
  `native_golden` always emits both the WAV and the control transcript.
- **Layered Euclid/Burst capture:** `layered-euclid-burst.wav` now rides shotgun
  with every golden run. The helper
  [`render_layered_euclid_burst_fixture()`](../../tests/native_golden/wav_helpers.cpp)
  walks a Euclid mask and hands each gate to the Burst engine so sampler,
  resonator, and granular voices all share the same schedule. The matching test
  (`test_render_layered_euclid_burst_golden()` in
  [`tests/native_golden/test_main.cpp`](../../tests/native_golden/test_main.cpp))
  keeps the WAV + control log wired into the manifest so reviewers can diff the
  Euclid/Burst event list and the per-frame modulation sweeps (`tone`,
  `color`, and `spray`) alongside the audio.
- **Hash discipline:** `golden::hash_pcm16` still mirrors the Python helper for
  PCM data, and a sibling FNV-1a byte walker fingerprints the log files. Both
  feed the same manifest so hashes stay in lockstep between test harness and
  CLI script.
- **Manifest:** `tests/native_golden/golden.json` stores the digest, artifact
  path, and either audio metadata (sample rate, frame count, channel count) or
  log metadata (line + byte counts). Optional notes keep the liner-book vibe
  intact, and `scripts/generate_native_golden_header.py` rewrites
  `fixtures_autogen.hpp` straight from the manifest so Unity tests always pull
  the latest catalog without manual edits.

## How to regenerate fixtures

```bash
pio test -e native_golden
python3 scripts/compute_golden_hashes.py --write
```

The `native_golden` env is just the standard native config with the flag wired
in, which keeps CI from caching a non-golden binary between runs.

(`scripts/compute_golden_hashes.py` insists on Python 3.7+, so lean on the
explicit `python3` binary even if your shell still points `python` at the
ancient 2.x ghosts.)

Registry acting up or hacking offline? Run `./scripts/offline_native_golden.sh`
to compile the standalone helper, regenerate every fixture, and refresh the
manifest without touching PlatformIO. It relies on the same render routines and
hash math, so the golden receipts line up exactly. The helper now pulls in the
heavy spatial renders (engine hybrid stack, macro orbit stack, the 7.1 bus) so
fallback runs never silently drop fixtures from the manifest.

Need the 30-second long-take collage specifically? Kick
`tests/native_golden/render_long_take.sh`. When PlatformIO is present it still
routes through `pio test -e native_golden --filter test_render_long_take_golden`,
but if the CLI is missing the wrapper now launches the same offline helper
described above **without** a filter. That means the empty `build/fixtures/`
folder on your desktop gets re-seeded every run instead of only refreshing
`long-random-take.wav`. If you really want to render a subset while offline,
call `scripts/offline_native_golden.sh --filter ...` directly so you stay in
control of the scope.

Add `--note name="liner note"` if you want to annotate why a render changed.
The script prints a tidy summary before committing anything to disk, so you can
spot-check hashes before rewriting the manifest.

Need to stage fixtures somewhere else while debugging? Pass
`SEEDBOX_FIXTURE_ROOT=/tmp/seedbox-fixtures` when running the tests. Pair it
with `SEEDBOX_PROJECT_ROOT=/wherever/you/cloned/seedBox` if you launched the
binary from an odd working directory and want to skip the auto-discovery.
The manifest continues to publish the canonical `build/fixtures/...` paths so
reviewers and automation stay in sync, but the raw files can live wherever keeps
your flow snappy.

## Roadmap to richer coverage

1. **Layer more engines.** Granular voices and the summing mixer are now in the
   gallery; the next frontier is pairing them with control streams (MIDI logs,
   modulation lanes) so reviewers get a full track sheet with each PR.
2. **Multichannel adventures.** Stereo renders are live, so start sketching
   surround/mid-side takes or alternate busses once the DSP matures. The
   freshly added `surround-bus.wav` is a six-channel proof-of-life that folds
   front mid/side energy into dedicated center/LFE lanes while crossfeeding the
   surrounds.
3. **CI artifacts.** The workflow now replays `pio test -e native_golden`,
   regenerates the autogen header, and uploads `build/fixtures/*` plus the
   refreshed manifest. Reviewers can grab the artifact bundle straight from the
   PR Files tab and spin the mix without cloning.
4. **Docs to tests loop.** Whenever you add a fixture, update this page and link
   directly to the test or example that produces it. Treat the roadmap like a
   lab book that references both the audio proof and the code sketch.

## Reviewer tips

- GitHub keeps the golden fixtures under the workflow artifacts. Download the
  `native-golden-fixtures` bundle from the PR checks page to hear the latest
  render set and read the manifest that stamped them.

## Related code & docs

- Rendering + assertions: [`tests/native_golden/test_main.cpp`](../../tests/native_golden/test_main.cpp)
- WAV helpers: [`tests/native_golden/wav_helpers.*`](../../tests/native_golden)
- Hash/manifest script: [`scripts/compute_golden_hashes.py`](../../scripts/compute_golden_hashes.py)
- Manifest source of truth: [`tests/native_golden/golden.json`](../../tests/native_golden/golden.json)
