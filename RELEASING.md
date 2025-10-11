# Releasing SeedBox

This project treats releases as mini zines: short, intentional, and well
annotated. Follow this playbook when cutting a new tag.

## 1. Prep the branch

- Start from `main` and ensure your workspace is clean: `git status`.
- Update `CHANGELOG.md` with the release notes. Highlight new examples, docs,
  and any TODO fixtures.
- Bump version numbers in code or metadata if required (none yet).
- Regenerate `include/BuildInfo.h` by running a build to ensure the version
  banner is fresh.

## 2. Verify the build matrix

Run the same checks as CI locally:

```bash
pio run -e native
pio test -e native
pio run -e teensy40
```

If network or hardware access is limited, document the limitation in the release
notes and link to a CI run that did succeed.

## 3. Tag and push

```bash
git tag -s vX.Y.Z -m "SeedBox vX.Y.Z"
git push origin vX.Y.Z
```

Signed tags are preferred. If you lack a signing key, note it in the release PR
so maintainers can co-sign.

## 4. Draft the release page

- Paste the changelog entry into the GitHub release body.
- Link to any example READMEs and TODO audio fixture slots so listeners know
  where future clips will land.
- Add a "Known Issues" section if golden audio fixtures or screenshots are still
  pending.

## 5. Post-release chores

- Update `CHANGELOG.md` by creating a new `[Unreleased]` section if needed.
- File follow-up issues for TODO items (audio renders, MN42 handshake demos,
  etc.).
- Celebrate in the chat or lab channel. The synth deserves a victory lap.
