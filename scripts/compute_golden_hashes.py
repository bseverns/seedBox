#!/usr/bin/env python3
# -*- coding: utf-8 -*-
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
import subprocess
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
    # We stash WAVs plus human-readable control logs (txt/json). Hash them all so
    # the manifest stays in sync with the binary/audio payloads we check in.
    allowed = {".wav", ".txt", ".json"}
    return sorted(p for p in fixtures_root.rglob("*") if p.is_file() and p.suffix.lower() in allowed)


def _read_pcm(path):
    # type: (Path) -> Tuple[bytes, int, int, int]
    try:
        return _read_pcm_with_wave(path)
    except (wave.Error, EOFError, RuntimeError) as exc:
        # ``wave`` likes to throw ``RuntimeError`` (with an empty message!) when it
        # walks a malformed chunk header, which made ``--write`` runs look like the
        # script just died with ``error:`` and zero context. Treat all of those the
        # same way we already handle ``wave.Error``: print a salvage warning and
        # fall back to the manual chunk scan so we can still fingerprint fixtures
        # that were trimmed or transported by lossy tools.
        print(
            "warning: {} has corrupt RIFF metadata ({}); attempting salvage".format(
                path, exc or "wave decoder raised {}".format(type(exc).__name__)
            ),
            file=sys.stderr,
        )
        return _read_pcm_salvage(path)


def _read_pcm_with_wave(path):
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

    payload, frames = _normalize_payload(path, payload, frames, channels, sample_width)
    return payload, frame_rate, frames, channels


def _read_pcm_salvage(path):
    # type: (Path) -> Tuple[bytes, int, int, int]
    blob = path.read_bytes()
    if len(blob) < 16 or not blob.startswith(b"RIFF"):
        raise ValueError("{} is not a WAVE/RIFF payload".format(path))

    wave_offset = blob.find(b"WAVE")
    if wave_offset == -1:
        raise ValueError("{} is missing the WAVE signature".format(path))

    offset = wave_offset + 4
    fmt = None  # type: Optional[Dict[str, int]]
    data_chunk = None  # type: Optional[bytes]

    while offset + 8 <= len(blob):
        chunk_id = blob[offset : offset + 4]
        chunk_size = int.from_bytes(blob[offset + 4 : offset + 8], "little")
        offset += 8
        available = len(blob) - offset
        if chunk_size > available:
            print(
                "warning: {} chunk {} claims {} byte(s) but only {} remain; clamping".format(
                    path, chunk_id.decode("ascii", "replace"), chunk_size, max(available, 0)
                ),
                file=sys.stderr,
            )
            chunk_size = max(available, 0)
        chunk = blob[offset : offset + chunk_size]
        offset += chunk_size
        if offset % 2:
            offset += 1  # word-aligned padding

        if chunk_id == b"fmt ":
            if len(chunk) < 16:
                raise ValueError("{} has incomplete fmt chunk".format(path))
            audio_format = int.from_bytes(chunk[0:2], "little")
            if audio_format != 1:
                raise ValueError("{} is not PCM (format {})".format(path, audio_format))
            channels = int.from_bytes(chunk[2:4], "little")
            sample_rate = int.from_bytes(chunk[4:8], "little")
            bits_per_sample = int.from_bytes(chunk[14:16], "little")
            sample_width = bits_per_sample // 8
            fmt = {
                "channels": channels,
                "sample_rate": sample_rate,
                "sample_width": sample_width,
            }
        elif chunk_id == b"data":
            data_chunk = chunk

    if fmt is None or not _fmt_is_sane(fmt):
        fmt = _scan_fmt_chunk(blob, path)
    if data_chunk is None:
        data_chunk = _scan_data_chunk(blob, path)
    if fmt is None or data_chunk is None:
        raise ValueError("{} is missing fmt or data chunks".format(path))

    channels = fmt["channels"]
    sample_rate = fmt["sample_rate"]
    sample_width = fmt["sample_width"]
    block_align = channels * sample_width if channels and sample_width else 0
    if block_align <= 0:
        raise ValueError("{} has invalid block alignment".format(path))

    frames = len(data_chunk) // block_align
    payload = data_chunk[: frames * block_align]
    payload, frames = _normalize_payload(path, payload, frames, channels, sample_width)
    return payload, sample_rate, frames, channels


def _scan_fmt_chunk(blob, path):
    # type: (bytes, Path) -> Optional[Dict[str, int]]
    offset = blob.find(b"fmt ")
    if offset == -1 or offset + 12 > len(blob):
        return None
    chunk = blob[offset + 8 : offset + 24]
    if len(chunk) < 4:
        return None

    def _field(field, expected):
        # type: (bytes, int) -> Optional[int]
        return int.from_bytes(field, "little") if len(field) == expected else None

    audio_format = _field(chunk[0:2], 2) or 1
    channels = _field(chunk[2:4], 2) or 1
    sample_rate = _field(chunk[4:8], 4) or 48000
    bits_per_sample = _field(chunk[14:16], 2) or 16

    warnings = []
    if audio_format != 1:
        warnings.append("format {}".format(audio_format))
    if channels <= 0:
        channels = 1
        warnings.append("channels reset to 1")
    if not (8000 <= sample_rate <= 384000):
        sample_rate = 48000
        warnings.append("sample_rate reset to 48000")
    sample_width = bits_per_sample // 8
    if sample_width <= 0 or sample_width > 4:
        sample_width = 2
        warnings.append("bits/sample reset to 16")

    if warnings:
        print(
            "warning: {} fmt chunk was desynced; {}".format(
                path, ", ".join(warnings)
            ),
            file=sys.stderr,
        )
    else:
        print(
            "warning: {} fmt chunk was desynced; recovered via signature scan".format(path),
            file=sys.stderr,
        )

    return {
        "channels": channels,
        "sample_rate": sample_rate,
        "sample_width": sample_width,
    }


def _fmt_is_sane(fmt):
    # type: (Dict[str, int]) -> bool
    channels = fmt.get("channels")
    sample_rate = fmt.get("sample_rate")
    sample_width = fmt.get("sample_width")
    if not isinstance(channels, int) or channels <= 0 or channels > 32:
        return False
    if not isinstance(sample_width, int) or sample_width <= 0 or sample_width > 8:
        return False
    if not isinstance(sample_rate, int) or not (8000 <= sample_rate <= 384000):
        return False
    return True


def _scan_data_chunk(blob, path):
    # type: (bytes, Path) -> Optional[bytes]
    offset = blob.find(b"data")
    if offset == -1:
        return None
    start = offset + 8
    if start >= len(blob):
        return None
    declared = int.from_bytes(blob[offset + 4 : offset + 8], "little")
    available = len(blob) - start
    if declared <= 0 or declared > available or available - declared > 32:
        print(
            "warning: {} data chunk length looked bogus ({} vs {}); using tail payload".format(
                path, declared, available
            ),
            file=sys.stderr,
        )
        return blob[start:]
    return blob[start : start + declared]


def _normalize_payload(path, payload, frames, channels, sample_width):
    # type: (Path, bytes, int, int, int) -> Tuple[bytes, int]
    block_align = channels * sample_width
    expected_bytes = frames * block_align
    actual_bytes = len(payload)

    if actual_bytes != expected_bytes:
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

    return payload, frames


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
        generator = (Path(__file__).resolve().parent / "generate_native_golden_header.py").resolve()
        subprocess.check_call(
            [
                sys.executable,
                str(generator),
                "--manifest",
                str(args.manifest),
            ]
        )
        print("fixture header refreshed -> tests/native_golden/fixtures_autogen.hpp")
    else:
        print("\n(dry run — re-run with --write to update the manifest)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
