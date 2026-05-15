# Operator UI roadmap

The project already exposes powerful controls, but many flows still require
CLI flags or config edits. This roadmap defines a performer-friendly operator
surface that stays aligned with core engine contracts.

## Core goals

- Load and switch recipes/presets without shell commands.
- Surface consent/safety state clearly before audio output changes.
- Visualize mapping/curve behavior in real time.

## Phase 1: read-only status panel

Scope:
- Current preset/recipe ID
- Clock source + BPM
- Quiet mode / panic / consent status
- Active engine per seed
- Host-boundary diagnostics (MIDI drops, oversize block drops, scratch capacity)

Deliverables:
- Minimal host-side status endpoint (JSON snapshot) sourced from `AppState`
- Lightweight UI view that refreshes at control-rate cadence

Current scaffold:
- `AppState::captureStatusSnapshot(StatusSnapshot&)` returns a typed read-only state payload.
- `AppState::captureStatusJson()` serializes the same payload as JSON for host tooling.
- The status payload now includes a `hostDiagnostics` block so a host-side UI can
  surface callback trouble without scraping debug overlays.

## Phase 2: safe control actions

Scope:
- Load recipe/preset
- Trigger reseed
- Toggle lock states
- Panic + transport controls

Guardrails:
- Explicit confirmation for high-impact actions in live mode
- Action audit log for teaching/performance playback

## Phase 3: mapping curve inspector

Scope:
- Curve preview graph for probability/density/modulation mappings
- Live marker showing current input/output point
- Export/import of mapping presets

## Acceptance criteria

- Non-technical performers can load a preset and confirm consent state from UI.
- No required direct YAML/CLI edits for common rehearsal workflows.
- UI actions map one-to-one to existing `AppState` control contracts.
