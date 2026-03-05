# Documentation single-source map

Use this page to keep overlapping docs from drifting. If two docs want to
explain the same contract, pick one canonical owner and link to it.

## Canonical owners

| Topic | Canonical doc | Link-only docs |
| --- | --- | --- |
| Starter onboarding commands | `docs/builder_bootstrap.md` | `README.md`, `docs/onboarding/newcomer_map.md` |
| Desktop CI + host dependency details | `docs/ci_desktop_builds.md` | `docs/README.md`, `README.md` |
| JUCE local build flags and host setup | `docs/juce_build.md` | `docs/ci_desktop_builds.md` |
| Script catalog + helper usage | `scripts/README.md` | `docs/builder_bootstrap.md` |
| 2026 platform improvements roadmap | `docs/roadmaps/platform_improvement_program.md` | any future status updates |

## Editing rule of thumb

1. Update the canonical doc first.
2. In secondary docs, keep only a short summary plus a link.
3. If you add a new topic, register it in this table the same day.
