# Native golden pipeline — receipts for every render

This roadmap is the companion piece to `tests/native_golden`. The goal is to
keep simulator renders honest by capturing both the raw audio artifacts and a
manifest of hashes that tests can diff without golden-ear guesswork.

## Current footprint

- **Render target:** `tests/native_golden/test_main.cpp` now prints five audio
  fixtures and two control logs when `ENABLE_GOLDEN=1`. In addition to the
  110 Hz mono drone, sampler chord stack, and resonator tail collage we now
  ship a stereo granular wash (`granular-haze.wav`, rendered by
  `render_granular_fixture()`) and a stereo master-bus composite
  (`mixer-console.wav`, blended by `render_mixer_fixture()`). Euclid and Burst
  debug transcripts still tag along so reviewers can diff timing logic beside
  the WAVs.
- **Hash discipline:** `golden::hash_pcm16` still mirrors the Python helper for
  PCM data, and a sibling FNV-1a byte walker fingerprints the log files. Both
  feed the same manifest so hashes stay in lockstep between test harness and
  CLI script.
- **Manifest:** `tests/native_golden/golden.json` stores the digest, artifact
  path, and either audio metadata (sample rate, frame count, channel count) or
  log metadata (line + byte counts). Optional notes keep the liner-book vibe
  intact.

## How to regenerate fixtures

```bash
PLATFORMIO_BUILD_FLAGS="-D ENABLE_GOLDEN=1" pio test -e native
python scripts/compute_golden_hashes.py --write
```

Add `--note name="liner note"` if you want to annotate why a render changed.
The script prints a tidy summary before committing anything to disk, so you can
spot-check hashes before rewriting the manifest.

## Roadmap to richer coverage

1. **Layer more engines.** Granular voices and the summing mixer are now in the
   gallery; the next frontier is pairing them with control streams (MIDI logs,
   modulation lanes) so reviewers get a full track sheet with each PR.
2. **Multichannel adventures.** Stereo renders are live, so start sketching
   surround/mid-side takes or alternate busses once the DSP matures.
3. **CI artifacts.** The workflow now replays `PLATFORMIO_BUILD_FLAGS="-D ENABLE_GOLDEN=1" pio test -e native`
   and uploads `build/fixtures/*` plus the refreshed manifest. Reviewers can
   grab the artifact bundle straight from the PR Files tab and spin the mix
   without cloning.
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
