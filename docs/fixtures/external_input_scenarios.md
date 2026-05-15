# External Input Probe Scenarios

`external_input_scenarios.json` is the checked-in manifest for native external-input probe scenario metadata. Keep scenario names, categories, notes, `golden_permutations` membership, and musical-effect contracts there first.

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

## Musical Contract Fields

Each scenario owns a conservative `musical_contract` object. These thresholds
are proof gates, not synthesis targets; keep them low enough for CI and local
smoke files while still rejecting silent, dry, clipped, or flat renders.

- `role`: human-readable scenario role used in failure reports and docs.
- `non_silence.min_output_rms`: minimum max-channel output RMS.
- `non_silence.min_output_peak`: minimum output peak.
- `no_clipping.max_output_peak`: maximum allowed output peak.
- `no_clipping.max_clipped_samples`: maximum allowed sample count at clipping-risk level.
- `input_output_difference.min_mean_abs_diff`: minimum stereo mean absolute difference between resampled input and output.
- `stereo_difference.required`: whether this scenario should prove stereo motion.
- `stereo_difference.min_mean_abs_diff`: minimum mean absolute left/right difference when stereo proof is required.
- `temporal_variation.required`: whether this scenario should prove block-to-block energy movement.
- `temporal_variation.min_block_rms_range`: minimum range across output block RMS values.
- `reseed.min_count`: minimum expected reseed count; this is `1` for `reseed-live` and `0` elsewhere.

Validate a probe summary against the manifest contract with:

```bash
python scripts/check_musical_effect_contract.py --summary <summary.json>
```

## Documented Scenario Set

- `mixed-boot`
- `granular-live`
- `resonator-live`
- `burst-overlay`
- `euclid-overlay`
- `reseed-live`
