# KiCad netlist forges — stitch the brain before copper

These SKiDL-powered scripts are how we preflight the SeedBox hardware stack
without cracking open KiCad's GUI. Think of them as recipe cards: press one,
get a fresh netlist. Ideal for CI checks, design reviews, or late-night layout
sessions when you want proof that every rail still meets its friends.

## Setup riff

1. `pip install skidl`
2. Point SKiDL at your KiCad symbol tables. Export `KICAD_SYMBOL_DIR` and friends
   (`KICAD6_SYMBOL_DIR`, etc.). If you're on a machine without KiCad, the scripts
   fall back to stub symbols so you can still generate a preview netlist with
   loud warnings.

Keep outputs under `build/hw/` — the scripts already default there and the tree
ignores those generated files.

## `teensy41_core.py` — roll your own MCU spine

Builds the IMXRT1062 brain, PJRC boot MCU, QSPI flash, and IO headers that make
a Teensy 4.1 clone. The pinout mirrors Jens Chr. Brynildsen's reference design
so you can route drop-in shields without reverse-engineering anything.

```bash
python scripts/kicad/teensy41_core.py --output build/hw/teensy41_core.net
```

The script instantiates power rails, exposes every digital pin, and runs an ERC
pass so busted nets scream before you commit copper.

## `sgtl5000_frontend.py` — codec ready to jam

Generates the SGTL5000 audio codec sheet with regulators, I²S/I²C wiring, and
reset choreography already done. It's the same schematic the firmware expects
on the other end of the ribbon cable.

```bash
python scripts/kicad/sgtl5000_frontend.py --output build/hw/sgtl5000_frontend.net
```

Pair it with the core netlist when you need a focused audio front-end review,
or run the full stack builder below to skip the manual wiring.

## `seedbox_stack.py` — one-button full brain

Imports both generators, reuses their `Net` objects, and spits out a unified
netlist with the MCU + codec already handshaking. Perfect when you want the
whole synth brain in one file for layout or BOM scrubs.

```bash
python scripts/kicad/seedbox_stack.py --output build/hw/seedbox_stack.net
```

Expect SKiDL to shout about missing footprints if KiCad isn't installed; that's
fine for smoke tests. Once you're near manufacturing, rerun with proper symbol
libraries so footprints, pin types, and ERC all line up.

Treat these scripts like a hardware rehearsal room: run them before every big
change, listen for the warnings, and you'll spot wiring blunders long before
they're etched in FR-4.
