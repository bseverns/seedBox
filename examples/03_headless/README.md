# Example 03 — headless loop

Run the SeedBox control loop without displays or MIDI. This example shows how to
pipe the scheduler into a CLI environment for automated demos or data capture.

## Run it

```bash
pio run -e native --project-option "src_dir=examples/03_headless"
.pio/build/native/program
```

Watch the printed scheduler tick count and the captured display snapshot. Quiet
mode keeps everything deterministic, making it perfect for CI smoke checks.

## TODOs

- TODO: Stream a short headless render to `/out/examples/headless.wav`.
- TODO: Add a JSON export of the display snapshot for web dashboards.
