# Safety

## Panic / loose-bits reset

When students hit a stuck note, a runaway sample, or a gate that won't die, trigger the panic pathway and the whole rig drops into a neutral pose:

- Long-press the **Live Capture** button (the same front-panel key that normally grabs a preset snapshot) to call `AppState::triggerPanic`. That action:
  - clears `PatternScheduler`'s pending queues so no remnant seeds fire before the next tick,
  - resets every engine (`Sampler`, `Granular`, `Resonator`, `Euclid`, `Burst`) so their voices / envelopes / pending clouds go quiet,
  - bangs `MidiRouter::panic()` to send “all notes off” on every exposed port and clears the board-facing MIDI note guard,
  - punches the input buffer and gate tracker clean, and
  - sets a flag that skips the very next clock tick so the audio/detented state stays silent until the next normal transport step.
- Hardware builds also print a one-line confirmation over the USB serial console: `PANIC: voices, queues, and transport cleared.` That message is emitted at control-rate once per panic so teachers know it landed.
- Because the scheduler is paused for one tick, the engines stay quiet until the following `tick()` call (or, when MIDI is driving the transport, until the next `onExternalClockTick()` that passes the panic gate).

Use panic as the classroom-safe escape hatch whenever a runaway sample, stuck gate, or orphaned clock pulse needs to be silenced without rebooting the Teensy rig.
