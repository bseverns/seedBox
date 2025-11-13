# Native golden pipeline — receipts for every render

This roadmap is the companion piece to `tests/native_golden`. The goal is to
keep simulator renders honest by capturing both the raw audio artifacts and a
manifest of hashes that tests can diff without golden-ear guesswork.

## Current footprint

- **Render target:** `tests/native_golden/test_main.cpp` writes a 1-second,
  110 Hz mono drone to `build/fixtures/drone-intro.wav` when
  `ENABLE_GOLDEN=1`. The same translation unit asserts that the file landed on
  disk and that the manifest remembers it.
- **Hash discipline:** `golden::hash_pcm16` mirrors the Python helper in
  `scripts/compute_golden_hashes.py`, both using 64-bit FNV-1a over the PCM
  payload so hash drift is basically impossible unless the audio content shifts.
- **Manifest:** `tests/native_golden/golden.json` stores the digest, frame
  count, sample rate, channel count, and optional notes for every fixture. The
  Unity test cracks the JSON to make sure the entry still exists.

## How to regenerate fixtures

```bash
pio test -e native -D ENABLE_GOLDEN=1
python scripts/compute_golden_hashes.py --write
```

Add `--note name="liner note"` if you want to annotate why a render changed.
The script prints a tidy summary before committing anything to disk, so you can
spot-check hashes before rewriting the manifest.

## Roadmap to richer coverage

1. **Layer more engines.** Capture granular bursts, sampler chords, and
   resonator sweeps as distinct fixtures. Each should get a descriptive name so
   the manifest reads like a track list.
2. **Multichannel adventures.** Once the mixer matrix stabilizes, consider
   adding stereo renders or parallel control streams (e.g. MIDI logs) so the
   manifest proves both the sound and the gestures that created it.
3. **CI artifacts.** Teach the GitHub workflow to upload `build/fixtures/*.wav`
   whenever `ENABLE_GOLDEN` is enabled. Pair the uploads with the manifest so
   reviewers can audition the before/after delta straight from the PR.
4. **Docs to tests loop.** Whenever you add a fixture, update this page and link
   directly to the test or example that produces it. Treat the roadmap like a
   lab book that references both the audio proof and the code sketch.

## Related code & docs

- Rendering + assertions: [`tests/native_golden/test_main.cpp`](../../tests/native_golden/test_main.cpp)
- WAV helpers: [`tests/native_golden/wav_helpers.*`](../../tests/native_golden)
- Hash/manifest script: [`scripts/compute_golden_hashes.py`](../../scripts/compute_golden_hashes.py)
- Manifest source of truth: [`tests/native_golden/golden.json`](../../tests/native_golden/golden.json)
