# Releasing SeedBox

We tag releases manually so every drop carries intent and documentation.

## Prerequisites

- Clean `main` branch with CI green.
- `platformio.ini` reflects pinned versions regenerated via
  [`docs/toolchain.md`](docs/toolchain.md).
- `CHANGELOG.md` updated with the release notes.
- No outstanding TODOs for required assets (audio fixtures can remain TODO if
  explicitly listed).

## Release steps

1. Bump version references in `CHANGELOG.md` and note the release date.
2. Run the full local checks:
   ```bash
   pio run -e native
   pio test -e native
   pio run -e teensy40
   ```
3. Tag the release:
   ```bash
   git tag -a vX.Y.Z -m "SeedBox vX.Y.Z"
   git push origin vX.Y.Z
   ```
4. Draft a GitHub release that includes:
   - Highlights lifted from `CHANGELOG.md`.
   - TODO reminder for future golden audio uploads if they are still pending.
5. Announce in the project notes or community channel with a short summary and
   link to new docs.

## Post-release

- Update `[Unreleased]` in `CHANGELOG.md` with new TODOs.
- Kick off a round of golden-audio validation when fixtures exist.

Keep it intentional, keep it musical.
