#!/usr/bin/env python3
"""Print the defaults and stories declared in include/SeedBoxConfig.h.

This helper keeps docs + CI honest by reading the canonical header and
emitting the flag table as either plaintext or Markdown.  The goal: never let a
stale README fib about what the build toggles actually do.
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys
from typing import Dict, Iterable, Tuple

ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "include" / "SeedBoxConfig.h"

# Known toggles we expect to surface, mapped to their friendly order.
FLAG_ORDER = (
    "SEEDBOX_HW",
    "SEEDBOX_SIM",
    "QUIET_MODE",
    "ENABLE_GOLDEN",
    "SEEDBOX_DEBUG_CLOCK_SOURCE",
    "SEEDBOX_DEBUG_UI",
)

MACRO_TEMPLATE = r"#define\s+{flag}\s+(?P<value>[^\s]+)"
SUMMARY_PATTERN = re.compile(
    r"\{\"(?P<flag>[^\"]+)\",\s*[^,]+,\s*\"(?P<story>[^\"]*)\"\}",
    re.MULTILINE | re.DOTALL,
)


def _extract_macros(text: str) -> Dict[str, str]:
    results: Dict[str, str] = {}
    for flag in FLAG_ORDER:
        pattern = re.compile(MACRO_TEMPLATE.format(flag=re.escape(flag)))
        match = pattern.search(text)
        if not match:
            raise SystemExit(f"Flag {flag} missing from {HEADER_PATH}")
        results[flag] = match.group("value")
    return results


def _extract_stories(text: str) -> Dict[str, str]:
    stories: Dict[str, str] = {}
    for match in SUMMARY_PATTERN.finditer(text):
        flag = match.group("flag")
        story = match.group("story")
        # The initializer uses escaped quotes for possessives and punctuation.
        stories[flag] = story.encode("utf-8").decode("unicode_escape")
    return stories


def _iter_rows(text: str) -> Iterable[Tuple[str, str, str]]:
    macros = _extract_macros(text)
    stories = _extract_stories(text)

    missing_story = [flag for flag in FLAG_ORDER if flag not in stories]
    if missing_story:
        raise SystemExit(
            "Flag stories missing from kFlagMatrix: " + ", ".join(missing_story)
        )

    for flag in FLAG_ORDER:
        yield flag, macros[flag], stories[flag]


def _as_plain(rows: Iterable[Tuple[str, str, str]]) -> str:
    lines = [
        "SeedBox build flag defaults (source: include/SeedBoxConfig.h)",
        "-----------------------------------------------------------",
    ]
    for flag, value, story in rows:
        lines.append(f"{flag} = {value}\n  {story}")
    return "\n".join(lines)


def _as_markdown(rows: Iterable[Tuple[str, str, str]]) -> str:
    table_lines = ["| Flag | Default | Story |", "| --- | --- | --- |"]
    for flag, value, story in rows:
        table_lines.append(f"| `{flag}` | `{value}` | {story} |")
    return "\n".join(table_lines)


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--format",
        choices=("plain", "markdown"),
        default="plain",
        help="Render as plain text (default) or Markdown table.",
    )
    args = parser.parse_args(list(argv))

    if not HEADER_PATH.exists():
        raise SystemExit(f"Cannot find {HEADER_PATH}")

    text = HEADER_PATH.read_text(encoding="utf-8")
    rows = list(_iter_rows(text))

    if args.format == "markdown":
        output = _as_markdown(rows)
    else:
        output = _as_plain(rows)

    sys.stdout.write(output)
    if not output.endswith("\n"):
        sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
