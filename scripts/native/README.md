# Native quantizer toolkit — the ghost rig cheat sheet

These native scripts are the "play without flashing firmware" tools. They talk
to the same buffers as the quantizer example harness, so you can rehearse
workflows, capture traces, and tune UI copy before the hardware wakes up. Bring
your favourite terminal; everything here is plain Python 3 with zero extra
packages.

## `quantizer_ws_display.py` — fake the front panel

Spin up a little WebSocket server that redraws the quantizer state every time
the native harness emits a frame. It's the visual monitor we use in labs when
the real OLEDs are busy.

### Fire it up

```bash
cd scripts/native
python quantizer_ws_display.py --port 8765
```

You should see the listener announce:

```
[quantizer-ws] listening on ws://127.0.0.1:8765/quantizer
```

Leave it running and jump to `examples/04_scale_quantizer` in another terminal.
After `pio run -e native`, point the harness at the ghost rig:

```bash
cd examples/04_scale_quantizer
.pio/build/native/program --ws=ws://127.0.0.1:8765/quantizer --drift=0.5
```

The console clears and prints a table with the current slot index, raw drifted
pitch, and the snapped values for the nearest/up/down modes. It updates in
place like a vintage CRT debug panel — perfect for demoing scale changes to a
room without screen-sharing your IDE.

### Record the vibe

Add `--export-csv` to the harness so every frame lands in `out/quantizer.csv`.
When you replay the CSV later, the WebSocket feed will still animate live
offsets while your DAW crunches the captured notes. The script has no notion of
playback yet, but the on-screen numbers are the same ones the firmware will
pronounce.

## Other native helpers

* `tap_tempo.py` — Sling MIDI-tap test data in and it prints BPM plus PPQN
  correction hints. Handy when you're calibrating sequencer clocks in a hurry.
* `micro_offset_probe.py` — Feed per-track offsets (ms) and it screams if any
  lane drifts past your tolerance. Great regression guardrails around swing
  experiments.

Treat them as lab pedals: stomp to check timing, stomp to prove offsets, then
pipe the quantizer feed to the ghost rig so everyone sees what the firmware
will soon be doing for real.
