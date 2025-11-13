#!/usr/bin/env python3
"""Emit the Markdown docs we expect spell-checkers and linters to cover.

CI jobs and local pre-flight hooks can shell out to this script so the allowlist
stays in one place. We name-check the headline teaching docs explicitly (the
ones that go stale fastest) and then add every other Markdown file living under
`docs/`.

Usage examples:

    # basic newline-delimited list for codespell or vale
    python scripts/doc_spellcheck_targets.py

    # pass to codespell directly
    codespell --builtin clear $(python scripts/doc_spellcheck_targets.py)
"""

from __future__ import annotations

from pathlib import Path
import sys

REPO_ROOT = Path(__file__).resolve().parents[1]
DOC_ROOT = REPO_ROOT / "docs"

# High-signal docs we always want at the top of the list so failures surface
# loudly in CI output.
PRIORITY_DOCS = [
    Path("docs/calibration_guide.md"),
    Path("docs/wiring_gallery.md"),
    Path("docs/troubleshooting_log.md"),
    Path("docs/builder_bootstrap.md"),
    Path("docs/hardware_bill_of_materials.md"),
]


def _iter_docs() -> list[Path]:
    docs: list[Path] = []

    for path in PRIORITY_DOCS:
        absolute = REPO_ROOT / path
        if not absolute.exists():
            print(f"warning: expected doc missing: {path}", file=sys.stderr)
            continue
        docs.append(path)

    if DOC_ROOT.exists():
        for path in sorted(DOC_ROOT.rglob("*.md")):
            rel = path.relative_to(REPO_ROOT)
            if rel not in docs:
                docs.append(rel)

    return docs


def main() -> int:
    docs = _iter_docs()
    if not docs:
        print("warning: no documentation files discovered", file=sys.stderr)
        return 1

    for path in docs:
        print(path.as_posix())
    return 0


if __name__ == "__main__":
    sys.exit(main())
