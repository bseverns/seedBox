# Platform improvement program (2026)

This roadmap turns five recurring pain points into tracked deliverables.

## 1) Setup complexity -> starter bundle

Goal: a newcomer can verify a working local setup in one command.

Shipped now:
- `scripts/starter_bundle.sh` runs `pio pkg install` and `pio test -e native`.
- `README.md` points to the starter bundle as the default laptop path.

Next:
- Add a CI smoke job that runs the starter script end-to-end.
- Add a `--native-golden` option once fixture generation is stable across hosts.

## 2) Dependency management -> pinned container baseline

Goal: remove host-version roulette for native builds and tests.

Shipped now:
- `containers/native-dev/Dockerfile` pins Ubuntu + PlatformIO.
- `containers/native-dev/compose.yaml` runs the repo in a consistent container.
- `containers/native-dev/README.md` documents build/test commands.

Next:
- Publish a tagged image in CI after green default-branch builds.
- Add a weekly dependency refresh check that opens a PR with version bumps.

## 3) Documentation redundancy -> single-source map

Goal: one authoritative doc per topic, link everywhere else.

Shipped now:
- `docs/DOC_SINGLE_SOURCE.md` maps canonical owners for setup, CI, JUCE, and scripts.
- `docs/README.md` now points to canonical build docs instead of duplicating details.

Next:
- Audit existing docs for copy/paste sections and replace with links.
- Add a docs-lint check that flags duplicated headings across docs.

## 4) Swarm scalability -> burst/swarm test roadmap

Goal: make swarm-style trigger clusters measurable before adding more features.

Shipped now:
- `docs/roadmaps/swarm_scalability.md` defines latency, collision, and multi-user milestones.

Next:
- Add native benchmarks for burst trigger queue pressure.
- Add a deterministic stress suite that replays multi-source trigger storms.

## 5) User-friendly UI -> operator surface roadmap

Goal: reduce reliance on CLI flags/YAML for common performer workflows.

Shipped now:
- `docs/roadmaps/operator_ui.md` defines a phased operator UI plan (load recipes, consent state, mapping curves).
- `AppState::captureStatusSnapshot` + `AppState::captureStatusJson` expose a read-only host-facing status payload.

Next:
- Build a small host-side UI prototype to consume that endpoint.
- Optionally add lightweight HTTP/WebSocket transport around the same payload.

## Success metrics

- New contributor reaches a green native test run in <= 15 minutes.
- Local and container-native test results match on the same commit.
- No duplicated build setup sections across docs without canonical links.
- Swarm benchmarks produce repeatable latency/collision numbers in CI artifacts.
- Operator UI prototype supports recipe load + consent/status visibility without editing files manually.
