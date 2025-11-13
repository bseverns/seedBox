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

from __future__ import annotations

import argparse
import datetime as _dt
import json
from pathlib import Path
import sys
import wave


FNV_OFFSET = 1469598103934665603
FNV_PRIME = 0x100000001B3


def _parse_note_overrides(raw_notes: list[str] | None) -> dict[str, str]:
    overrides: dict[str, str] = {}
    if not raw_notes:
        return overrides
    for entry in raw_notes:
        if "=" not in entry:
            raise ValueError(f"Malformed --note override '{entry}'. Use name=text.")
        name, text = entry.split("=", 1)
        name = name.strip()
        if not name:
            raise ValueError(f"Malformed --note override '{entry}'. Fixture name missing.")
        overrides[name] = text.strip()
    return overrides


def _fnv64_pcm16(payload: bytes) -> str:
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
    return f"{state:016x}"


def _load_manifest(path: Path) -> dict:
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


def _scan_fixtures(fixtures_root: Path) -> list[Path]:
    if not fixtures_root.exists():
        return []
    return sorted(p for p in fixtures_root.rglob("*.wav") if p.is_file())


def _read_pcm(path: Path) -> tuple[bytes, int, int, int]:
    with wave.open(str(path), "rb") as wav:
        sample_width = wav.getsampwidth()
        if sample_width != 2:
            raise ValueError(f"{path} is {sample_width * 8}-bit; expected 16-bit PCM")
        frames = wav.getnframes()
        frame_rate = wav.getframerate()
        channels = wav.getnchannels()
        payload = wav.readframes(frames)
    if len(payload) != frames * channels * 2:
        raise ValueError(f"Frame payload mismatch for {path}")
    return payload, frame_rate, frames, channels


def _merge_fixture(existing: dict[str, dict],
                   overrides: dict[str, str],
                   fixture_name: str,
                   data: dict) -> dict:
    merged = existing.get(fixture_name, {}).copy()
    merged.update(data)
    if fixture_name in overrides:
        merged["notes"] = overrides[fixture_name]
    elif "notes" not in merged:
        merged["notes"] = ""
    return merged


def compute_manifest(fixtures_root: Path,
                     manifest_path: Path,
                     note_overrides: dict[str, str]) -> tuple[dict, list[dict]]:
    manifest = _load_manifest(manifest_path)
    fixtures_root_list = manifest.get("fixtures", [])
    if not isinstance(fixtures_root_list, list):
        fixtures_root_list = []
    existing_map = {item.get("name", ""): item for item in fixtures_root_list if isinstance(item, dict)}

    fixtures = []
    for wav_path in _scan_fixtures(fixtures_root):
        payload, sample_rate, frames, channels = _read_pcm(wav_path)
        hash_hex = _fnv64_pcm16(payload)
        name = wav_path.stem
        fixtures.append(
            _merge_fixture(
                existing_map,
                note_overrides,
                name,
                {
                    "name": name,
                    "hash": hash_hex,
                    "wav_path": str(wav_path).replace("\\", "/"),
                    "sample_rate_hz": sample_rate,
                    "frames": frames,
                    "channels": channels,
                },
            )
        )

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
            "hash": "64-bit FNV-1a over PCM16 payload",
            "script": "scripts/compute_golden_hashes.py",
        }
    )
    manifest["tooling"] = tooling
    return manifest, fixtures


def render_table(fixtures: list[dict]) -> str:
    if not fixtures:
        return "(no fixtures discovered)"
    header = f"{'Fixture':20} {'Hash':18} {'Frames':>8} {'Rate':>8} {'Channels':>8}"
    lines = [header, "-" * len(header)]
    for item in fixtures:
        lines.append(
            f"{item['name']:20} {item['hash']:18} "
            f"{item['frames']:8d} {item['sample_rate_hz']:8d} {item['channels']:8d}"
        )
    return "\n".join(lines)


def main(argv: list[str]) -> int:
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
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(render_table(fixtures))
    if args.write:
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        with args.manifest.open("w", encoding="utf-8") as handle:
            json.dump(manifest, handle, indent=2)
            handle.write("\n")
        print(f"\nmanifest updated → {args.manifest}")
    else:
        print("\n(dry run — re-run with --write to update the manifest)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
