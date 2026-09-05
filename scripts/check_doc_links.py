#!/usr/bin/env python3
"""Check local Markdown link targets; external URLs and fragments are not fetched."""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
# Handles inline links/images, angle-wrapped destinations and one nested pair
# of parentheses (common in filenames). Reference definitions are checked too.
INLINE = re.compile(r'!?\[[^\]\n]*\]\(\s*(<[^>\n]+>|(?:\\.|[^\s()\\]|\([^()]*\))+)(?:\s+["\'][^\n]*?["\'])?\s*\)')
REFERENCE = re.compile(r'^ {0,3}\[[^\]\n]+\]:\s*(<[^>\n]+>|\S+)', re.M)
HTML = re.compile(r'\b(?:href|src)=["\']([^"\']+)["\']')
AUTOLINK = re.compile(r'<((?:file:|vscode:|/Users/|/home/|/tmp/|~/)[^>]+)>', re.I)
MACHINE = re.compile(r'^(?:~/|/(?:Users|home|private|tmp|var|Library|opt)(?:/|$)|[A-Za-z]:[\\/]|\\\\)')


def prose(text: str) -> str:
    lines = []
    fence = None
    for line in text.splitlines(keepends=True):
        marker = re.match(r'^\s{0,3}(`{3,}|~{3,})', line)
        if fence:
            if marker and marker[1][0] == fence[0] and len(marker[1]) >= len(fence):
                fence = None
            lines.append('\n')
        elif marker:
            fence = marker[1]
            lines.append('\n')
        else:
            # Preserve newlines for useful diagnostics; ignore links shown as code.
            lines.append(re.sub(r'(`+).*?\1', '', line))
    return ''.join(lines)


def check_file(root: Path, path: Path, known: set[Path]) -> list[str]:
    text = prose(path.read_text(encoding='utf-8'))
    errors = []
    for pattern in (INLINE, REFERENCE, HTML, AUTOLINK):
        for match in pattern.finditer(text):
            target = match[1].strip('<>')
            target = re.sub(r'\\([() ])', r'\1', target)
            line = text.count('\n', 0, match.start()) + 1
            location = f'{path.relative_to(root)}:{line}'
            if MACHINE.match(unquote(target)) or target.lower().startswith(('file:', 'vscode:')):
                errors.append(f'{location}: machine-specific link: {target}')
                continue
            parsed = urlsplit(target)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            local_path = unquote(parsed.path)
            candidate = (root / local_path.lstrip('/') if local_path.startswith('/')
                         else path.parent / local_path).resolve()
            if candidate != root and root not in candidate.parents:
                errors.append(f'{location}: link escapes repository: {target}')
            elif candidate not in known:
                errors.append(f'{location}: missing repository target: {target}')
    return errors


def repository_files(root: Path) -> list[Path]:
    result = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z', '--cached',
                                      '--others', '--exclude-standard'])
    return sorted({root / name.decode('utf-8') for name in result.split(b'\0') if name})


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    root = parser.parse_args().root.resolve()
    files = [path for path in repository_files(root) if path.is_file()]
    # Use repository entries, not arbitrary ignored build outputs on this machine.
    # This also detects filename-case errors on case-insensitive developer disks.
    known = set(files)
    for path in files:
        known.update(parent for parent in path.parents if parent == root or root in parent.parents)
    documents = [path for path in files if path.suffix.lower() == '.md']
    errors = [error for path in documents for error in check_file(root, path, known)]
    if errors:
        print('\n'.join(errors))
        return 1
    print(f'Local doc links OK: {len(documents)} Markdown files (file targets only; no external URL or fragment checks)')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
