# Releasing SeedBox like you mean it

This project is part synth lab, part sketchbook. Shipping a release should feel
more like tuning an instrument than chasing a corporate checklist, but we still
want repeatable moves. Here's the ritual.

## 1. Pick the version number

1. Read the `CHANGELOG.md` and decide whether you're cutting a patch, minor, or
   major release. Stay honest about breaking changes.
2. Update the version constant wherever the firmware and scripts expose it
   (usually `include/seedbox_version.h` and `scripts/version.py`).
3. Commit the bumps in their own commit or alongside the changelog update.

## 2. Curate the changelog

1. Move items from the `Unreleased` section of `CHANGELOG.md` into a new
   `## [x.y.z] - YYYY-MM-DD` section.
2. Write notes like you're teaching a friend how the release feels, not just
   what files moved.
3. Mention any remaining TODOs, especially hardware fixtures or datasets still
   brewing.

## 3. Tag the release

1. Create an annotated tag: `git tag -a vx.y.z -m "SeedBox vx.y.z"`.
2. Push tags: `git push --follow-tags origin main` (or the branch you're
   blessing).
3. Draft the GitHub release notes by pasting the changelog entry and sprinkling
   in demo audio links if you have them.

## 4. Let CI have its say

1. Kick off the PlatformIO matrix: `pio test -e native` locally before you push.
2. After the tag lands, make sure GitHub Actions (or your CI flavor) turns green
   for both the native and hardware builds.
3. If something flakes, fix it before you call the release done. No haunted
   builds allowed.

## 5. Celebrate and document

1. Snap a photo or grab audio of the release jam for the docs.
2. Open a fresh `Unreleased` section with at least one ambition for the next
   version so future-you has a runway.
3. Share the release in the community channel; teach someone what you learned.

Keep this checklist human. If a step feels stiff, rewrite it — but never skip
it.
