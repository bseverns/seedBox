# External Input Probe Scenarios

`external_input_scenarios.json` is the checked-in manifest for native external-input probe scenario metadata. Keep scenario names, categories, notes, and `golden_permutations` membership there first.

The C++ probe in `tools/native_input_probe.cpp` still owns the scenario setup functions and render behavior. When changing the scenario table, run:

```bash
python scripts/validate_input_scenarios.py
```

## Golden Permutations

Run the local external-input permutation suite with:

```bash
python scripts/run_local_input_golden.py --input <file.wav> --suite golden-permutations
```

The command writes a local receipt folder beside the input WAV by default. Do not commit generated WAVs, local receipt JSON, generated browsers, or machine-specific absolute paths.

## Documented Scenario Set

- `mixed-boot`
- `granular-live`
- `resonator-live`
- `burst-overlay`
- `euclid-overlay`
- `reseed-live`
