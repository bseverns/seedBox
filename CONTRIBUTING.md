# Contributing to SeedBox

SeedBox is half studio notebook, half teaching lab. Contributions should help
both sides: make the instrument sturdier *and* leave clues for the next curious
builder. This guide covers our expectations so you can jump in confidently.

## Codes of conduct and licenses

- The [Contributor Covenant](CODE_OF_CONDUCT.md) applies everywhere: issues,
  discussions, livestreams, late-night commits.
- Firmware and code land under the [MIT License](LICENSE).
- Documentation, diagrams, and markdown live under
  [CC-BY-4.0](LICENSE-docs). Attribute remixers loudly.

## Before you branch

1. Sync your fork: `git pull --rebase origin main`.
2. Create a topic branch: `git switch -c feat/<short-description>` or
   `fix/<bug-ticket>`.
3. Avoid editing `.pio/` or generated headers (`include/BuildInfo.h`) by hand.

We squash-merge most pull requests. Write commits like zine entries: short
phrases in the imperative mood. Example: `Add quiet-mode helper for tests`.

## Pull request hygiene

- Keep PRs focused. If you discover a new idea mid-stream, open an issue and
  keep the branch scoped.
- Describe the musical or UX intent in your PR body. Link to examples or tests
  that showcase the behavior.
- Make sure docs stay in sync. If you add a feature, note it in the relevant
  README and add a TODO for any missing audio fixtures.

## Running the checks

These commands must succeed locally before opening a PR. Run them from the repo
root unless noted otherwise.

| Goal | Command |
| ---- | ------- |
| Native build + tests | `pio run -e native && pio test -e native` |
| Teensy firmware build | `pio run -e teensy40` |
| Inspect dependency pins | `pio pkg list` and `pio run -t envdump` |

Golden audio tests are stubbed for now. They only run when
`-D ENABLE_GOLDEN=1` is present. Leave that flag off unless you're regenerating
fixtures.

## Audio-thread etiquette

- **No allocation inside audio callbacks.** If you need buffers, allocate them
  statically or in setup code.
- Keep per-sample math deterministic. Use fixed seeds when prototyping random
  modulation, especially while `QUIET_MODE` is enabled.
- Respect `SEEDBOX_HW` and `QUIET_MODE`. Hardware-only code must compile out of
  the native build, and quiet mode should avoid persistence, external IO, and
  nondeterministic randomness.

## Submitting your PR

1. Re-run the commands above.
2. Ensure your branch has no merge conflicts: `git status` should be clean.
3. Fill in the PR template with:
   - A one-paragraph intent summary.
   - A checklist of tests you ran.
   - Notes about any TODO audio fixtures or screenshots.
4. Mention which docs you touched so reviewers can cross-check tone and diagrams.

We don't require sign-offs, but we *do* expect you to stick around for review
feedback. Bring questions to the discussion — this project thrives on shared
learning.
