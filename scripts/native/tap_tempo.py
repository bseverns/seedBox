#!/usr/bin/env python3
"""Tap tempo estimator for nerds who want receipts.

This tiny helper chews on a list of timestamps (in seconds) and spits back the
average BPM plus a handful of debugging stats.  Feed it either explicit
timestamps:

    ./tap_tempo.py 0.0 0.48 0.96 1.44

...or pipe a newline-delimited stream in via STDIN:

    cat taps.txt | ./tap_tempo.py

Each timestamp is treated as a beat hit; the script measures deltas between
adjacent taps, averages them, and converts the result to BPM.  Optionally set
`--ppqn` if your taps represent MIDI clock pulses instead of straight quarter
notes.  We intentionally narrate every step so workshops can riff on the math
without cracking open a spreadsheet mid-lesson.
"""

from __future__ import annotations

import argparse
import statistics
import sys
from typing import Iterable, List


def _parse_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Estimate BPM from tap timestamps.")
    parser.add_argument(
        "timestamps",
        nargs="*",
        type=float,
        help="Tap timestamps in seconds. Leave empty to read from STDIN.",
    )
    parser.add_argument(
        "--ppqn",
        type=int,
        default=1,
        help=(
            "Pulses per quarter note. Use 24 for raw MIDI clock, keep the default "
            "for straight beat taps."
        ),
    )
    parser.add_argument(
        "--trim",
        type=int,
        default=0,
        help="Drop this many warm-up taps from both ends before averaging.",
    )
    return parser.parse_args(argv)


def _load_timestamps(args: argparse.Namespace) -> List[float]:
    values: List[float] = list(args.timestamps)
    if not values:
        values = [float(line.strip()) for line in sys.stdin if line.strip()]
    if len(values) < 2:
        raise SystemExit("Need at least two taps to estimate tempo.")
    values.sort()
    return values


def _compute_intervals(timestamps: Iterable[float]) -> List[float]:
    ordered = list(timestamps)
    return [b - a for a, b in zip(ordered[:-1], ordered[1:]) if b > a]


def _trim(intervals: List[float], trim: int) -> List[float]:
    if trim <= 0 or len(intervals) <= trim * 2:
        return intervals
    return intervals[trim:-trim]


def main(argv: List[str] | None = None) -> int:
    args = _parse_args(argv or sys.argv[1:])
    timestamps = _load_timestamps(args)
    intervals = _compute_intervals(timestamps)
    if not intervals:
        raise SystemExit("Timestamps must be strictly increasing.")
    cleaned = _trim(intervals, args.trim)
    mean_interval = statistics.mean(cleaned)
    bpm = 60.0 / (mean_interval / max(args.ppqn, 1))
    spread = statistics.pstdev(cleaned) if len(cleaned) > 1 else 0.0

    print("TAP TEMPO REPORT")
    print("================")
    print(f"Samples analysed : {len(cleaned)} intervals")
    print(f"Mean interval    : {mean_interval:.6f} s")
    print(f"Std deviation    : {spread:.6f} s")
    print(f"Estimated BPM    : {bpm:.2f}")
    if args.ppqn != 1:
        print(f"(PPQN correction : {args.ppqn} pulses per quarter note)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
