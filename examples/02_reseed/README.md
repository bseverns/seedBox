# Example 02 — reseed handshake

Simulates MN42 nudging SeedBox via CC#14 and prints the refreshed seed stats.

## Run it

```bash
g++ -std=c++17 -I../../include -I../../src -o reseed main.cpp
./reseed
```

## TODO

- TODO: render a reseed cadence WAV and drop it in `/out/reseed.wav`.
- TODO: update `tests/native_golden/golden.json` with the reseed hash.
