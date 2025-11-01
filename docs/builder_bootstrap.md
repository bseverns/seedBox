# Builder bootstrapping primer

Welcome to the SeedBox staging area. This doc is the mix of shop notebook and
teaching guide you reach for when someone says "make it work on my desk." Treat
it as a living index — when you learn something gnarly or wire a new widget,
leave breadcrumbs here so the next person can ship faster.

## Table of contents

1. [Environment setup](#environment-setup)
2. [Repository rituals](#repository-rituals)
3. [Hardware wiring & test points](#hardware-wiring--test-points)
4. [Build + test commands](#build--test-commands)
5. [Debugging playbook](#debugging-playbook)
6. [Student labs & suggested experiments](#student-labs--suggested-experiments)
7. [Reference links](#reference-links)

## Environment setup

### Operating system notes

- **macOS / Linux**: fully supported. Install the latest Python 3.10+ and make
  sure `pipx` or `pip` is available. PlatformIO uses virtual environments under
  the hood, so a minimal system Python is fine.
- **Windows**: works via WSL2. Use Ubuntu 22.04 or newer, then follow the same
  steps as Linux. Native Windows builds of PlatformIO are okay, but the Teensy
  toolchain paths differ — document the quirks in this section if you go that
  route.

### PlatformIO bring-up

```bash
pip install --upgrade platformio
pio pkg install
```

- The first command pulls in PlatformIO globally so `pio` is on your path.
- `pio pkg install` reads `platformio.ini` and downloads all Teensy + Unity
  dependencies into `.pio/`. Re-run this after editing `platformio.ini`.
- Stash any generated `.wav` renders in `out/` and log artifacts in `artifacts/`.
  Both are ignored by git, so no one has to review binary blobs in PRs.

### Nice-to-have extras

- `ninja` speeds up native builds; PlatformIO will fall back to `make` otherwise.
- `teensy-loader-cli` handles uploads without the Arduino IDE. On Debian-based
  systems: `sudo apt install teensy-loader-cli`.
- `pyenv` or `asdf` for juggling Python versions if you bounce between projects.

## Repository rituals

- **Stay in sync**: run `git pull --rebase` before you branch. Merging in master
  while you have uncommitted PlatformIO artifacts is a quick path to chaos. After
  big merges, sanity-check `main` with
  `python scripts/describe_seedbox_config.py --format=markdown` so the build flag
  crib sheet in docs still matches the header.
- **Generated headers**: `scripts/gen_version.py` writes `include/BuildInfo.h`.
  The build system calls it automatically, but if you see missing-version errors
  during native tests, run the script manually to regenerate the header.
- **Docs as code**: every substantial firmware change should update the relevant
  roadmap or this primer. Leave a timestamped note in the "Repository rituals"
  section when you discover workflow hacks.
- **Binary artifacts**: never commit compiled firmware or audio blobs. If a doc
  references a file that should be generated, leave a `TODO: generate ...` note
  instead.

## Hardware wiring & test points

### Core stack

- **MCU**: Teensy 4.0
- **Audio interface**: PJRC Audio Shield (SGTL5000)
- **Display**: SparkFun Qwiic OLED 1.3" (SH1107 over I²C, default address `0x3C`)
- **Input**: Two rotary encoders + push buttons, USB MIDI, plus dual 3.5 mm
  Type-A MIDI jacks (DIN is dead; embrace the mini TRS future)

### Pin cheat sheet (Teensy 4.0)

| Function | Pin(s) | Notes |
|----------|--------|-------|
| I²S Audio (SGTL5000) | 23 (MCLK), 21 (BCLK), 20 (LRCLK), 7 (TX), 8 (RX), 10 (SD_CS) | Keep these quiet; they belong to the audio shield stack. |
| OLED I²C | 18 (SDA), 19 (SCL) | Qwiic OLED already has pull-ups; add externals only if you hang more devices. |
| USB MIDI | Native USB port | Enumerates as `USB_MIDI_SERIAL`. |
| TRS MIDI IN | 28 (Serial7 RX) | Mirror the Type-A spec; route the opto output from the TRS jack straight here. |
| TRS MIDI OUT | 29 (Serial7 TX) | Buffer with the usual 33 Ω resistors before hitting the jack. |
| Encoders | Seed/Bank: 0/1 + switch on 2; Density/Prob: 3/4 + switch on 5; Tone/Tilt: 24/26 + switch on 27; FX/Mutate: 6/9 + switch on 30 | Debounce in software via `EncoderReader`. |
| Buttons | Tap: 31, Shift: 32, Alt Seed: 33 | Active-low with 10 kΩ pull-ups. |
| LEDs (debug) | 16, 17 | Optional heartbeat / tick lamps. |

> **TRS wiring ritual:** Mirror the official Type-A pinout. Route the TRS IN jack
> through the standard MIDI opto network and land its output on Serial7 RX. Fan
> the TX leg through a pair of 33 Ω resistors before touching the OUT jack. With
> DIN retired, the switched jacks simply mute the TRS path when nothing is
> plugged in, leaving USB MIDI as the silent fallback.

> **Qwiic cabling tip:** The SparkFun OLED ships with JST-SH connectors and
> on-board pull-ups. Either use a stock Qwiic lead into the Teensy (red=3.3 V,
> black=GND, blue=SDA, yellow=SCL) or solder directly to the castellated pads if
> you crave a slimmer wiring harness.

Print this table and tape it to your bench. Update the pin map when hardware
assignments shift so firmware and CAD stay aligned.

### Power sanity checks

- Feed the Teensy via USB during development. When adding external supplies,
  remove the VUSB-VIN link and double-check ground continuity.
- The audio shield likes a clean 3.3 V rail. If you hear hiss, check the boost
  converters powering other modules.
- Always plug in headphones or a dummy load before firing up new DSP. Feedback
  tests can scream.

## Build + test commands

| Goal | Command | Notes |
|------|---------|-------|
| Run unit tests fast | `pio test -e native` | Uses the host build; Teensy libs mocked out. |
| Build firmware | `pio run -e teensy40` | Generates `.pio/build/.../firmware.hex`. |
| Build firmware (legacy alias) | `pio run -e teensy40_usbmidiserial` | Same build, kept for 2024 scripts. |
| Upload via CLI | `pio run -e teensy40 --target upload` | Requires `teensy-loader-cli`. |
| Upload via CLI (legacy alias) | `pio run -e teensy40_usbmidiserial --target upload` | Same CLI for legacy docs. |
| Open serial console | `pio device monitor -b 115200` | Shares the USB cable with MIDI clock. |
| Regenerate build info | `python scripts/gen_version.py` | Only needed if the auto-hook fails. |

> ⚠️ **Quiet mode reminder:** defaults live in
> [`include/SeedBoxConfig.h`](../include/SeedBoxConfig.h). Hardware uploads stay
> quiet until you append `-D QUIET_MODE=0` to your `build_flags` (per-env in
> `platformio.ini` or via `--project-option`). Native tests already flip the flag
> off so simulations keep their deterministic seeds.

When documenting lab sessions, jot down the exact command invocations and link
to this table. Future students can replay your steps.

## Debugging playbook

- **Clock drift**: If the scheduler feels laggy, log incoming MIDI clock rates
  with `MidiRouter::debugDump()`. Compare USB vs. TRS feeds.
- **Seed chaos**: When triggers look random, print the seed table via
  `SeedTable::debugDescribe()` to confirm reseed order.
- **Audio silence**: The Sampler and Granular engines are works in progress. Confirm
  the scheduler is firing by watching the OLED snapshots before hunting in DSP
  code.
- **Native vs. hardware mismatches**: Wrap hardware-only includes with
  `#ifdef SEEDBOX_HW`. The native build should always compile without Teensy
  headers present.

Document additional war stories here — broken solder joints, OS driver drama,
or fixes for flaky USB hubs.

## Student labs & suggested experiments

1. **Clock detective**: Use the native build to simulate incoming MIDI clock at
   different BPM values. The simulator's sample cursor now advances using the
   `60 / (BPM * 24)` seconds-per-tick math, so plot how `whenSamples` spreads
   apart as you twist tempo. Archive the graphs in `/docs/labs/` (create it when
   you have data) so future builders can replay the experiment.
2. **Reseed rituals**: Modify the test suite to reseed mid-pattern and track the
   deterministic output. Teach classmates how to prove the system is stable.
3. **Hardware smoke test**: Build a checklist that walks through power-up,
   OLED boot splash, encoder rotation, and MIDI transport detection. Convert the
   checklist into an automated hardware-in-the-loop test when possible.

As you run these labs, paste links to your findings or PRs below each bullet so
the lineage stays obvious.

## Reference links

- [PlatformIO documentation](https://docs.platformio.org/en/latest/)
- [PJRC Teensy 4.0 pinout](https://www.pjrc.com/teensy/pinout.html)
- [PJRC Audio Shield guide](https://www.pjrc.com/store/teensy3_audio.html)
- [Teensy Loader CLI](https://www.pjrc.com/teensy/loader_cli.html)
- [Unity test framework (Arduino flavor)](https://github.com/ThrowTheSwitch/Unity)

If a link 404s, replace it immediately and note the update date. Broken docs are
how lab myths are born.
