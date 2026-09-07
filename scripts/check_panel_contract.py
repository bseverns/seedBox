#!/usr/bin/env python3
"""Compile the Board contract and compare its builder-facing representations."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def compiler_command() -> list[str]:
    configured = os.environ.get("CXX")
    if configured:
        return shlex.split(configured)
    # An Intel-only Python running under Rosetta launches universal tools as
    # x86_64. On Apple Silicon that cannot use an ARM-only Command Line Tools
    # xcrun library, even though the SDK and headers are valid. Force Apple's
    # system C++ driver to arm64 when the host reports ARM capability.
    if sys.platform == "darwin":
        try:
            probe = subprocess.run(["arch", "-arm64", "/usr/bin/true"], capture_output=True)
            if probe.returncode == 0:
                return ["arch", "-arm64", "/usr/bin/c++"]
        except OSError:
            pass
    return ["c++"]


def load_contract(root: Path) -> list[tuple[str, str, str, str]]:
    with tempfile.TemporaryDirectory(prefix="seedbox-panel-") as directory:
        executable = Path(directory) / "panel-contract"
        subprocess.run([*compiler_command(), "-std=c++17",
                        "-DSEEDBOX_HW=0", "-I", str(root / "include"),
                        str(root / "tools/panel_contract_dump.cpp"), "-o", str(executable)], check=True)
        output = subprocess.check_output([str(executable)], text=True)
    return [tuple(line.split("\t")) for line in output.splitlines()]


def check_surfaces(root: Path, controls: list[tuple[str, str, str, str]]) -> list[str]:
    errors = []
    encoders = [row for row in controls if row[0] == "encoder"]
    buttons = [row for row in controls if row[0] == "button"]
    expected = [(label, token, pins) for _, label, token, pins in controls]
    guide = (root / "docs/panel_cheat_sheet.md").read_text()
    rows = re.findall(r"^\| ([^|]+) \| `([a-z]+)` \| ([0-9 /]+) \|$", guide, re.M)
    actual = [(label.strip(), token, pins.replace(" ", "")) for label, token, pins in rows]
    if actual != expected:
        errors.append("panel_cheat_sheet.md: control labels, tokens, order or GPIO table differs from PanelControls.h")

    svg = ET.parse(root / "assets/front-panel-map.svg").getroot()
    ns = {"s": "http://www.w3.org/2000/svg"}
    groups = {}
    shapes = []
    for element in svg.iter():
        if element.get("class") in ("encoder", "button"):
            shapes.append(element)
    for group in svg.findall(".//s:g", ns):
        if any(node.get("class") in ("encoder", "button") for node in group):
            token = group.get("id")
            if token in groups:
                errors.append(f"front-panel-map.svg: duplicate control group {token}")
            groups[token] = group
    if set(groups) != {row[2] for row in controls} or len(shapes) != len(controls):
        errors.append("front-panel-map.svg: missing, extra or ungrouped control")
    for kind, label, token, pins in controls:
        group = groups.get(token)
        if group is None:
            continue
        texts = ["".join(node.itertext()) for node in group.findall("s:text", ns)]
        if texts.count(label) != 1:
            errors.append(f"front-panel-map.svg: {token} must have exactly one '{label}' label")
        control_shapes = [node for node in group if node.get("class") in ("encoder", "button")]
        if len(control_shapes) != 1 or control_shapes[0].get("class") != kind:
            errors.append(f"front-panel-map.svg: {token} must be one {kind}")
        pin_text = pins.replace("/", " / ")
        if not any(text == pin_text or text.startswith(pin_text + " ·") for text in texts):
            errors.append(f"front-panel-map.svg: {token} GPIO annotation differs from {pins}")

    bom = (root / "docs/hardware_bill_of_materials.md").read_text()
    for item, count in (("Rotary encoders with integrated push buttons", len(encoders)),
                        ("Knobs for encoders", len(encoders)), ("Momentary push buttons", len(buttons))):
        quantities = re.findall(r"\| " + re.escape(item) + r" \(x(\d+)\) \|", bom)
        if quantities != [str(count)]:
            errors.append(f"hardware_bill_of_materials.md: expected {item} (x{count})")

    builder = (root / "docs/builder_bootstrap.md").read_text()
    encoder_descriptions = []
    for _, label, _, pins in encoders:
        a, b, switch = pins.split("/")
        encoder_descriptions.append(f"{label}: {a}/{b} + switch on {switch}")
    for row_name, expected_mapping in (
        ("Encoders", "; ".join(encoder_descriptions)),
        ("Buttons", ", ".join(f"{label}: {pins}" for _, label, _, pins in buttons)),
    ):
        mappings = re.findall(r"^\| " + row_name + r" \| ([^|]+) \|", builder, re.M)
        if [mapping.strip() for mapping in mappings] != [expected_mapping]:
            errors.append(f"builder_bootstrap.md: {row_name} mapping differs from PanelControls.h")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    root = parser.parse_args().root.resolve()
    try:
        controls = load_contract(root)
        errors = check_surfaces(root, controls)
    except (OSError, subprocess.CalledProcessError, ET.ParseError, ValueError) as error:
        print(f"Panel contract check failed: {error}")
        return 1
    if errors:
        print("\n".join(errors))
        return 1
    print(f"Panel contract OK: {len(controls)} controls; GPIO, guide, SVG, BOM and builder mappings agree")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
