#!/usr/bin/env python3
"""Dump the granular perf HUD from a Teensy over USB serial.

Usage:
  python granular_perf_hud.py /dev/ttyACM0

The firmware already exposes all of the interesting counters via
`GranularEngine::Stats`.  To mirror the OLED HUD on a bench rig, sprinkle this
helper into your sketch (or a quick PlatformIO test):

```cpp
void loop() {
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 100) {
    return;
  }
  lastPrint = millis();
  const auto stats = appState.granularStats();
  Serial.printf("GV%02u SD%02u GP%03lu S%02u|%02u P%02u|%02u F%u%u\n",
                stats.activeVoiceCount,
                stats.sdOnlyVoiceCount,
                static_cast<unsigned long>(stats.grainsPlanned % 1000u),
                stats.grainSizeHistogram[0] + stats.grainSizeHistogram[1],
                stats.grainSizeHistogram[GranularEngine::Stats::kHistogramBins - 1],
                stats.sprayHistogram[0] + stats.sprayHistogram[1],
                stats.sprayHistogram[GranularEngine::Stats::kHistogramBins - 1],
                stats.busiestMixerLoad,
                stats.mixerGroupsEngaged);
}
```

Fire up this script and it will time-stamp every HUD line so you can correlate
voice pressure with mixer fan-out or CPU hiccups.  No GUI, no oscilloscope —
just raw, punk-rock telemetry. `F%u%u` encodes "voices on the busiest mixer"
and "mixer groups currently engaged" so you can spot fan-out collapse at a
glance.
"""
from __future__ import annotations

import argparse
import datetime as _dt
import sys
from typing import Iterator

try:
    import serial  # type: ignore
except ImportError as exc:  # pragma: no cover - doc tool
    raise SystemExit(
        "pyserial is required. Install it with `pip install pyserial` before running the HUD logger."
    ) from exc


def _read_lines(device: serial.Serial) -> Iterator[str]:
    buffer = bytearray()
    while True:
        chunk = device.read(1)
        if not chunk:
            continue
        if chunk == b"\n":
            yield buffer.decode(errors="ignore").strip()
            buffer.clear()
        else:
            buffer.extend(chunk)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Mirror the SeedBox granular HUD over USB serial")
    parser.add_argument("port", help="Serial port exposed by the Teensy (e.g. /dev/ttyACM0 or COM6)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate used by the sketch (default: 115200)")
    args = parser.parse_args(argv)

    with serial.Serial(args.port, args.baud, timeout=1) as dev:
        print(f"Connected to {dev.port} @ {dev.baudrate} baud. Press Ctrl+C to exit.")
        for line in _read_lines(dev):
            if not line:
                continue
            timestamp = _dt.datetime.now().strftime("%H:%M:%S.%f")[:-3]
            print(f"[{timestamp}] {line}")

    return 0


if __name__ == "__main__":  # pragma: no cover - manual bench helper
    sys.exit(main(sys.argv[1:]))
