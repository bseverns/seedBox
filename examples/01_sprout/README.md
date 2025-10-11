# Example 01 — sprout

Deterministic boot: spins up `AppState` in Quiet Mode and prints the seed table.

## Run it

```bash
g++ -std=c++17 -I../../include -Isrc -I../../src -o sprout main.cpp
./sprout
```

(Or wire it into PlatformIO by swapping `src/main.cpp` temporarily.)

## TODO

- TODO: render a 1 s WAV snapshot and save to `/out/sprout.wav`.
- TODO: publish the hash in `tests/native_golden/golden.json` once fixtures land.
