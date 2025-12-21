# Artifact signatures: punk-rock trust, but verified

Every artifact bundle that rolls out of CI (native goldens, JUCE builds on each OS) ships with a detached GPG signature from key `4473F115745A1A61`. Use it to prove the download wasn’t mangled — or malicious.

## How the workflows sign
- CI pulls a private signing subkey from repo secrets, builds the archives, then emits both the artifact and a matching `.sig` file.
- Forks and local runs skip signatures automatically if the secrets are missing; the build still completes, but you won’t get a `.sig`.

## How to verify a drop
1. Import the published public key (we mirror it on releases and keyservers).
2. Verify the artifact against its signature:
   ```bash
   gpg --verify seedbox-linux-host.tar.gz.sig seedbox-linux-host.tar.gz
   ```
   Swap in whatever archive you grabbed (`native_golden.tar.gz`, `seedbox-macos-universal.tar.gz`, `seedbox-windows-host.zip`, etc.).
3. Trust only downloads that show “Good signature” for `4473F115745A1A61`; if verification fails, toss the file and ping us.

## Where to read more
- Golden fixtures live with their own receipts in [`tests/native_golden/`](../../tests/native_golden) and [`docs/roadmaps/native_golden.md`](../roadmaps/native_golden.md).
- Desktop build knobs and dependencies are documented in [`docs/ci_desktop_builds.md`](../ci_desktop_builds.md).
