# Generated files and reference fixtures

The repository keeps reviewed golden fixtures as reproducible reference
material. Local build products and exploratory renders stay out of commits.
The ignore rules live in [`.gitignore`](../.gitignore).

| Kind | Location | Commit policy |
| --- | --- | --- |
| Curated golden WAVs, control ledgers, and fixture metadata | `build/fixtures/` | Tracked intentionally when added or refreshed as reviewed reference material. |
| Golden hash manifest and generated fixture index | `tests/native_golden/golden.json`, `tests/native_golden/fixtures_autogen.hpp`, `build/fixtures/index.html` | Update with the corresponding fixture changes. |
| Exploratory audio and temporary test logs | `out/`, `artifacts/`, or a temporary directory | Ignored; do not commit. |
| Compiled firmware, plugins, executables, and build caches | `.pio/`, other `build/` subdirectories, or a temporary build directory | Do not commit. CI can publish build artifacts separately. |
| Generated version header | `include/BuildInfo.generated.h` | Ignored; the PlatformIO version hook recreates it during builds. |

`build/fixtures/` is an explicit exception to the build-output ignore rule.
That exception permits reference WAVs; it is not an invitation to store arbitrary
binaries or every exploratory render there. Source fixture generators and their
instructions remain part of the repository.

## Refreshing a reference

1. Reproduce the intended scenario using the
   [golden harness instructions](../tests/native_golden/README.md).
2. Review the changed audio and control ledger and explain the intentional
   behavior change in the PR.
3. Run `python3 scripts/compute_golden_hashes.py --write` to refresh the hash
   manifest, generated fixture header, and browser together.
4. Review the diff before committing the fixture and its companion metadata.

For experiments, use an output override such as `SEEDBOX_FIXTURE_ROOT` with the
native golden harness, or the local-output workflow in
[`run_local_input_golden.py`](../scripts/run_local_input_golden.py).
Do not refresh reference hashes merely to make an unexplained mismatch pass.

Historical receipts can name local input paths and generated output paths as
provenance. Those paths are not portable repository links or evidence that the
files are included in a fresh checkout.
