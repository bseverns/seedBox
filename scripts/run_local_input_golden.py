#!/usr/bin/env python3
"""Run a local-only external-input pass through the native golden manifest flow."""

from __future__ import annotations

import argparse
import contextlib
import hashlib
import json
import subprocess
import sys
import wave
from pathlib import Path


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _wav_meta(path: Path) -> dict[str, object]:
    with contextlib.closing(wave.open(str(path), "rb")) as wav:
        frames = wav.getnframes()
        sample_rate = wav.getframerate()
        return {
            "channels": wav.getnchannels(),
            "sample_rate": sample_rate,
            "frames": frames,
            "duration_s": round(frames / float(sample_rate), 6) if sample_rate > 0 else 0.0,
            "sample_width": wav.getsampwidth(),
        }


def _run(cmd: list[str], cwd: Path) -> None:
    subprocess.check_call(cmd, cwd=str(cwd))


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True, help="External WAV to feed through the native host stack")
    parser.add_argument(
        "--output-dir",
        type=Path,
        help="Where the local receipt/golden artifacts should land "
        "(default: sibling '<stem>-seedbox-receipt' folder beside the input)",
    )
    parser.add_argument("--block-size", type=int, default=256, help="Host block size for the probe render")
    parser.add_argument("--sample-rate", type=int, default=48000, help="Target render sample rate")
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Assume build/juce/seedbox_native_input_probe already exists",
    )
    args = parser.parse_args(argv)

    repo_root = Path(__file__).resolve().parent.parent
    input_path = args.input.expanduser().resolve()
    if not input_path.exists():
        raise SystemExit(f"input file does not exist: {input_path}")

    output_dir = (
        args.output_dir.expanduser().resolve()
        if args.output_dir
        else (input_path.parent / f"{input_path.stem}-seedbox-receipt").resolve()
    )
    output_dir.mkdir(parents=True, exist_ok=True)

    probe = repo_root / "build" / "juce" / "seedbox_native_input_probe"
    if not args.skip_build:
      _run(
          [
              "cmake",
              "--build",
              "build/juce",
              "--config",
              "Release",
              "--target",
              "seedbox_native_input_probe",
          ],
          repo_root,
      )
    if not probe.exists():
        raise SystemExit(f"probe binary is missing: {probe}")

    output_wav = output_dir / f"{input_path.stem}-native-output.wav"
    status_json = output_dir / f"{input_path.stem}-native-status.json"
    summary_json = output_dir / f"{input_path.stem}-native-summary.json"
    manifest_json = output_dir / "golden.local.json"
    browser_html = output_dir / "index.html"
    receipt_json = output_dir / "input-receipt.json"

    _run(
        [
            str(probe),
            "--input",
            str(input_path),
            "--output",
            str(output_wav),
            "--status-json",
            str(status_json),
            "--summary-json",
            str(summary_json),
            "--block-size",
            str(args.block_size),
            "--sample-rate",
            str(args.sample_rate),
        ],
        repo_root,
    )

    source_meta = _wav_meta(input_path)
    output_meta = _wav_meta(output_wav)
    receipt = {
        "kind": "external-input-local-golden",
        "input_path": str(input_path),
        "input_sha256": _sha256(input_path),
        "input_wav": source_meta,
        "probe_output_path": str(output_wav),
        "probe_output_sha256": _sha256(output_wav),
        "probe_output_wav": output_meta,
        "status_json_path": str(status_json),
        "status_json_sha256": _sha256(status_json),
        "summary_json_path": str(summary_json),
        "summary_json_sha256": _sha256(summary_json),
        "render": {
            "block_size": args.block_size,
            "sample_rate": args.sample_rate,
        },
    }
    receipt_json.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")

    notes = {
        output_wav.stem: f"Local external-input golden from {input_path.name} "
                         f"(sha256 {receipt['input_sha256'][:12]}..., block {args.block_size}, sr {args.sample_rate}).",
        status_json.stem: f"Final AppState status snapshot for {input_path.name}.",
        summary_json.stem: f"Render metrics summary for {input_path.name}.",
        receipt_json.stem: f"Source/input identity ledger for {input_path.name}.",
    }

    cmd = [
        sys.executable,
        str((repo_root / "scripts" / "compute_golden_hashes.py").resolve()),
        "--fixtures-root",
        str(output_dir),
        "--manifest",
        str(manifest_json),
        "--browser-output",
        str(browser_html),
        "--write",
        "--skip-header",
    ]
    for name, note in notes.items():
        cmd.extend(["--note", f"{name}={note}"])
    _run(cmd, repo_root)

    print(f"Local golden manifest: {manifest_json}")
    print(f"Local golden browser: {browser_html}")
    print(f"Local receipt ledger: {receipt_json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
