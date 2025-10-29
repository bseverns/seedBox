#!/usr/bin/env python3
"""Mash up the Teensy 4.1 core + SGTL5000 codec into one SKiDL netlist.

This is the "just give me the whole synth brain" button. It imports the
Teensy-inspired core generator, reuses the exact same `Net` objects inside the
SGTL5000 frontend, and spits out a single netlist that already has all the I²S,
I²C, reset, and power rails tied together. Run it when you want a headless
preview of the finished stack or when you're ready to route the board in KiCad
without juggling multiple sheets.

Usage::

    python scripts/kicad/seedbox_stack.py --output build/hw/seedbox_stack.net

You'll still need KiCad's symbol libraries somewhere SKiDL can see them
(`KICAD_SYMBOL_DIR`, etc.) unless you're fine with the stub symbol fallback the
other scripts use for previews.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from skidl import ERC, KICAD, generate_netlist, set_default_tool

import sgtl5000_frontend
import teensy41_core

set_default_tool(KICAD)


def build_stack() -> dict[str, dict[str, object]]:
    """Instantiate the core MCU and audio codec on the same schematic sheet."""

    core = teensy41_core.build_core()
    frontend = sgtl5000_frontend.build_frontend(
        {
            "avdd": core["v3v3"],
            "dvdd": core["v3v3"],
            "gnd": core["gnd"],
            "mclk": core["mclk"],
            "bclk": core["bclk"],
            "lrclk": core["lrclk"],
            "i2s_tx": core["i2s_tx"],
            "i2s_rx": core["i2s_rx"],
            "i2c_scl": core["i2c_scl"],
            "i2c_sda": core["i2c_sda"],
            "codec_reset": core["codec_reset"],
        }
    )

    return {"core": core, "frontend": frontend}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/hw/seedbox_stack.net"),
        help="Where to drop the generated netlist.",
    )
    args = parser.parse_args(argv)

    args.output.parent.mkdir(parents=True, exist_ok=True)

    build_stack()
    ERC()
    generate_netlist(file_=str(args.output))
    return 0


if __name__ == "__main__":
    sys.exit(main())
