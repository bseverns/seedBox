#!/usr/bin/env python3
"""Build and smoke-test seedbox_native_input_probe with a generated WAV."""

from __future__ import annotations

import argparse
import json
import math
import shutil
import struct
import subprocess
import sys
import wave
from pathlib import Path


def _run(cmd: list[str], cwd: Path) -> None:
    print("+ " + " ".join(cmd), flush=True)
    subprocess.check_call(cmd, cwd=str(cwd))


def _write_tiny_wav(path: Path, sample_rate: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    duration_seconds = 0.5
    frames = int(sample_rate * duration_seconds)
    frequency_hz = 220.0
    amplitude = 0.35

    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        for frame in range(frames):
            t = frame / float(sample_rate)
            envelope = min(1.0, frame / 240.0, (frames - frame) / 240.0)
            sample = int(round(math.sin(2.0 * math.pi * frequency_hz * t) * amplitude * envelope * 32767.0))
            wav.writeframesraw(struct.pack("<h", sample))


def _probe_path(build_dir: Path, config: str) -> Path:
    suffix = ".exe" if sys.platform == "win32" else ""
    candidates = [
        build_dir / f"seedbox_native_input_probe{suffix}",
        build_dir / config / f"seedbox_native_input_probe{suffix}",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate

    found = list(build_dir.rglob(f"seedbox_native_input_probe{suffix}"))
    if found:
        return found[0]
    raise SystemExit(f"seedbox_native_input_probe binary not found under {build_dir}")


def _assert_file(path: Path, label: str) -> None:
    if not path.exists():
        raise SystemExit(f"{label} missing: {path}")
    if path.stat().st_size <= 0:
        raise SystemExit(f"{label} is empty: {path}")


def _assert_output_wav(path: Path) -> None:
    _assert_file(path, "output WAV")
    with wave.open(str(path), "rb") as wav:
        if wav.getnframes() <= 0:
            raise SystemExit(f"output WAV has no frames: {path}")
        if wav.getsampwidth() != 2:
            raise SystemExit(f"output WAV is not PCM16: {path}")


def _assert_json_outputs(status_path: Path, summary_path: Path) -> None:
    _assert_file(status_path, "status JSON")
    _assert_file(summary_path, "summary JSON")

    with status_path.open("r", encoding="utf-8") as handle:
        status = json.load(handle)
    if "hostDiagnostics" not in status:
        raise SystemExit("status JSON does not contain hostDiagnostics")

    with summary_path.open("r", encoding="utf-8") as handle:
        summary = json.load(handle)
    render = summary.get("render")
    if not isinstance(render, dict):
        raise SystemExit("summary JSON does not contain render metrics")

    left_rms = float(render.get("leftRms", 0.0) or 0.0)
    right_rms = float(render.get("rightRms", 0.0) or 0.0)
    left_peak = float(render.get("leftPeak", 0.0) or 0.0)
    right_peak = float(render.get("rightPeak", 0.0) or 0.0)
    if max(left_rms, right_rms, left_peak, right_peak) <= 0.0:
        raise SystemExit("probe output appears silent according to summary JSON")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build/juce"), help="CMake build directory")
    parser.add_argument(
        "--work-dir",
        type=Path,
        default=Path("build/native_input_probe_smoke"),
        help="Scratch directory for generated smoke-test input and outputs",
    )
    parser.add_argument("--config", default="Release", help="CMake build configuration")
    parser.add_argument("--skip-configure", action="store_true", help="Use an already configured CMake build directory")
    parser.add_argument("--skip-build", action="store_true", help="Use an already built seedbox_native_input_probe")
    args = parser.parse_args(argv)

    repo_root = Path(__file__).resolve().parent.parent
    build_dir = (repo_root / args.build_dir).resolve()
    work_dir = (repo_root / args.work_dir).resolve()
    sample_rate = 48000

    if not args.skip_configure:
        _run(
            [
                "cmake",
                "-S",
                ".",
                "-B",
                str(build_dir),
                f"-DCMAKE_BUILD_TYPE={args.config}",
            ],
            repo_root,
        )

    if not args.skip_build:
        _run(
            [
                "cmake",
                "--build",
                str(build_dir),
                "--config",
                args.config,
                "--target",
                "seedbox_native_input_probe",
            ],
            repo_root,
        )

    probe = _probe_path(build_dir, args.config)
    if work_dir.exists():
        shutil.rmtree(work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)

    input_wav = work_dir / "tiny-input.wav"
    output_wav = work_dir / "tiny-output.wav"
    status_json = work_dir / "tiny-status.json"
    summary_json = work_dir / "tiny-summary.json"
    _write_tiny_wav(input_wav, sample_rate)

    _run(
        [
            str(probe),
            "--input",
            str(input_wav),
            "--output",
            str(output_wav),
            "--status-json",
            str(status_json),
            "--summary-json",
            str(summary_json),
            "--scenario",
            "granular-live",
            "--clock-mode",
            "internal-block",
            "--block-size",
            "256",
            "--sample-rate",
            str(sample_rate),
        ],
        repo_root,
    )

    _assert_output_wav(output_wav)
    _assert_json_outputs(status_json, summary_json)

    print(f"Native input probe smoke passed: {work_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
