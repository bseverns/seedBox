# Seed Cards

A Seed Card is the compact, inspectable proof for one named SeedBox render. It
joins the musical identity, controls, execution body, audio, and control ledger
that otherwise live in separate places. It is evidence, not promotional copy.

SB-06 supplies the format and checker. The registry is intentionally empty
until SB-07 adds the first end-to-end card.

## Format

The registry is [`index.json`](index.json). Each registered card lives under
`docs/seed_cards/cards/` and conforms to
[`seed-card.schema.json`](seed-card.schema.json). Start from
[`template.json`](template.json), copy it into `cards/<slug>.json`, then replace
every illustrative value with measured evidence.

Every card must state:

- a readable name plus the master seed, per-seed PRNG state, source, lineage,
  and the complete `Seed` genome from [`Seed.h`](../../include/Seed.h);
- the controls that matter to the capture and why they matter;
- the body (`native`, `juce`, or `hardware`), exact target, and revision used;
- repository-relative paths to a rendered WAV and a control ledger;
- lowercase SHA-256 hashes for both evidence files; and
- one concise statement each for what changed and what stayed fixed.

`source` uses `lfsr`, `tap_tempo`, `preset`, or `live_input`, matching
`Seed::Source`. `engine` uses the current EngineRouter IDs 0–5. The full genome
is deliberate: a preset serialization alone is insufficient because the card
also needs the `source` and `lineage` provenance fields.

Evidence files should be reviewed fixture material, normally under
[`build/fixtures/`](../../build/fixtures/), following the
[artifact policy](../artifact_policy.md). Do not point a card at local scratch
files, generated build products, or paths outside the repository.

## Validation

```sh
python3 scripts/validate_seed_cards.py
python3 -m unittest discover -s scripts/tests -p test_seed_cards.py
```

The checker validates the registry, required proof fields, card IDs, safe
repository-relative evidence paths, and the SHA-256 contents of each artifact.
It rejects cards omitted from the registry and refuses an empty or malformed
proof statement. The checker has no third-party Python dependency.

When a card is ready, add it to `index.json`, run the checker, then review its
audio and ledger alongside the matching golden-fixture metadata. See the
[Seed Gallery](../SeedGallery.md) for the current fixture collection.
