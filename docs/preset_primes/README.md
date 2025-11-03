# Preset prime banks — canonical seed captures

This folder is the ledger for every curated preset prime bank that ships with
SeedBox. Each file is a straight dump of `seedbox::Preset::serialize()` so you
can diff the DNA the same way the firmware sees it. Once a bank lands here you
can:

1. `AppState::setSeedPreset(presetId, preset.seeds)` during boot to populate the
   in-memory bank.
2. Push the same JSON blob through any `seedbox::io::Store` backend to stash it
   in EEPROM or on SD.

Because the files are just JSON, you can crack them open in class, talk through
what each seed is doing, and still feed the exact data straight into hardware or
sim builds without lossy conversions.

## Canonical banks

| File | Preset slot | Notes |
| --- | --- | --- |
| [`workshop_bank.json`](workshop_bank.json) | `workshop-a` | Four-seed groove that mixes sampler hits with granular live-input and a resonator drone. |

## How to mint a new bank

1. Boot the native sim (`pio test -e native` or `pio run -e native`) and sculpt
   the scene you care about.
2. Call `Preset snapshot = app.snapshotPreset("your-slot");` inside a quick
   harness (or adapt `test/test_app/test_presets.cpp`).
3. Dump `snapshot.serialize()` to disk and drop the JSON in this folder.
4. Commit it alongside any lesson notes so future you knows why the bank exists.

Remember to run `pio test -e native -f test_app/test_presets.cpp` before you ship
changes; that suite proves the serialization path is still deterministic for the
whole preset pipeline.
