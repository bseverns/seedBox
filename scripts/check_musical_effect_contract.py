#!/usr/bin/env python3
"""Check a native input probe summary against a scenario musical contract."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


DEFAULT_MANIFEST = Path("docs/fixtures/external_input_scenarios.json")


def _number(value: Any, default: float = 0.0) -> float:
    if isinstance(value, (int, float)):
        return float(value)
    return default


def _object(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def _load_scenario_contract(manifest_path: Path, scenario_name: str) -> dict[str, Any]:
    with manifest_path.open("r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    for scenario in manifest.get("scenarios", []):
        if isinstance(scenario, dict) and scenario.get("name") == scenario_name:
            contract = scenario.get("musical_contract")
            if not isinstance(contract, dict):
                raise SystemExit(f"scenario {scenario_name} has no musical_contract in {manifest_path}")
            return contract
    raise SystemExit(f"scenario {scenario_name} not found in {manifest_path}")


def _check(summary: dict[str, Any], contract: dict[str, Any]) -> list[str]:
    failures: list[str] = []
    render = _object(summary.get("render"))
    scenario = _object(summary.get("scenario"))
    block_rms = _object(render.get("blockRms"))

    left_rms = _number(render.get("leftRms"))
    right_rms = _number(render.get("rightRms"))
    output_rms = max(left_rms, right_rms)
    output_peak = _number(render.get("maxPeak"), max(_number(render.get("leftPeak")), _number(render.get("rightPeak"))))

    non_silence = _object(contract.get("non_silence"))
    min_output_rms = _number(non_silence.get("min_output_rms"))
    min_output_peak = _number(non_silence.get("min_output_peak"))
    if output_rms < min_output_rms:
        failures.append(f"output RMS {output_rms:.6f} below contract {min_output_rms:.6f}")
    if output_peak < min_output_peak:
        failures.append(f"output peak {output_peak:.6f} below contract {min_output_peak:.6f}")

    no_clipping = _object(contract.get("no_clipping"))
    max_output_peak = _number(no_clipping.get("max_output_peak"), 1.0)
    max_clipped_samples = int(_number(no_clipping.get("max_clipped_samples"), 0.0))
    clipped_samples = int(_number(render.get("clippedSampleCount"), 0.0))
    if output_peak > max_output_peak:
        failures.append(f"output peak {output_peak:.6f} exceeds contract {max_output_peak:.6f}")
    if clipped_samples > max_clipped_samples:
        failures.append(f"clipped sample count {clipped_samples} exceeds contract {max_clipped_samples}")

    input_output = _object(contract.get("input_output_difference"))
    min_input_output_diff = _number(input_output.get("min_mean_abs_diff"))
    input_output_diff = _number(render.get("inputOutputMeanAbsDiff"), _number(render.get("leftInputMeanAbsDiff")))
    if input_output_diff < min_input_output_diff:
        failures.append(
            f"input/output mean abs diff {input_output_diff:.6f} below contract {min_input_output_diff:.6f}"
        )

    stereo = _object(contract.get("stereo_difference"))
    if bool(stereo.get("required", False)):
        min_stereo_diff = _number(stereo.get("min_mean_abs_diff"))
        stereo_diff = _number(render.get("stereoMeanAbsDiff"))
        if stereo_diff < min_stereo_diff:
            failures.append(f"stereo mean abs diff {stereo_diff:.6f} below contract {min_stereo_diff:.6f}")

    temporal = _object(contract.get("temporal_variation"))
    if bool(temporal.get("required", False)):
        min_block_rms_range = _number(temporal.get("min_block_rms_range"))
        block_rms_range = _number(block_rms.get("range"))
        if block_rms_range < min_block_rms_range:
            failures.append(f"block RMS range {block_rms_range:.6f} below contract {min_block_rms_range:.6f}")

    reseed = _object(contract.get("reseed"))
    min_reseed_count = int(_number(reseed.get("min_count"), 0.0))
    reseed_count = int(_number(scenario.get("reseedCount"), 0.0))
    if reseed_count < min_reseed_count:
        failures.append(f"reseed count {reseed_count} below contract {min_reseed_count}")

    return failures


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--summary", type=Path, required=True, help="native_input_probe summary JSON")
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST, help="scenario manifest")
    parser.add_argument("--scenario", help="scenario name; defaults to summary.scenario.name")
    args = parser.parse_args(argv)

    repo_root = Path(__file__).resolve().parent.parent
    summary_path = args.summary.expanduser().resolve()
    manifest_path = args.manifest if args.manifest.is_absolute() else repo_root / args.manifest

    with summary_path.open("r", encoding="utf-8") as handle:
        summary = json.load(handle)
    summary_scenario = _object(summary.get("scenario"))
    scenario_name = args.scenario or summary_scenario.get("name")
    if not isinstance(scenario_name, str) or not scenario_name:
        raise SystemExit("scenario name missing; pass --scenario or include scenario.name in summary")

    contract = _load_scenario_contract(manifest_path, scenario_name)
    failures = _check(summary, contract)
    role = contract.get("role", "unknown-role")
    if failures:
        print(f"musical effect contract failed for {scenario_name} ({role}):", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1

    render = _object(summary.get("render"))
    block_rms = _object(render.get("blockRms"))
    print(
        "Musical effect contract passed for "
        f"{scenario_name} ({role}): rms={max(_number(render.get('leftRms')), _number(render.get('rightRms'))):.6f}, "
        f"peak={_number(render.get('maxPeak')):.6f}, "
        f"diff={_number(render.get('inputOutputMeanAbsDiff')):.6f}, "
        f"stereo={_number(render.get('stereoMeanAbsDiff')):.6f}, "
        f"blockRmsRange={_number(block_rms.get('range')):.6f}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
