# Native golden harness — the mixtape still on the cutting room floor

This folder is the sketchbook for the "render it, hash it, trust it" pipeline.
The first brick is live: a deterministic 1-second drone render that hashes to
`f53315eb7db89d33` using 64-bit FNV-1a. The TODOs below still matter, but
there's finally a real sound anchored to the words.

## Where we're heading

1. **Render the jam.** Patch the engine to spit raw buffers into
   `golden::write_wav_16` while running under the `native` environment. With
   `ENABLE_GOLDEN=1` the helper now writes an honest-to-goodness PCM file into
   `artifacts/` for inspection.
2. **Hash the payload.** When the renderer runs, feed the PCM frames into
   `golden::hash_pcm16` and stash the fingerprint in `golden.json`. The helper
   uses 64-bit FNV-1a so repeated renders should land on the published hash.
3. **Compare and scream (politely).** Future assertions will load `golden.json`,
   recompute hashes, and shout when the bytes shift. Until more fixtures exist,
   we're just proving the harness compiles and the reference hash is stable.

## Updating fixtures (once they're real)

```bash
# Turn on the flag so the helpers actually do work
pio test -e native -D ENABLE_GOLDEN=1

# Re-render whatever synth paths changed and commit the fresh hashes
python scripts/compute_golden_hashes.py  # (future tool, bring your own flavor)
```

Treat this doc like a zine page: scribble lessons learned, drop TODOs, and make
sure the next hacker knows why the groove changed.
