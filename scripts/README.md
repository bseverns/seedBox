# Utility scripts — the pit crew

`scripts/` holds tiny helpers that keep the build smooth and the version info
honest. Nothing here should feel scary; if a script needs special setup, it must
say so loudly.

## What's in the garage

| Script | Job | Notes |
| --- | --- | --- |
| `gen_version.py` | Generates `include/BuildInfo.h` with git hash + build time. | PlatformIO runs it before builds so the firmware can introduce itself over serial. |
| `native/tap_tempo.py` | Estimates BPM from tap timestamps. | Accepts CLI args or STDIN; prints mean interval, BPM, and optional PPQN correction so workshops can nerd out on timing math. |
| `native/micro_offset_probe.py` | Audits per-track micro offsets. | Feed it offsets in milliseconds and it yells if any lane drifts beyond your tolerance — perfect for regression gates around swing experiments. |
| `kicad/sgtl5000_frontend.py` | Spits out a KiCad-ready SGTL5000 codec netlist using SKiDL. | Needs KiCad libraries on disk (`KICAD_SYMBOL_DIR` etc.) and `pip install skidl`. In library-less CI or preview runs it falls back to internal stub symbols, prints a ton of warnings, and still drops `build/hw/sgtl5000_frontend.net`. |
| `kicad/teensy41_core.py` | Builds the Teensy 4.1 core (IMXRT1062, MKL02, QSPI flash, USB-C, breakouts). | Mirrors the [JensChr reference board](https://github.com/jenschr/Teensy-4.1-example) so we can fab our own MCU base. Same SKiDL + KiCad lib requirements as the SGTL script; stub symbols unblock previews. |
| `kicad/seedbox_stack.py` | One-shot netlist for the full SeedBox brain (core + SGTL5000). | Calls the two generators above, reuses their nets, and spits out `build/hw/seedbox_stack.net` so you can start layout with everything already talking. |

## When you add a script

1. Prefer standard-library dependencies. If you need extra packages, document
   the install steps in the top-level README.
2. Make reruns safe — scripts should be idempotent so they never trash previous
   artifacts.
3. Drop a usage example either in the script header or this README. Future you
   will appreciate the reminder.

If a script spits out renders or logs, aim them at `out/` for disposable jams or
`artifacts/` for golden material. Both paths are already ignored by git, so the
history stays focused on intent, not binaries.

### KiCad / SKiDL setup crib notes

The SGTL5000 generator leans on SKiDL's KiCad integration. Make sure your KiCad
install exported `KICAD_SYMBOL_DIR`, `KICAD6_SYMBOL_DIR` (or whichever major
version you live on), and your `fp-lib-table` is in the usual KiCad config
directory. If SKiDL screams about missing libraries, point those environment
variables at your KiCad share tree, rerun the script, and you should be back in
business. Keep the one-liner handy:

```bash
pip install skidl
python scripts/kicad/sgtl5000_frontend.py --output build/hw/sgtl5000_frontend.net
```

No KiCad install on hand? The script now conjures stubbed symbols so you can
still preview the topology and net names. Expect SKiDL to yell about "missing
libraries" and spit out ERC warnings — that's fine for a sanity check. Once you
route this for real, rerun the script with proper KiCad libs so footprints and
pin types stay honest.

### Building a full-fat Teensy clone without dev boards

`kicad/teensy41_core.py` tracks the same vibe as Jens Chr. Brynildsen's
Teensy 4.1 example design: drop the IMXRT1062, PJRC's MKL02 boot MCU, the QSPI
flash, and USB-C straight onto your board, then fan out all IO through chunky
2x20 headers. You can still run the core and SGTL generators separately if you
want to stitch the sheets by hand, but the new `seedbox_stack.py` shortcut does
the legwork for you:

```bash
python scripts/kicad/seedbox_stack.py --output build/hw/seedbox_stack.net
```

Under the hood that script reuses the same `Net` objects for the shared rails,
I²S, I²C, and codec reset lines, so the MCU pins and the SGTL5000 land on the
exact same nets. Fire it up and you've basically recreated the Teensy 4.1 +
audio shield stack without leaving SKiDL.

Treat these helpers like the band techs: not flashy, but the show can't start
without them.
