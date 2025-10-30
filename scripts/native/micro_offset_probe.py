#!/usr/bin/env python3
"""Crunch per-track micro offsets and call out suspicious drift.

This script exists for those "did we overswing that lane?" conversations. Feed
it a pile of offsets (milliseconds, signed), either as command-line arguments or
via a whitespace-delimited file/STDIN stream, and it will summarise the spread.
If any absolute deviation blows past `--tolerance`, the exit code flips to 1 so
CI (or an irate producer) can throw a flag.
"""

from __future__ import annotations

import argparse
import statistics
import sys
from pathlib import Path
from typing import Iterable, List


def _parse_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate per-track micro offsets.")
    parser.add_argument(
        "offsets",
        nargs="*",
        type=float,
        help="Offset values in milliseconds."
    )
    parser.add_argument(
        "--file",
        type=Path,
        help="Optional file containing whitespace-delimited offsets."
    )
    parser.add_argument(
        "--tolerance",
        type=float,
        default=2.0,
        help="Absolute deviation in ms before we scream."
    )
    return parser.parse_args(argv)


def _load_offsets(args: argparse.Namespace) -> List[float]:
    values: List[float] = list(args.offsets)
    if args.file:
        payload = args.file.read_text().split()
        values.extend(float(token) for token in payload)
    if not args.offsets and not args.file:
        payload = sys.stdin.read().split()
        values.extend(float(token) for token in payload)
    if not values:
        raise SystemExit("No offsets supplied.  Give me numbers or give me silence.")
    return values


def _summarise(offsets: Iterable[float]) -> None:
    values = list(offsets)
    mean = statistics.mean(values)
    spread = statistics.pstdev(values) if len(values) > 1 else 0.0
    minimum = min(values)
    maximum = max(values)
    print("MICRO OFFSET REPORT")
    print("====================")
    print(f"Samples analysed : {len(values)} tracks")
    print(f"Mean offset      : {mean:.3f} ms")
    print(f"Std deviation    : {spread:.3f} ms")
    print(f"Min / max        : {minimum:.3f} ms / {maximum:.3f} ms")


def main(argv: List[str] | None = None) -> int:
    args = _parse_args(argv or sys.argv[1:])
    offsets = _load_offsets(args)
    _summarise(offsets)
    limit = abs(args.tolerance)
    offenders = [value for value in offsets if abs(value) > limit]
    if offenders:
        print("WARNING: offsets beyond tolerance detected:")
        for value in offenders:
            print(f"  -> {value:.3f} ms")
        return 1
    print("All offsets sit inside the requested tolerance. Carry on.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
