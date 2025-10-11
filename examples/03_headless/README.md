# Example 03 — headless tick loop

Minimal CLI loop for CI or scripting. It ticks the scheduler a few times and
prints HAL metrics.

## Run it

```bash
g++ -std=c++17 -I../../include -I../../src -o headless main.cpp
./headless
```

## TODO

- TODO: render a short burst of headless output and stash in `/out/headless.wav`.
- TODO: script this example as part of a future smoke test.
