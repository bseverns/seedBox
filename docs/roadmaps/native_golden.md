# Native golden pipeline — receipts for every render

This roadmap is the companion piece to `tests/native_golden`. The goal is to
keep simulator renders honest by capturing both the raw audio artifacts and a
manifest of hashes that tests can diff without golden-ear guesswork.

## Current footprint

- **Render target:** `tests/native_golden/test_main.cpp` now prints ten audio
  fixtures (including the new quadraphonic `quad-bus.wav` and the fresh
  six-lane `surround-stage.wav`) and three control
  logs when `ENABLE_GOLDEN=1`. In addition to the 110 Hz mono drone, sampler
  chord stack, and resonator tail collage we now ship a stereo granular wash
  (`granular-haze.wav`, rendered by `render_granular_fixture()`), a stereo
  master-bus composite (`mixer-console.wav`, blended by `render_mixer_fixture()`),
  a surround-ready mixdown (`quad-bus.wav`), a mid-side seasoned LCR/LFE layout
  (`surround-stage.wav`), and two reseed passes. Euclid,
  Burst, and reseed event transcripts still tag along so reviewers can diff
  timing logic beside the WAVs. PlatformIO bakes the absolute project root into
  `SEEDBOX_PROJECT_ROOT_HINT`, the runner honors a `SEEDBOX_PROJECT_ROOT`
  override, and it still climbs the filesystem looking for `platformio.ini` if
  all else fails. No matter how you launch the tests, the renders end up in
  `<repo>/build/fixtures` instead of hiding inside `.pio/`.
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
hash math, so the golden receipts line up exactly.

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
   surround/mid-side takes or alternate busses once the DSP matures.
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
