# Native golden audio harness (stub)

This folder sets up the deterministic audio test harness. We keep the wiring
ready while postponing binary fixtures.

## What exists now

- `harness.h/.cpp` — placeholder render + WAV writer stubs.
- `golden.json` — template mapping fixture names to hashes.
- `test/native_golden/test_main.cpp` — Unity test that skips unless
  `ENABLE_GOLDEN` is defined.

## TODO roadmap

1. Render 1–2 s snippets (`sprout`, `reseed cadence`) using the native build.
2. Capture raw PCM buffers, write them to `/out/<name>.wav` using a tiny
   PCM16 writer.
3. Compute SHA-256 hashes for each buffer and update `golden.json`.
4. Enable assertions in the Unity test so it verifies the hash set.
5. Upload audio previews separately (never in-repo) and link from docs.

**Reminder:** keep `QUIET_MODE` enabled during renders to lock RNG seeds.
