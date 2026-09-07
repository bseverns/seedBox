#!/usr/bin/env python3
"""Validate cross-body Seed Card parity records without overstating coverage."""
from __future__ import annotations
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DIMENSIONS = {"lineage", "scheduling", "engine_choice", "control_behavior", "output_characteristics"}
BODIES = {"native", "juce", "hardware"}

def validate(root: Path) -> list[str]:
    root = root.resolve(); errors = []
    try: index = json.loads((root / "docs/cross_body_parity/index.json").read_text())
    except (OSError, json.JSONDecodeError) as error: return [f"parity index: {error}"]
    if index.get("format_version") != "1.0" or not isinstance(index.get("records"), list):
        return ["parity index: expected format_version 1.0 and records array"]
    for name in index["records"]:
        path = root / name
        try: record = json.loads(path.read_text())
        except (OSError, json.JSONDecodeError) as error: errors.append(f"{name}: {error}"); continue
        if record.get("format_version") != "1.0" or not isinstance(record.get("id"), str): errors.append(f"{name}: missing format_version or id")
        if not (root / record.get("seed_card", "")).is_file(): errors.append(f"{name}: missing Seed Card")
        dimensions = set(record.get("dimensions", []))
        if dimensions != DIMENSIONS: errors.append(f"{name}: dimensions must be the canonical five")
        bodies = record.get("bodies", {})
        if set(bodies) != BODIES: errors.append(f"{name}: native, juce, and hardware bodies are required"); continue
        observed = []
        for body, value in bodies.items():
            status = value.get("status") if isinstance(value, dict) else None
            if status not in {"observed", "unavailable", "not_run"}: errors.append(f"{name}.{body}: invalid status"); continue
            if status == "observed":
                observed.append(value)
                if not isinstance(value.get("evidence"), str) or not isinstance(value.get("findings"), dict) or set(value["findings"]) != DIMENSIONS:
                    errors.append(f"{name}.{body}: observed body needs evidence and all findings")
            elif not isinstance(value.get("reason"), str) or not value["reason"].strip(): errors.append(f"{name}.{body}: missing reason")
        claim = record.get("conclusion", "").lower()
        if len(observed) < 2 and "parity is not" not in claim: errors.append(f"{name}: cannot claim parity with fewer than two observed bodies")
    return errors

if __name__ == "__main__":
    errors = validate(ROOT)
    if errors: print("\n".join(errors), file=sys.stderr); raise SystemExit(1)
    print("Cross-body parity records OK")
