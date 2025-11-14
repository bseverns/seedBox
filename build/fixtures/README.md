# Golden fixture pressings

These WAVs and logs are the receipts from the native golden harness. We keep
them checked in so CI can diff what the synth engines actually spit out without
having to re-render on every review.

- Regenerate the stack with `./scripts/offline_native_golden.sh` when you tweak
  an engine.
- Or run `pio test -e native_golden` with `ENABLE_GOLDEN=1` and rerun
  `python scripts/compute_golden_hashes.py --write`.
- Never hand-edit the fixtures; the hashes in
  [`tests/native_golden/golden.json`](../../tests/native_golden/golden.json) must
  stay in lock-step with these files.

Yeah it's a little noisy to stash binaries in git, but failing CI because the
`build/fixtures` directory evaporated is way noisier.
