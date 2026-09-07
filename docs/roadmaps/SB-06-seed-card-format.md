# SB-06 — Canonical Seed Card proof format

SB-06 defines a small, reusable proof object for one named SeedBox render. It
does not publish a card or imply that a fixture is cross-body equivalent.

## Delivered contract

- [`docs/seed_cards/index.json`](../seed_cards/index.json) is the registry for completed cards.
- [`seed-card.schema.json`](../seed_cards/seed-card.schema.json) records the versioned JSON contract; [`template.json`](../seed_cards/template.json) shows a complete starting shape without pretending it is evidence.
- Every registered card requires the named seed, full `Seed` genome and lineage, relevant controls, render body and revision, audio render, control ledger, SHA-256 hashes, and concise `changed` / `stayed_fixed` statements.
- [`validate_seed_cards.py`](../../scripts/validate_seed_cards.py) validates the registry and recomputes evidence hashes with Python's standard library. Its tests cover a complete proof, hash/proof drift, and unregistered cards.
- The `seed-card-contract` CI job runs the checker and its unit tests.

## Card workflow

1. Generate and review a deterministic audio render and its control ledger.
2. Copy the template to `docs/seed_cards/cards/<slug>.json` and replace every illustrative value with the captured seed state and evidence.
3. Put the audio and ledger in reviewed repository fixture storage as described by the [artifact policy](../artifact_policy.md), calculate SHA-256 values, and list the card in `index.json`.
4. Run the checker and listen/read the evidence before review.

SB-07 will use this contract to publish one real native Seed Card.
