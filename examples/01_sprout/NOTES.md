# sprout run notes — 2025-02-14

## Build + launch attempts

- `pio run -e native -t upload` stalled while pulling the `native@1.2.1` platform (the CLI printed the install header but never finished). Need to chase whether it's a mirror timeout or a missing offline cache before the README promises the command as gospel.
- Fallback compile with `g++ -std=c++17 examples/01_sprout/src/main.cpp -o /tmp/sprout` worked cleanly for poking at the sim. Might be worth documenting as a "quick riff" option until PlatformIO behaves.

## Quiet vs loud sweep

- Default quiet mode runs the loop at 10× speed, dumping all eight ghost hits in about a second—perfect for smoke-test vibes.
- `--loud` slows the sim to real time. Expect ~5 seconds of output for two measures while every beat breathes.

## Seed mutations on deck

- `default` — kick on 1, snare on 3, hat on 4. Baseline groove.
- `hatless` — strips the hat hits; nice for spotlighting the downbeat without cymbal chatter.
- `afterbeat-chop` — adds a ghost clap on the "and" while sliding the snare to beat four. Good for hearing how off-beat accents land even in quiet mode.

## Oddities worth circling back to

- No mutation descriptions in-code yet; might be useful to expose quick blurbs when `--list-mutations` fires.
- Consider exposing BPM and measure count flags once we start bouncing WAVs so we can dial in longer skeletons.
