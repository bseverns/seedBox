# SB-05 — Repository truth-drift cleanup

This step reconciles documentation, comments, and examples with the existing
implementation. It changes no engine, input-routing, or host/audio architecture.

## Corrections

- `Seed.h` and the seed field guide now list all six EngineRouter IDs:
  Sampler, Granular, Resonator, Euclid, Burst, and Toy (0–5).
- The [artifact policy](../artifact_policy.md) distinguishes intentionally
  tracked golden WAVs/ledgers from ignored exploratory renders and compiled
  outputs. Contributor, builder, and test instructions link to the same policy.
- 41 machine-specific architecture links now use repository-relative targets.
  Their stale line-number anchors were removed. Two additional broken links
  were corrected: the TRS guide's config-header path and the wiring gallery's
  OLED implementation link.
- The JUCE README now describes the actual panel/Detailed Editor layout,
  consolidates shortcuts and persistence guidance, and documents the optional
  dry fallback instead of claiming unconditional monitoring.
- Builder guidance identifies the PlatformIO version hook correctly, separates
  CMake/Ninja from PlatformIO/SCons, states the actual quiet-mode defaults, and
  names existing snapshot APIs instead of nonexistent debugging helpers.
- Unsupported `--test-name` examples now select their containing suites. The
  TRS guide uses `teensy40` and documents build flags in `platformio.ini` rather
  than an unsupported command-line override and nonexistent environment.
- SB-01 and SB-03 link to their completed follow-ups. Their original test counts
  remain historical validation records. The unused `HardwareConfig.h` control
  declarations are identified as legacy references, with the current contract
  in `PanelControls.h`.

## Local-link guard

```sh
python3 scripts/check_doc_links.py
python3 -m unittest discover -s scripts/tests -p test_doc_links.py
```

The checker reads repository Markdown files and validates inline links/images,
reference definitions, and HTML link/image attributes against repository file
and directory names. It rejects machine-specific destinations, paths outside
the repository, missing files, and filename-case drift. Ignored build outputs
cannot satisfy a link. Code examples and provenance paths written as code are
left alone; historical receipts are not rewritten to invent portable inputs.

This is a file-target check, not a complete Markdown parser. It does not fetch
external URLs or validate heading/line fragments. Six tests cover supported
link forms, code fences, escaped/encoded filenames, nonportable paths, and
missing or ignored targets. The `doc-links` CI job runs the guard and tests
before the main build matrix.

## Validation

Final local results are recorded here before handoff. Hardware upload commands
are documented for builders; this cleanup does not flash a device or regenerate
reference audio. GitHub CI results must be checked on the eventual commit.

## Next step

SB-06 can define the canonical Seed Card proof format using the now-consistent
fixture policy. This cleanup does not decide the broader JUCE role reserved for
SB-15 or claim new cross-body or real-time guarantees.
