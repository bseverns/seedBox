# Native golden harness — the mixtape still on the cutting room floor

This folder is the sketchbook for the future "render it, hash it, trust it"
pipeline. Nothing here is final; it's a loud sticky note telling us how the
story should unfold once we start freezing reference audio.

## Where we're heading

1. **Render the jam.** Patch the engine to spit raw buffers into
   `golden::write_wav_16` while running under the `native` environment. The
   helper will grow up into a real PCM writer once `ENABLE_GOLDEN=1` becomes a
   thing you flip on purpose.
2. **Hash the payload.** When the renderer runs, feed the PCM frames into
   `golden::hash_pcm16` and stash the fingerprint in `golden.json`. The fake hash
   keeps the build green for now so you can wire up the plumbing without
   breaking anybody's flow.
3. **Compare and scream (politely).** Future assertions will load `golden.json`,
   recompute hashes, and shout when the bytes shift. Until the toggle is on,
   we're just proving the harness compiles.

## Updating fixtures (once they're real)

```bash
# Turn on the flag so the helpers actually do work
pio test -e native -D ENABLE_GOLDEN=1

# Re-render whatever synth paths changed and commit the fresh hashes
python scripts/compute_golden_hashes.py  # (future tool, bring your own flavor)
```

Treat this doc like a zine page: scribble lessons learned, drop TODOs, and make
sure the next hacker knows why the groove changed.
