#!/usr/bin/env python3
"""Generate a browsable HTML index for native golden fixtures."""

from __future__ import annotations

import argparse
import html
import json
import os
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List


def _load_manifest(path: Path) -> Dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError("Manifest root must be a JSON object")
    return data


def _fixture_path(root: Path, fixture_path: str) -> Path:
    normalized = fixture_path.replace("\\", "/")
    return (root / normalized).resolve()


def _href(output_path: Path, target_path: Path) -> str:
    return html.escape(os.path.relpath(target_path, start=output_path.parent).replace("\\", "/"), quote=True)


def _read_preview(path: Path, max_lines: int = 20, max_chars: int = 2000) -> str:
    if not path.exists():
        return "[missing artifact on disk]"
    body = path.read_text(encoding="utf-8", errors="replace")
    lines = body.splitlines()
    preview = "\n".join(lines[:max_lines])
    if len(preview) > max_chars:
        preview = preview[:max_chars].rstrip() + "\n..."
    elif len(lines) > max_lines:
        preview += "\n..."
    return preview


def _duration_text(item: Dict[str, Any]) -> str:
    frames = item.get("frames")
    sample_rate = item.get("sample_rate_hz")
    if not isinstance(frames, int) or not isinstance(sample_rate, int) or sample_rate <= 0:
        return "n/a"
    total_seconds = frames / float(sample_rate)
    minutes = int(total_seconds // 60)
    seconds = total_seconds - (minutes * 60)
    if minutes > 0:
        return f"{minutes}:{seconds:05.2f}"
    return f"{total_seconds:.2f}s"


def _summary_chip(label: str, value: str) -> str:
    return (
        '<span class="chip">'
        f'<span class="chip-label">{html.escape(label)}</span>'
        f'<span class="chip-value">{html.escape(value)}</span>'
        "</span>"
    )


def _notes_block(item: Dict[str, Any]) -> str:
    notes = str(item.get("notes", "") or "").strip()
    if not notes:
        return '<p class="notes muted">No liner notes yet.</p>'
    return f'<p class="notes">{html.escape(notes)}</p>'


def _audio_cards(fixtures: Iterable[Dict[str, Any]], logs_by_name: Dict[str, Dict[str, Any]], repo_root: Path,
                 output_path: Path) -> str:
    cards: List[str] = []
    for item in fixtures:
        if item.get("kind") != "audio":
            continue
        name = str(item.get("name", "unnamed"))
        artifact_path = _fixture_path(repo_root, str(item.get("path", "")))
        control_name = f"{name}-control"
        companion = logs_by_name.get(control_name)

        chips = [
            _summary_chip("Duration", _duration_text(item)),
            _summary_chip("Rate", f"{item.get('sample_rate_hz', '?')} Hz"),
            _summary_chip("Layout", str(item.get("channel_layout", item.get("channels", "?")))),
            _summary_chip("Frames", str(item.get("frames", "?"))),
            _summary_chip("Hash", str(item.get("hash", "?"))),
        ]

        companion_block = ""
        if companion is not None:
            companion_path = _fixture_path(repo_root, str(companion.get("path", "")))
            companion_preview = html.escape(_read_preview(companion_path))
            companion_block = f"""
            <details class="ledger-preview">
              <summary>Companion ledger: {html.escape(control_name)} ({html.escape(str(companion.get("lines", "?")))} lines)</summary>
              <div class="ledger-actions">
                <a href="{_href(output_path, companion_path)}">Open ledger</a>
                <span>{html.escape(str(companion.get("bytes", "?")))} bytes</span>
              </div>
              <pre>{companion_preview}</pre>
            </details>
            """

        cards.append(
            f"""
            <article class="fixture-card audio-card" data-terms="{html.escape((name + ' ' + str(item.get('notes', '')) + ' ' + str(item.get('path', ''))).lower(), quote=True)}">
              <div class="card-topline">
                <p class="eyebrow">Audio fixture</p>
                <h2>{html.escape(name)}</h2>
              </div>
              {_notes_block(item)}
              <div class="chip-row">
                {''.join(chips)}
              </div>
              <audio controls preload="none" src="{_href(output_path, artifact_path)}"></audio>
              <div class="artifact-links">
                <a href="{_href(output_path, artifact_path)}">Open WAV</a>
                <a href="{_href(output_path, repo_root / 'tests/native_golden/golden.json')}">Manifest entry source</a>
              </div>
              <p class="pathline">{html.escape(str(item.get("path", "")))}</p>
              {companion_block}
            </article>
            """
        )
    return "\n".join(cards)


def _log_cards(fixtures: Iterable[Dict[str, Any]], paired_log_names: Iterable[str], repo_root: Path,
               output_path: Path) -> str:
    paired = set(paired_log_names)
    cards: List[str] = []
    for item in fixtures:
        if item.get("kind") != "log":
            continue
        name = str(item.get("name", "unnamed"))
        if name in paired:
            continue
        artifact_path = _fixture_path(repo_root, str(item.get("path", "")))
        preview = html.escape(_read_preview(artifact_path))
        chips = [
            _summary_chip("Lines", str(item.get("lines", "?"))),
            _summary_chip("Bytes", str(item.get("bytes", "?"))),
            _summary_chip("Hash", str(item.get("hash", "?"))),
        ]
        cards.append(
            f"""
            <article class="fixture-card log-card" data-terms="{html.escape((name + ' ' + str(item.get('notes', '')) + ' ' + str(item.get('path', ''))).lower(), quote=True)}">
              <div class="card-topline">
                <p class="eyebrow">Standalone log</p>
                <h2>{html.escape(name)}</h2>
              </div>
              {_notes_block(item)}
              <div class="chip-row">
                {''.join(chips)}
              </div>
              <div class="artifact-links">
                <a href="{_href(output_path, artifact_path)}">Open log</a>
              </div>
              <p class="pathline">{html.escape(str(item.get("path", "")))}</p>
              <pre>{preview}</pre>
            </article>
            """
        )
    if not cards:
        return '<p class="empty-state">No standalone log artifacts beyond the companion control ledgers.</p>'
    return "\n".join(cards)


def render_browser(manifest: Dict[str, Any], repo_root: Path, output_path: Path) -> str:
    fixtures = manifest.get("fixtures", [])
    if not isinstance(fixtures, list):
        raise ValueError("Manifest fixtures field must be a list")

    audio = [item for item in fixtures if isinstance(item, dict) and item.get("kind") == "audio"]
    logs = [item for item in fixtures if isinstance(item, dict) and item.get("kind") == "log"]
    logs_by_name = {str(item.get("name", "")): item for item in logs}
    paired_log_names = [f"{item.get('name', '')}-control" for item in audio]

    generated_at = str(manifest.get("generated_at_utc", "unknown"))
    tooling = manifest.get("tooling", {})
    if not isinstance(tooling, dict):
        tooling = {}
    last_updated_by = str(manifest.get("last_updated_by", tooling.get("script", "unknown")))
    total_audio_seconds = 0.0
    for item in audio:
        frames = item.get("frames")
        sample_rate = item.get("sample_rate_hz")
        if isinstance(frames, int) and isinstance(sample_rate, int) and sample_rate > 0:
            total_audio_seconds += frames / float(sample_rate)

    audio_cards = _audio_cards(audio, logs_by_name, repo_root, output_path)
    log_cards = _log_cards(logs, paired_log_names, repo_root, output_path)
    manifest_href = _href(output_path, repo_root / "tests/native_golden/golden.json")
    readme_href = _href(output_path, repo_root / "tests/native_golden/README.md")
    gallery_href = _href(output_path, repo_root / "docs/SeedGallery.md")

    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>SeedBox Golden Fixture Browser</title>
  <style>
    :root {{
      --ink: #101820;
      --paper: #f5efe2;
      --panel: rgba(255, 250, 242, 0.9);
      --panel-strong: rgba(255, 246, 233, 0.96);
      --line: rgba(16, 24, 32, 0.14);
      --muted: #5f6b70;
      --accent: #cc5a2c;
      --accent-2: #127a72;
      --shadow: rgba(16, 24, 32, 0.12);
    }}
    * {{ box-sizing: border-box; }}
    body {{
      margin: 0;
      color: var(--ink);
      background:
        radial-gradient(circle at top left, rgba(18, 122, 114, 0.22), transparent 32%),
        radial-gradient(circle at top right, rgba(204, 90, 44, 0.18), transparent 28%),
        linear-gradient(180deg, #efe6d2 0%, #f9f5ec 42%, #efe7d8 100%);
      font-family: "Avenir Next", "Trebuchet MS", "Segoe UI", sans-serif;
      line-height: 1.45;
    }}
    .shell {{
      max-width: 1360px;
      margin: 0 auto;
      padding: 28px 20px 64px;
    }}
    .hero {{
      position: relative;
      overflow: hidden;
      border: 1px solid var(--line);
      border-radius: 28px;
      padding: 28px;
      background:
        linear-gradient(135deg, rgba(16, 24, 32, 0.96), rgba(26, 44, 51, 0.94)),
        linear-gradient(120deg, rgba(204, 90, 44, 0.24), transparent 55%);
      color: #f5efe2;
      box-shadow: 0 24px 60px var(--shadow);
    }}
    .hero::after {{
      content: "";
      position: absolute;
      inset: auto -40px -70px auto;
      width: 220px;
      height: 220px;
      border-radius: 50%;
      background: radial-gradient(circle, rgba(204, 90, 44, 0.34), transparent 68%);
      pointer-events: none;
    }}
    .eyebrow {{
      margin: 0 0 8px;
      text-transform: uppercase;
      letter-spacing: 0.18em;
      font-size: 0.78rem;
      color: inherit;
      opacity: 0.8;
    }}
    h1 {{
      margin: 0;
      font-family: "Iowan Old Style", "Palatino Linotype", serif;
      font-size: clamp(2.4rem, 5vw, 4.4rem);
      line-height: 0.95;
      max-width: 12ch;
    }}
    .hero-copy {{
      max-width: 72ch;
      margin: 14px 0 0;
      color: rgba(245, 239, 226, 0.88);
      font-size: 1.03rem;
    }}
    .hero-meta {{
      display: flex;
      flex-wrap: wrap;
      gap: 12px;
      margin-top: 22px;
    }}
    .stat {{
      min-width: 132px;
      padding: 12px 14px;
      border-radius: 16px;
      background: rgba(255, 255, 255, 0.08);
      border: 1px solid rgba(255, 255, 255, 0.08);
    }}
    .stat strong {{
      display: block;
      font-size: 1.05rem;
    }}
    .hero-links {{
      display: flex;
      flex-wrap: wrap;
      gap: 12px;
      margin-top: 22px;
    }}
    a {{
      color: inherit;
    }}
    .hero-links a,
    .artifact-links a {{
      text-decoration: none;
      border-bottom: 1px solid currentColor;
    }}
    .controls {{
      display: grid;
      grid-template-columns: minmax(240px, 420px) auto;
      gap: 16px;
      align-items: center;
      margin: 26px 0 18px;
    }}
    .search {{
      width: 100%;
      padding: 14px 16px;
      border: 1px solid var(--line);
      border-radius: 999px;
      background: rgba(255, 255, 255, 0.75);
      color: var(--ink);
      font: inherit;
      box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.4);
    }}
    .tool-card {{
      margin: 0 0 30px;
      padding: 20px;
      border-radius: 24px;
      border: 1px solid var(--line);
      background:
        linear-gradient(180deg, rgba(241, 248, 246, 0.96), rgba(249, 242, 232, 0.96));
      box-shadow: 0 20px 36px rgba(16, 24, 32, 0.08);
    }}
    .tool-grid {{
      display: grid;
      grid-template-columns: minmax(0, 1.1fr) minmax(260px, 0.9fr);
      gap: 22px;
      align-items: start;
    }}
    .tool-card h2 {{
      margin: 0 0 10px;
      font-family: "Iowan Old Style", "Palatino Linotype", serif;
      font-size: clamp(1.7rem, 3vw, 2.2rem);
    }}
    .tool-card p {{
      margin: 0 0 12px;
    }}
    .tone-form {{
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 12px;
    }}
    .tone-form label {{
      display: flex;
      flex-direction: column;
      gap: 6px;
      font-size: 0.94rem;
      color: var(--muted);
    }}
    .tone-form label.full {{
      grid-column: 1 / -1;
    }}
    .tone-form input {{
      width: 100%;
      padding: 12px 13px;
      border-radius: 14px;
      border: 1px solid rgba(16, 24, 32, 0.16);
      background: rgba(255, 255, 255, 0.84);
      color: var(--ink);
      font: inherit;
    }}
    .tone-actions {{
      display: flex;
      flex-wrap: wrap;
      gap: 12px;
      margin-top: 16px;
      align-items: center;
    }}
    .tone-button {{
      border: 0;
      border-radius: 999px;
      padding: 12px 18px;
      background: var(--accent);
      color: #fff7ef;
      font: inherit;
      font-weight: 700;
      cursor: pointer;
      box-shadow: 0 10px 22px rgba(204, 90, 44, 0.24);
    }}
    .tone-button[disabled] {{
      opacity: 0.65;
      cursor: wait;
    }}
    .tone-status {{
      min-height: 1.4em;
      font-size: 0.94rem;
    }}
    .tone-status.error {{
      color: #9d2e12;
    }}
    .tone-status.ok {{
      color: #127a72;
    }}
    .tool-callout {{
      padding: 14px 16px;
      border-radius: 18px;
      background: rgba(16, 24, 32, 0.06);
      border: 1px solid rgba(16, 24, 32, 0.08);
    }}
    .tool-callout code {{
      display: inline-block;
      margin-top: 6px;
      white-space: pre-wrap;
    }}
    .run-log {{
      margin-top: 16px;
    }}
    .section-heading {{
      display: flex;
      justify-content: space-between;
      align-items: baseline;
      gap: 12px;
      margin: 28px 0 14px;
    }}
    .section-heading h2 {{
      margin: 0;
      font-family: "Iowan Old Style", "Palatino Linotype", serif;
      font-size: clamp(1.7rem, 3.2vw, 2.3rem);
    }}
    .section-heading p {{
      margin: 0;
      color: var(--muted);
    }}
    .grid {{
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 18px;
    }}
    .fixture-card {{
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 22px;
      padding: 18px;
      box-shadow: 0 20px 36px rgba(16, 24, 32, 0.08);
      backdrop-filter: blur(10px);
    }}
    .audio-card {{
      background:
        linear-gradient(180deg, rgba(255, 250, 242, 0.98), rgba(248, 242, 231, 0.96));
    }}
    .log-card {{
      background:
        linear-gradient(180deg, rgba(244, 249, 247, 0.96), rgba(236, 245, 241, 0.96));
    }}
    .card-topline h2 {{
      margin: 0;
      font-size: 1.35rem;
    }}
    .card-topline .eyebrow {{
      color: var(--accent-2);
    }}
    .notes {{
      margin: 12px 0 14px;
    }}
    .muted {{
      color: var(--muted);
    }}
    .chip-row {{
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      margin-bottom: 14px;
    }}
    .chip {{
      display: inline-flex;
      gap: 8px;
      align-items: center;
      padding: 7px 10px;
      border-radius: 999px;
      background: rgba(16, 24, 32, 0.06);
      border: 1px solid rgba(16, 24, 32, 0.08);
      font-size: 0.9rem;
    }}
    .chip-label {{
      color: var(--muted);
    }}
    .chip-value {{
      font-weight: 700;
    }}
    audio {{
      width: 100%;
      margin: 6px 0 10px;
      accent-color: var(--accent);
    }}
    .artifact-links {{
      display: flex;
      flex-wrap: wrap;
      gap: 14px;
      margin: 8px 0;
      color: var(--accent-2);
    }}
    .pathline {{
      margin: 8px 0 0;
      color: var(--muted);
      font-size: 0.92rem;
      word-break: break-all;
    }}
    .ledger-preview {{
      margin-top: 14px;
      border-top: 1px solid var(--line);
      padding-top: 14px;
    }}
    .ledger-preview summary {{
      cursor: pointer;
      font-weight: 700;
    }}
    .ledger-actions {{
      display: flex;
      flex-wrap: wrap;
      gap: 14px;
      margin: 10px 0 8px;
      color: var(--muted);
      font-size: 0.92rem;
    }}
    pre {{
      margin: 12px 0 0;
      padding: 14px;
      border-radius: 16px;
      overflow: auto;
      white-space: pre-wrap;
      background: rgba(16, 24, 32, 0.9);
      color: #eaf4f2;
      font: 0.84rem/1.5 "SFMono-Regular", "Menlo", monospace;
      max-height: 280px;
    }}
    .empty-state {{
      color: var(--muted);
      padding: 18px 2px 0;
    }}
    .hidden {{
      display: none !important;
    }}
    @media (max-width: 800px) {{
      .shell {{
        padding: 16px 14px 42px;
      }}
      .hero {{
        padding: 22px 18px;
      }}
      .tool-grid,
      .tone-form,
      .controls {{
        grid-template-columns: 1fr;
      }}
    }}
  </style>
</head>
<body>
  <main class="shell">
    <section class="hero">
      <p class="eyebrow">SeedBox Native Golden Harness</p>
      <h1>Golden Fixture Browser</h1>
      <p class="hero-copy">
        Deterministic renders, companion ledgers, and manifest receipts in one place.
        This page is generated from <code>tests/native_golden/golden.json</code> so the browser
        stays anchored to checked-in artifacts instead of hand-written marketing copy.
      </p>
      <div class="hero-meta">
        <div class="stat"><strong>{len(fixtures)}</strong><span>Total artifacts</span></div>
        <div class="stat"><strong>{len(audio)}</strong><span>Audio fixtures</span></div>
        <div class="stat"><strong>{len(logs)}</strong><span>Logs and ledgers</span></div>
        <div class="stat"><strong>{total_audio_seconds:.1f}s</strong><span>Total audio duration</span></div>
      </div>
      <div class="hero-links">
        <a href="{manifest_href}">Open manifest</a>
        <a href="{readme_href}">Golden harness README</a>
        <a href="{gallery_href}">Seed gallery docs</a>
      </div>
      <div class="hero-meta">
        <div class="stat"><strong>{html.escape(generated_at)}</strong><span>Manifest generated</span></div>
        <div class="stat"><strong>{html.escape(last_updated_by)}</strong><span>Last updated by</span></div>
      </div>
    </section>

    <section class="controls" aria-label="Fixture browser controls">
      <input id="fixture-search" class="search" type="search" placeholder="Filter by fixture name, note, or path">
      <p class="muted">Companion <code>*-control</code> ledgers are embedded in the matching audio cards.</p>
    </section>

    <section class="tool-card" aria-labelledby="tone-tool-heading">
      <div class="tool-grid">
        <div>
          <p class="eyebrow">Local-only Tooling</p>
          <h2 id="tone-tool-heading">Cut A New Input Tone</h2>
          <p>
            This surface can mint a new <code>input-tone-*</code> fixture through the offline golden
            pipeline when the browser is served by <code>scripts/serve_golden_fixture_browser.py</code>.
            The server compiles the native helper, writes the WAV + control log, refreshes the manifest,
            and regenerates this page before the browser reloads.
          </p>
          <form id="tone-form" class="tone-form">
            <label class="full">
              Fixture name
              <input id="tone-name" name="name" type="text" placeholder="a4-reference">
            </label>
            <label>
              Frequency (Hz)
              <input id="tone-frequency" name="frequency_hz" type="number" min="1" max="24000" step="0.01" value="440">
            </label>
            <label>
              Amplitude
              <input id="tone-amplitude" name="amplitude" type="number" min="0.01" max="1" step="0.01" value="0.35">
            </label>
            <label class="full">
              Duration (seconds)
              <input id="tone-duration" name="duration_seconds" type="number" min="0.01" max="60" step="0.01" value="2.0">
            </label>
            <div class="tone-actions full">
              <button id="tone-submit" class="tone-button" type="submit">Render Tone Fixture</button>
              <span id="tone-status" class="tone-status muted">Idle.</span>
            </div>
          </form>
          <details class="ledger-preview run-log">
            <summary>Latest pipeline transcript</summary>
            <pre id="tone-log">No render yet.</pre>
          </details>
        </div>
        <aside class="tool-callout">
          <p><strong>Launch command</strong></p>
          <code>python3 scripts/serve_golden_fixture_browser.py</code>
          <p class="muted">
            If you opened this page via a plain static server, the tone form will refuse with a same-origin API error.
            That is intentional: only the dedicated local dev server is allowed to run the golden pipeline.
          </p>
        </aside>
      </div>
    </section>

    <section>
      <div class="section-heading">
        <h2>Audio Pressings</h2>
        <p>Playable WAV fixtures with hashes, notes, and inline control previews.</p>
      </div>
      <div class="grid" id="audio-grid">
        {audio_cards}
      </div>
    </section>

    <section>
      <div class="section-heading">
        <h2>Standalone Logs</h2>
        <p>Manifest-side receipts that do not already ride along with an audio card.</p>
      </div>
      <div class="grid" id="log-grid">
        {log_cards}
      </div>
    </section>
  </main>

  <script>
    const searchInput = document.getElementById("fixture-search");
    const cards = Array.from(document.querySelectorAll(".fixture-card"));
    const toneForm = document.getElementById("tone-form");
    const toneSubmit = document.getElementById("tone-submit");
    const toneStatus = document.getElementById("tone-status");
    const toneLog = document.getElementById("tone-log");
    searchInput.addEventListener("input", () => {{
      const term = searchInput.value.trim().toLowerCase();
      for (const card of cards) {{
        const haystack = card.dataset.terms || "";
        card.classList.toggle("hidden", term !== "" && !haystack.includes(term));
      }}
    }});

    function setToneStatus(message, kind) {{
      toneStatus.textContent = message;
      toneStatus.className = `tone-status ${{kind || "muted"}}`;
    }}

    toneForm.addEventListener("submit", async (event) => {{
      event.preventDefault();
      toneSubmit.disabled = true;
      setToneStatus("Rendering through the offline golden pipeline...", "");
      toneLog.textContent = "Running...";

      const payload = {{
        name: document.getElementById("tone-name").value.trim() || null,
        frequency_hz: Number(document.getElementById("tone-frequency").value),
        amplitude: Number(document.getElementById("tone-amplitude").value),
        duration_seconds: Number(document.getElementById("tone-duration").value),
      }};

      try {{
        const response = await fetch("/api/input-tone", {{
          method: "POST",
          headers: {{
            "Content-Type": "application/json",
          }},
          body: JSON.stringify(payload),
        }});
        const result = await response.json();
        toneLog.textContent = [result.stdout || "", result.stderr || ""].filter(Boolean).join("\\n");

        if (!response.ok || !result.ok) {{
          const error = result.error || "Tone render failed.";
          const staticServerHint = response.status === 404
            ? " Launch the page with `python3 scripts/serve_golden_fixture_browser.py`."
            : "";
          throw new Error(error + staticServerHint);
        }}

        setToneStatus("Tone fixture rendered. Reloading the browser surface...", "ok");
        window.setTimeout(() => window.location.reload(), 900);
      }} catch (error) {{
        const message = error instanceof Error ? error.message : "Tone render failed.";
        setToneStatus(message, "error");
        if (toneLog.textContent === "Running...") {{
          toneLog.textContent = message;
        }}
      }} finally {{
        toneSubmit.disabled = false;
      }}
    }});
  </script>
</body>
</html>
"""


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=Path("tests/native_golden/golden.json"),
                        help="Path to the golden manifest JSON")
    parser.add_argument("--output", type=Path, default=Path("build/fixtures/index.html"),
                        help="Path to the generated HTML browser")
    args = parser.parse_args(argv)

    repo_root = Path(__file__).resolve().parent.parent
    manifest_path = (repo_root / args.manifest).resolve() if not args.manifest.is_absolute() else args.manifest.resolve()
    output_path = (repo_root / args.output).resolve() if not args.output.is_absolute() else args.output.resolve()

    try:
        manifest = _load_manifest(manifest_path)
        body = render_browser(manifest, repo_root, output_path)
    except Exception as exc:
      print(f"error: {exc}", file=sys.stderr)
      return 1

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(body, encoding="utf-8")
    print(f"fixture browser refreshed -> {output_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
