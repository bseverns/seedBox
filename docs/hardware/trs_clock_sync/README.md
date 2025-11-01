# TRS Clock Sync Bench Jam

This walkthrough is the hands-on proof that the new Serial7 TRS MIDI driver is
keeping pace with USB. The mission: fire clock pulses into the Type-A mini jack
and watch the firmware flip `externalClockDominant_` just like it does when a
USB host is in charge.

> **Prep mantra:** we're building instruments as lab gear. Keep notes, be loud
> about what you're measuring, and don't be afraid to scribble extra prints into
the firmware when you're teaching.

## Gear you need

* SeedBox hardware running the latest firmware from this repo.
* A MIDI clock source that speaks 24 PPQN over Type-A TRS (another sequencer, a
  MIDIpal, whatever keeps time without drama).
* USB cable back to your laptop so you can watch the serial log.
* PlatformIO (or the PJRC toolchain) ready to build/upload for Teensy 4.x.

## Build with debug breadcrumbs

The firmware now ships with an opt-in log hook that yells whenever the external
clock state flips. Toggle it on when you compile (and peek at
[`include/SeedBoxConfig.h`](../../include/SeedBoxConfig.h) for the canonical
flag descriptions):

```bash
# from repo root
pio run -e seedbox_hw -t upload --silent --project-option="build_flags = -DQUIET_MODE=0 -DSEEDBOX_DEBUG_CLOCK_SOURCE=1"
```

* `QUIET_MODE=0` keeps the MIDI router live. Quiet mode short-circuits the
  callbacks, so leave that at zero for this test.
* `SEEDBOX_DEBUG_CLOCK_SOURCE=1` enables the Serial prints inside
  `AppState::onExternalClockTick/Start/Stop`. They announce exactly when the
  firmware decides an external clock is in charge.

If you're not using PlatformIO, inject the same `-D` flags into your
`platform.txt` / Makefile setup before compiling.

## Wiring + clock fire

1. Patch the clock source's Type-A TRS **output** into SeedBox's Type-A **input**
   (the jack tied to `Serial7` RX on pin `28`).
2. Open a serial monitor at 115200 baud (PlatformIO: `pio device monitor -b
   115200`).
3. Power up both devices. Let SeedBox finish booting — you'll see a quiet
   console until clocks arrive.
4. Start the external clock.

## What to observe

* The serial monitor should spit a one-liner that reads
  `external clock: TRS/USB seized transport`. That's the moment
  `externalClockDominant_` flips to true because of the TRS pulses.
* If your clock box also sends MIDI Start, you'll see an immediate
  `external clock: transport START` line.
* Kill the external clock (or send MIDI Stop) and watch for
  `external clock: transport STOP`. That log fires only when the boolean drops
  back to false, matching the USB behaviour.

While the log is screaming, spin SeedBox's encoders and confirm the internal
scheduler is riding the incoming pulse train — note triggers will line up with
the external clock rate.

## Bonus riffs

* For deeper analysis, clip a scope or logic analyser to pins 28/29 and capture
  the raw UART stream. You'll see the 31.25 kbaud MIDI bytes that the new HAL
  parser is chewing through.
* Swap in a different TRS-to-DIN adaptor (Type-B, DIY lash-up, etc.) and confirm
  the wiring still lands on the Serial7 pins defined in
  `include/HardwareConfig.h`.
* Feel free to leave the debug flag on during rehearsals — it's a solid live
  indicator when a DAW reclaims transport control mid-set.

Document everything you notice. Treat this README as a living lab log — tack on
scope screenshots, weird edge cases, or alternative clock sources as you find
them.
