#!/usr/bin/env python3
"""Validate Seed Card registry entries and hash their checked-in evidence."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
CARD_DIR = Path("docs/seed_cards/cards")
REQUIRED_GENOME = {
    "pitch", "envA", "envD", "envS", "envR", "density", "probability",
    "jitterMs", "tone", "spread", "engine", "sampleIdx", "mutateAmt",
    "granular", "resonator",
}
REQUIRED_GRANULAR = {"grainSizeMs", "sprayMs", "transpose", "windowSkew", "stereoSpread", "source", "sdSlot"}
REQUIRED_RESONATOR = {"exciteMs", "damping", "brightness", "feedback", "mode", "bank"}
SLUG = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*\Z")
SHA256 = re.compile(r"[a-f0-9]{64}\Z")


def load_json(path: Path, errors: list[str]):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        errors.append(f"{path}: invalid JSON: {exc}")
        return None


def require_object(value, label: str, errors: list[str]):
    if not isinstance(value, dict):
        errors.append(f"{label}: expected an object")
        return None
    return value


def require_fields(value, fields: set[str], label: str, errors: list[str]):
    obj = require_object(value, label, errors)
    if obj is None:
        return None
    missing = sorted(fields - obj.keys())
    if missing:
        errors.append(f"{label}: missing {', '.join(missing)}")
    return obj


def evidence_path(root: Path, raw: object, label: str, errors: list[str]):
    if not isinstance(raw, str) or not raw:
        errors.append(f"{label}: path must be a non-empty repository-relative string")
        return None
    candidate = (root / raw).resolve()
    if Path(raw).is_absolute() or candidate == root or root not in candidate.parents:
        errors.append(f"{label}: path escapes repository: {raw}")
        return None
    if not candidate.is_file():
        errors.append(f"{label}: evidence file is missing: {raw}")
        return None
    return candidate


def validate_card(root: Path, relative: Path, errors: list[str]):
    path = root / relative
    card = load_json(path, errors)
    if not isinstance(card, dict):
        return None
    label = relative.as_posix()
    for field in ("format_version", "id", "name", "seed", "controls", "body", "evidence", "hashes", "comparison"):
        if field not in card:
            errors.append(f"{label}: missing {field}")
    if card.get("format_version") != "1.0":
        errors.append(f"{label}: format_version must be 1.0")
    card_id = card.get("id")
    if not isinstance(card_id, str) or not SLUG.fullmatch(card_id):
        errors.append(f"{label}: id must be a lowercase slug")
    elif card_id != relative.stem:
        errors.append(f"{label}: id must match the card filename")
    if not isinstance(card.get("name"), str) or not card["name"].strip():
        errors.append(f"{label}: name must be non-empty")

    seed = require_fields(card.get("seed"), {"master_seed", "id", "prng", "source", "lineage", "genome"}, f"{label}.seed", errors)
    if seed:
        for key in ("master_seed", "id", "prng", "lineage"):
            if not isinstance(seed.get(key), int) or seed[key] < 0:
                errors.append(f"{label}.seed.{key}: must be a non-negative integer")
        if seed.get("source") not in {"lfsr", "tap_tempo", "preset", "live_input"}:
            errors.append(f"{label}.seed.source: unsupported Seed::Source value")
        genome = require_fields(seed.get("genome"), REQUIRED_GENOME, f"{label}.seed.genome", errors)
        if genome:
            if not isinstance(genome.get("engine"), int) or not 0 <= genome["engine"] <= 5:
                errors.append(f"{label}.seed.genome.engine: must be an EngineRouter ID from 0 through 5")
            require_fields(genome.get("granular"), REQUIRED_GRANULAR, f"{label}.seed.genome.granular", errors)
            require_fields(genome.get("resonator"), REQUIRED_RESONATOR, f"{label}.seed.genome.resonator", errors)

    controls = card.get("controls")
    if not isinstance(controls, list) or not controls:
        errors.append(f"{label}.controls: must contain at least one relevant control")
    else:
        for index, control in enumerate(controls):
            require_fields(control, {"control", "value", "purpose"}, f"{label}.controls[{index}]", errors)

    body = require_fields(card.get("body"), {"kind", "target", "revision"}, f"{label}.body", errors)
    if body and body.get("kind") not in {"native", "juce", "hardware"}:
        errors.append(f"{label}.body.kind: expected native, juce, or hardware")

    evidence = require_fields(card.get("evidence"), {"audio_render", "control_ledger"}, f"{label}.evidence", errors)
    hashes = require_fields(card.get("hashes"), {"algorithm", "audio_render", "control_ledger"}, f"{label}.hashes", errors)
    if hashes and hashes.get("algorithm") != "sha256":
        errors.append(f"{label}.hashes.algorithm: must be sha256")
    if evidence and hashes:
        for kind in ("audio_render", "control_ledger"):
            entry = require_fields(evidence.get(kind), {"path", "description"}, f"{label}.evidence.{kind}", errors)
            expected = hashes.get(kind)
            if not isinstance(expected, str) or not SHA256.fullmatch(expected):
                errors.append(f"{label}.hashes.{kind}: must be 64 lowercase hexadecimal characters")
                continue
            if entry:
                artifact = evidence_path(root, entry.get("path"), f"{label}.evidence.{kind}", errors)
                if artifact and hashlib.sha256(artifact.read_bytes()).hexdigest() != expected:
                    errors.append(f"{label}.hashes.{kind}: does not match {entry['path']}")

    comparison = require_fields(card.get("comparison"), {"changed", "stayed_fixed"}, f"{label}.comparison", errors)
    if comparison:
        for field in ("changed", "stayed_fixed"):
            if not isinstance(comparison.get(field), str) or not comparison[field].strip():
                errors.append(f"{label}.comparison.{field}: must be a concise non-empty statement")
    return card_id if isinstance(card_id, str) else None


def validate_registry(root: Path) -> list[str]:
    root = root.resolve()
    errors: list[str] = []
    index_path = root / "docs/seed_cards/index.json"
    index = load_json(index_path, errors)
    if not isinstance(index, dict):
        return errors
    if index.get("format_version") != "1.0":
        errors.append("docs/seed_cards/index.json: format_version must be 1.0")
    cards = index.get("cards")
    if not isinstance(cards, list):
        errors.append("docs/seed_cards/index.json: cards must be an array")
        return errors
    registered: set[Path] = set()
    ids: set[str] = set()
    for value in cards:
        if not isinstance(value, str):
            errors.append("docs/seed_cards/index.json: card paths must be strings")
            continue
        relative = Path(value)
        if relative.is_absolute() or relative.parent != CARD_DIR or relative.suffix != ".json":
            errors.append(f"docs/seed_cards/index.json: card must be directly under {CARD_DIR}: {value}")
            continue
        if relative in registered:
            errors.append(f"docs/seed_cards/index.json: duplicate card path: {value}")
            continue
        registered.add(relative)
        card_id = validate_card(root, relative, errors)
        if card_id:
            if card_id in ids:
                errors.append(f"{relative}: duplicate card id: {card_id}")
            ids.add(card_id)
    actual = {path.relative_to(root) for path in (root / CARD_DIR).glob("*.json")}
    for path in sorted(actual - registered):
        errors.append(f"{path}: card is not listed in docs/seed_cards/index.json")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    root = parser.parse_args().root.resolve()
    errors = validate_registry(root)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    index = json.loads((root / "docs/seed_cards/index.json").read_text())
    print(f"Seed Cards OK: {len(index['cards'])} registered card(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
