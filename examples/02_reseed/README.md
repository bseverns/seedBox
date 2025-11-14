# 02 · reseed — shuffle the ghost garden

We finally pipe the quiet garden into the offline renderer. Two master seeds walk
the same stem list, we reshuffle them three laps each, and bounce deterministic
stems to disk with the receipts stapled alongside.

## What it does

* Compiles under the native PlatformIO toolchain and runs as a desktop binary.
* Shuffles a fixed roster of imaginary stems with a deterministic RNG, then
  reschedules them across three passes per seed.
* Renders each pass into `/out/reseed-A.wav` and `/out/reseed-B.wav` via the
  shared offline renderer and writes `/out/reseed-log.json` describing every hit.

## Wiring

Still zero. This is a notebook jam session with the speakers pulled from the rack.

## Running it

```sh
pio run -d examples/02_reseed -t run
```

The binary writes all artifacts relative to the repo root:

* `out/reseed-A.wav` — bounce for seed `0xCAFE`.
* `out/reseed-B.wav` — bounce for seed `0xBEEF`.
* `out/reseed-log.json` — structured log of the shuffle order, lanes, RNG
  fingerprints, and the WAV file the block belongs to.

Each run is deterministic. Regenerate the files whenever the stem list or
render recipe changes.

## Keeping the golden fixtures fresh

The regression harness in `tests/native_golden` locks the WAV and JSON payloads
in step with the example. When you tweak the rendering recipe:

1. Re-run the example so `/out/` holds the new stems and log.
2. Capture the fixtures with golden mode enabled:
   ```sh
   ENABLE_GOLDEN=1 pio test -e native_golden
   ```
3. Update the manifest hashes:
   ```sh
   python scripts/compute_golden_hashes.py --write \
     --note reseed-A="seed 0xCAFE offline stem" \
     --note reseed-B="seed 0xBEEF offline stem" \
     --note reseed-log="paired event log"
   ```

Treat the README like a studio notebook: scribble why you changed things, not
just what flipped.
