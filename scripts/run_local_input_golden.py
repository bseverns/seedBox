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


SCENARIO_MANIFEST = Path("docs/fixtures/external_input_scenarios.json")


def _load_scenarios(repo_root: Path) -> list[dict[str, object]]:
    manifest_path = repo_root / SCENARIO_MANIFEST
    with manifest_path.open("r", encoding="utf-8") as handle:
        manifest = json.load(handle)

    scenarios = manifest.get("scenarios")
    if not isinstance(scenarios, list):
        raise SystemExit(f"{manifest_path} must contain a scenarios list")

    seen: set[str] = set()
    loaded: list[dict[str, object]] = []
    for scenario in scenarios:
        if not isinstance(scenario, dict):
            raise SystemExit(f"{manifest_path} contains a non-object scenario entry")
        name = scenario.get("name")
        category = scenario.get("category")
        note = scenario.get("note")
        participates = scenario.get("golden_permutations")
        if not all(isinstance(value, str) and value for value in (name, category, note)):
            raise SystemExit(f"{manifest_path} scenario entries require non-empty name, category, and note")
        if not isinstance(participates, bool):
            raise SystemExit(f"{manifest_path} scenario {name} must set golden_permutations to true or false")
        if name in seen:
            raise SystemExit(f"{manifest_path} duplicates scenario name: {name}")
        seen.add(name)
        loaded.append(
            {
                "name": name,
                "category": category,
                "note": note,
                "golden_permutations": participates,
            }
        )
    return loaded


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
    parser.add_argument("--bpm", type=float, default=120.0, help="Musical BPM when using the golden permutation suite")
    parser.add_argument(
        "--suite",
        choices=["single", "golden-permutations"],
        default="single",
        help="Render one straight host pass or a local golden-style permutation suite",
    )
    parser.add_argument(
        "--scenario",
        default="mixed-boot",
        help="Scenario name when running --suite single",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Assume build/juce/seedbox_native_input_probe already exists",
    )
    args = parser.parse_args(argv)

    repo_root = Path(__file__).resolve().parent.parent
    manifest_scenarios = _load_scenarios(repo_root)
    manifest_by_name = {str(scenario["name"]): scenario for scenario in manifest_scenarios}
    input_path = args.input.expanduser().resolve()
    if not input_path.exists():
        raise SystemExit(f"input file does not exist: {input_path}")

    output_dir = (
        args.output_dir.expanduser().resolve()
        if args.output_dir
        else (input_path.parent / f"{input_path.stem}-seedbox-receipt").resolve()
    )
    output_dir.mkdir(parents=True, exist_ok=True)
    fixtures_dir = output_dir / "fixtures"
    fixtures_dir.mkdir(parents=True, exist_ok=True)

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

    output_wav = fixtures_dir / f"{input_path.stem}-native-output.wav"
    status_json = fixtures_dir / f"{input_path.stem}-native-status.json"
    summary_json = fixtures_dir / f"{input_path.stem}-native-summary.json"
    manifest_json = output_dir / "golden.local.json"
    browser_html = output_dir / "index.html"
    receipt_json = fixtures_dir / "input-receipt.json"

    if args.suite == "golden-permutations":
        scenarios = [scenario for scenario in manifest_scenarios if scenario["golden_permutations"]]
    else:
        scenario = manifest_by_name.get(args.scenario)
        if scenario is None:
            raise SystemExit(f"unknown scenario in {SCENARIO_MANIFEST}: {args.scenario}")
        scenarios = [scenario]
    if not scenarios:
        raise SystemExit(f"{SCENARIO_MANIFEST} does not define any scenarios for {args.suite}")
    clock_mode = "external-ppqn" if args.suite == "golden-permutations" else "internal-block"

    scenario_artifacts: list[dict[str, object]] = []
    for scenario in scenarios:
        scenario_name = scenario["name"]
        output_wav = fixtures_dir / f"{input_path.stem}-{scenario_name}-output.wav"
        status_json = fixtures_dir / f"{input_path.stem}-{scenario_name}-status.json"
        summary_json = fixtures_dir / f"{input_path.stem}-{scenario_name}-summary.json"

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
                "--scenario",
                scenario_name,
                "--clock-mode",
                clock_mode,
                "--bpm",
                str(args.bpm),
                "--block-size",
                str(args.block_size),
                "--sample-rate",
                str(args.sample_rate),
            ],
            repo_root,
        )

        scenario_artifacts.append(
            {
                "name": scenario_name,
                "category": scenario["category"],
                "note": scenario["note"],
                "output_path": str(output_wav),
                "output_sha256": _sha256(output_wav),
                "output_wav": _wav_meta(output_wav),
                "status_json_path": str(status_json),
                "status_json_sha256": _sha256(status_json),
                "summary_json_path": str(summary_json),
                "summary_json_sha256": _sha256(summary_json),
            }
        )

    source_meta = _wav_meta(input_path)
    receipt = {
        "kind": "external-input-local-golden",
        "suite": args.suite,
        "input_path": str(input_path),
        "input_sha256": _sha256(input_path),
        "input_wav": source_meta,
        "scenarios": scenario_artifacts,
        "render": {
            "block_size": args.block_size,
            "sample_rate": args.sample_rate,
            "bpm": args.bpm,
            "clock_mode": clock_mode,
        },
    }
    receipt_json.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")

    notes = {
        receipt_json.stem: f"Source/input identity ledger for {input_path.name}.",
    }
    for artifact in scenario_artifacts:
        stem_base = Path(str(artifact["output_path"])).stem.removesuffix("-output")
        output_stem = Path(str(artifact["output_path"])).stem
        status_stem = Path(str(artifact["status_json_path"])).stem
        summary_stem = Path(str(artifact["summary_json_path"])).stem
        notes[output_stem] = (
            f"{artifact['name']} local external-input golden from {input_path.name} "
            f"(sha256 {receipt['input_sha256'][:12]}..., block {args.block_size}, sr {args.sample_rate}, "
            f"clock {clock_mode}@{args.bpm:.1f}bpm; "
            f"{artifact['note']})"
        )
        notes[status_stem] = f"Final AppState status snapshot for {input_path.name} scenario {artifact['name']}."
        notes[summary_stem] = f"Render metrics summary for {input_path.name} scenario {artifact['name']}."

    cmd = [
        sys.executable,
        str((repo_root / "scripts" / "compute_golden_hashes.py").resolve()),
        "--fixtures-root",
        str(fixtures_dir),
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
    print("Scenarios:")
    for artifact in scenario_artifacts:
        print(f"  - {artifact['name']}: {artifact['output_path']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
