#!/usr/bin/env python3
"""Validate external-input probe scenario metadata stays in sync."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
MANIFEST_PATH = REPO_ROOT / "docs" / "fixtures" / "external_input_scenarios.json"
DOC_PATH = REPO_ROOT / "docs" / "fixtures" / "external_input_scenarios.md"
CPP_PATH = REPO_ROOT / "tools" / "native_input_probe.cpp"


def _validate_number(contract: dict[str, object], path: str, failures: list[str], *, minimum: float = 0.0) -> None:
    current: object = contract
    for part in path.split("."):
        if not isinstance(current, dict) or part not in current:
            failures.append(f"musical_contract missing {path}")
            return
        current = current[part]
    if not isinstance(current, (int, float)) or isinstance(current, bool) or float(current) < minimum:
        failures.append(f"musical_contract {path} must be a number >= {minimum:g}")


def _validate_bool(contract: dict[str, object], path: str, failures: list[str]) -> None:
    current: object = contract
    for part in path.split("."):
        if not isinstance(current, dict) or part not in current:
            failures.append(f"musical_contract missing {path}")
            return
        current = current[part]
    if not isinstance(current, bool):
        failures.append(f"musical_contract {path} must be true or false")


def _validate_contract(name: str, scenario: dict[str, object], failures: list[str]) -> None:
    contract = scenario.get("musical_contract")
    if not isinstance(contract, dict):
        failures.append(f"manifest scenario {name} must define musical_contract")
        return
    role = contract.get("role")
    if not isinstance(role, str) or not role:
        failures.append(f"manifest scenario {name} musical_contract.role must be non-empty")

    contract_failures: list[str] = []
    for path in (
        "non_silence.min_output_rms",
        "non_silence.min_output_peak",
        "no_clipping.max_output_peak",
        "no_clipping.max_clipped_samples",
        "input_output_difference.min_mean_abs_diff",
        "stereo_difference.min_mean_abs_diff",
        "temporal_variation.min_block_rms_range",
        "reseed.min_count",
    ):
        _validate_number(contract, path, contract_failures)
    _validate_bool(contract, "stereo_difference.required", contract_failures)
    _validate_bool(contract, "temporal_variation.required", contract_failures)
    for failure in contract_failures:
        failures.append(f"manifest scenario {name} {failure}")


def _load_manifest() -> dict[str, dict[str, object]]:
    failures: list[str] = []
    with MANIFEST_PATH.open("r", encoding="utf-8") as handle:
        manifest = json.load(handle)

    scenarios = manifest.get("scenarios")
    if not isinstance(scenarios, list):
        raise ValueError(f"{MANIFEST_PATH.relative_to(REPO_ROOT)} must contain a scenarios list")

    by_name: dict[str, dict[str, object]] = {}
    for index, scenario in enumerate(scenarios):
        if not isinstance(scenario, dict):
            failures.append(f"manifest scenario #{index + 1} is not an object")
            continue

        name = scenario.get("name")
        category = scenario.get("category")
        note = scenario.get("note")
        participates = scenario.get("golden_permutations")
        if not isinstance(name, str) or not name:
            failures.append(f"manifest scenario #{index + 1} has an invalid name")
            continue
        if name in by_name:
            failures.append(f"manifest duplicates scenario name: {name}")
            continue
        if not isinstance(category, str) or not category:
            failures.append(f"manifest scenario {name} has an invalid category")
        if not isinstance(note, str) or not note:
            failures.append(f"manifest scenario {name} has an invalid note")
        if not isinstance(participates, bool):
            failures.append(f"manifest scenario {name} must set golden_permutations to true or false")
        _validate_contract(name, scenario, failures)
        by_name[name] = scenario

    if failures:
        raise ValueError("\n".join(failures))
    return by_name


def _load_cpp_scenarios() -> dict[str, dict[str, str]]:
    source = CPP_PATH.read_text(encoding="utf-8")
    table_match = re.search(
        r"static const std::array<ScenarioSpec,\s*\d+>\s+kScenarios\{\{(?P<table>.*?)\n\s*\}\};",
        source,
        re.DOTALL,
    )
    if not table_match:
        raise ValueError("could not find kScenarios table in tools/native_input_probe.cpp")

    entry_pattern = re.compile(
        r'\{\s*"(?P<name>[^"]+)",\s*"(?P<category>[^"]+)",\s*"(?P<note>(?:[^"\\]|\\.)*)",\s*&(?P<setup>\w+),\s*&(?P<before_block>\w+)\s*\}',
        re.DOTALL,
    )
    scenarios: dict[str, dict[str, str]] = {}
    for match in entry_pattern.finditer(table_match.group("table")):
        name = match.group("name")
        if name in scenarios:
            raise ValueError(f"C++ table duplicates scenario name: {name}")
        scenarios[name] = {
            "category": match.group("category"),
            "note": match.group("note"),
            "setup": match.group("setup"),
            "before_block": match.group("before_block"),
        }

    if not scenarios:
        raise ValueError("could not parse any C++ scenario entries")
    return scenarios


def _load_documented_names() -> list[str]:
    text = DOC_PATH.read_text(encoding="utf-8")
    section_match = re.search(
        r"^## Documented Scenario Set\s*(?P<section>.*?)(?:\n## |\Z)",
        text,
        re.MULTILINE | re.DOTALL,
    )
    if not section_match:
        raise ValueError("could not find 'Documented Scenario Set' in docs/fixtures/external_input_scenarios.md")

    names = re.findall(r"^\s*-\s+`([^`]+)`\s*$", section_match.group("section"), re.MULTILINE)
    if not names:
        raise ValueError("documented scenario set is empty")
    if len(set(names)) != len(names):
        raise ValueError("documented scenario set contains duplicate names")
    return names


def _set_diff(label: str, left: set[str], right_label: str, right: set[str]) -> list[str]:
    missing = sorted(left - right)
    if not missing:
        return []
    return [f"{label} not found in {right_label}: {', '.join(missing)}"]


def main() -> int:
    failures: list[str] = []
    try:
        manifest = _load_manifest()
        cpp = _load_cpp_scenarios()
        docs = _load_documented_names()
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"input scenario validation failed: {exc}", file=sys.stderr)
        return 1

    manifest_names = set(manifest)
    cpp_names = set(cpp)
    doc_names = set(docs)

    failures.extend(_set_diff("manifest scenarios", manifest_names, "C++ table", cpp_names))
    failures.extend(_set_diff("C++ scenarios", cpp_names, "manifest", manifest_names))
    failures.extend(_set_diff("manifest scenarios", manifest_names, "docs", doc_names))
    failures.extend(_set_diff("documented scenarios", doc_names, "manifest", manifest_names))

    for name in sorted(manifest_names & cpp_names):
        manifest_scenario = manifest[name]
        cpp_scenario = cpp[name]
        if manifest_scenario["category"] != cpp_scenario["category"]:
            failures.append(
                f"{name} category differs: manifest={manifest_scenario['category']!r}, "
                f"C++={cpp_scenario['category']!r}"
            )
        if manifest_scenario["note"] != cpp_scenario["note"]:
            failures.append(
                f"{name} note differs between manifest and C++ table"
            )

    if failures:
        print("input scenario validation failed:", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1

    print(
        "Validated external-input scenarios: "
        + ", ".join(name for name in docs)
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
