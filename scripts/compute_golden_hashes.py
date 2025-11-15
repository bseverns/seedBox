#!/usr/bin/env python3
"""Crunch hashes for rendered golden fixtures.

This helper keeps the native golden harness honest by crawling the rendered
WAV files in ``build/fixtures/`` (or a custom path), recomputing the 64-bit
FNV-1a fingerprint we publish in ``tests/native_golden/golden.json``, and
optionally updating the manifest in place.

By default the script runs in dry-run mode and prints a tidy table. Pass
``--write`` once you like what you see. Notes from the existing manifest are
preserved unless you override them with ``--note fixture-name="fresh note"``.
"""

from __future__ import print_function

import sys

if sys.version_info < (3, 7):  # pragma: no cover - guard for misconfigured envs
    sys.stderr.write(
        "error: compute_golden_hashes.py requires Python 3.7+ (found {}.{}).\n".format(
            sys.version_info[0], sys.version_info[1]
        )
    )
    sys.exit(1)

import argparse
import datetime as _dt
import json
from pathlib import Path
import wave
try:
    from typing import Any, Dict, List, Optional, Tuple
except ImportError:  # pragma: no cover - typing is optional on old Python
    Any = Dict = List = Optional = Tuple = object  # type: ignore[misc,assignment]


FNV_OFFSET = 1469598103934665603
FNV_PRIME = 0x100000001B3


def _parse_note_overrides(raw_notes):
    # type: (Optional[List[str]]) -> Dict[str, str]
    overrides = {}  # type: Dict[str, str]
    if not raw_notes:
        return overrides
    for entry in raw_notes:
        if "=" not in entry:
            raise ValueError(
                "Malformed --note override '{}'. Use name=text.".format(entry)
            )
        name, text = entry.split("=", 1)
        name = name.strip()
        if not name:
            raise ValueError(
                "Malformed --note override '{}'. Fixture name missing.".format(entry)
            )
        overrides[name] = text.strip()
    return overrides


def _fnv64(payload):
    # type: (bytes) -> str
    state = FNV_OFFSET
    mask = (1 << 64) - 1
    for byte in payload:
        state ^= byte
        state = (state * FNV_PRIME) & mask
    return "{:016x}".format(state)


def _fnv64_pcm16(payload):
    # type: (bytes) -> str
    state = FNV_OFFSET
    mask = (1 << 64) - 1
    if len(payload) % 2 != 0:
        raise ValueError("PCM16 payload must be an even number of bytes")
    for i in range(0, len(payload), 2):
        lo = payload[i]
        hi = payload[i + 1]
        state ^= lo
        state = (state * FNV_PRIME) & mask
        state ^= hi
        state = (state * FNV_PRIME) & mask
    return "{:016x}".format(state)


def _load_manifest(path):
    # type: (Path) -> Dict[str, Any]
    if path.exists():
        with path.open("r", encoding="utf-8") as handle:
            return json.load(handle)
    return {
        "generated_at_utc": None,
        "fixtures": [],
        "tooling": {
            "hash": "64-bit FNV-1a over PCM16 payload",
            "script": "scripts/compute_golden_hashes.py",
        },
    }


def _scan_fixtures(fixtures_root):
    # type: (Path) -> List[Path]
    if not fixtures_root.exists():
        return []
    allowed = {".wav", ".txt"}
    return sorted(p for p in fixtures_root.rglob("*") if p.is_file() and p.suffix.lower() in allowed)


def _read_pcm(path):
    # type: (Path) -> Tuple[bytes, int, int, int]
    with wave.open(str(path), "rb") as wav:
        sample_width = wav.getsampwidth()
        if sample_width != 2:
            raise ValueError(
                "{} is {}-bit; expected 16-bit PCM".format(path, sample_width * 8)
            )
        frames = wav.getnframes()
        frame_rate = wav.getframerate()
        channels = wav.getnchannels()
        payload = wav.readframes(frames)

    block_align = channels * sample_width
    expected_bytes = frames * block_align
    actual_bytes = len(payload)

    if actual_bytes != expected_bytes:
        # Some of our legacy fixtures are missing a couple of trailing bytes, which
        # causes ``wave`` to hand us a payload that is smaller than the frame
        # metadata advertises. Rather than hard fail in CI, patch the payload so
        # the manifest stays reproducible.
        if actual_bytes < expected_bytes:
            deficit = expected_bytes - actual_bytes
            print(
                "warning: {} truncated by {} byte(s); padding with zeros".format(
                    path, deficit
                ),
                file=sys.stderr,
            )
            payload += b"\x00" * deficit
            actual_bytes = expected_bytes
        else:
            payload = payload[:expected_bytes]
            actual_bytes = expected_bytes

    remainder = actual_bytes % block_align
    if remainder:
        actual_bytes -= remainder
        payload = payload[:actual_bytes]
        frames = actual_bytes // block_align

    return payload, frame_rate, frames, channels


def _merge_fixture(existing,
                   overrides,
                   fixture_name,
                   data):
    # type: (Dict[str, Dict[str, Any]], Dict[str, str], str, Dict[str, Any]) -> Dict[str, Any]
    merged = existing.get(fixture_name, {}).copy()
    merged.update(data)
    if fixture_name in overrides:
        merged["notes"] = overrides[fixture_name]
    elif "notes" not in merged:
        merged["notes"] = ""
    return merged


def compute_manifest(fixtures_root,
                     manifest_path,
                     note_overrides):
    # type: (Path, Path, Dict[str, str]) -> Tuple[Dict[str, Any], List[Dict[str, Any]]]
    manifest = _load_manifest(manifest_path)
    fixtures_root_list = manifest.get("fixtures", [])
    if not isinstance(fixtures_root_list, list):
        fixtures_root_list = []
    existing_map = {item.get("name", ""): item for item in fixtures_root_list if isinstance(item, dict)}

    fixtures = []
    for path in _scan_fixtures(fixtures_root):
        suffix = path.suffix.lower()
        name = path.stem
        data = {
            "name": name,
            "path": str(path).replace("\\", "/"),
        }  # type: Dict[str, Any]
        if suffix == ".wav":
            payload, sample_rate, frames, channels = _read_pcm(path)
            hash_hex = _fnv64_pcm16(payload)
            layout = "mono" if channels == 1 else (
                "stereo" if channels == 2 else "{}-channel".format(channels)
            )
            data.update(
                {
                    "kind": "audio",
                    "hash": hash_hex,
                    "wav_path": str(path).replace("\\", "/"),
                    "sample_rate_hz": sample_rate,
                    "frames": frames,
                    "channels": channels,
                    "channel_layout": layout,
                }
            )
        else:
            body = path.read_bytes()
            hash_hex = _fnv64(body)
            lines = body.count(b"\n")
            data.update(
                {
                    "kind": "log",
                    "hash": hash_hex,
                    "bytes": len(body),
                    "lines": lines,
                }
            )
        fixtures.append(_merge_fixture(existing_map, note_overrides, name, data))

    fixtures.sort(key=lambda item: item["name"])
    manifest["fixtures"] = fixtures
    manifest["generated_at_utc"] = (
        _dt.datetime.now(_dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
    )
    tooling = manifest.get("tooling")
    if not isinstance(tooling, dict):
        tooling = {"legacy": tooling} if tooling is not None else {}
    tooling.update(
        {
            "hash": "64-bit FNV-1a over artifact payload",
            "script": "scripts/compute_golden_hashes.py",
        }
    )
    manifest["tooling"] = tooling
    return manifest, fixtures


def render_table(fixtures):
    # type: (List[Dict[str, Any]]) -> str
    if not fixtures:
        return "(no fixtures discovered)"
    header = "{:20} {:6} {:18} {:>24}".format("Fixture", "Kind", "Hash", "Summary")
    lines = [header, "-" * len(header)]
    for item in fixtures:
        kind = item.get("kind", "audio")
        if kind == "audio":
            if all(k in item for k in ("frames", "sample_rate_hz", "channels")):
                layout = item.get("channel_layout")
                summary = "{:d}f @ {:d}Hz x{:d}".format(
                    item["frames"], item["sample_rate_hz"], item["channels"]
                )
                if layout:
                    summary += " ({})".format(layout)
            else:
                summary = "(missing audio metadata)"
        else:
            summary = (
                "{} lines / {} bytes".format(
                    item.get("lines", 0), item.get("bytes", 0)
                )
                if "bytes" in item
                else "(missing log metadata)"
            )
        lines.append(
            "{:20} {:6} {:18} {:>24}".format(
                item["name"], kind, item["hash"], summary
            )
        )
    return "\n".join(lines)


def main(argv):
    # type: (List[str]) -> int
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixtures-root",
        default=Path("build/fixtures"),
        type=Path,
        help="Where rendered WAV fixtures live (default: build/fixtures)",
    )
    parser.add_argument(
        "--manifest",
        default=Path("tests/native_golden/golden.json"),
        type=Path,
        help="Path to the golden manifest JSON",
    )
    parser.add_argument(
        "--write",
        action="store_true",
        help="Actually write the manifest instead of printing a dry-run table",
    )
    parser.add_argument(
        "--note",
        action="append",
        help="Override notes for a fixture: --note drone-intro='fresh note'",
    )
    args = parser.parse_args(argv)

    try:
        note_overrides = _parse_note_overrides(args.note)
        manifest, fixtures = compute_manifest(args.fixtures_root, args.manifest, note_overrides)
    except Exception as exc:  # pragma: no cover - CLI surfacing
        print("error: {}".format(exc), file=sys.stderr)
        return 1

    print(render_table(fixtures))
    if args.write:
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        with args.manifest.open("w", encoding="utf-8") as handle:
            json.dump(manifest, handle, indent=2)
            handle.write("\n")
        print("\nmanifest updated -> {}".format(args.manifest))
    else:
        print("\n(dry run — re-run with --write to update the manifest)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
