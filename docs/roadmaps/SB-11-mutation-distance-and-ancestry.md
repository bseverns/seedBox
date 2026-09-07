# SB-11 — Mutation distance and ancestry

Each `Seed` now records its immediate parent ID, PRNG state, lineage, normalized
mutation distance, and a bitmask of moved pitch, density, probability, jitter,
tone, and spread dimensions. `SeedAncestry` records this whenever host edits or
safe seed-page nudges change an unlocked genome, and preset JSON preserves it.

The record answers immediate parent and mutation questions without replaying a
control log. It is deliberately an immediate-parent record; branching history
and performance navigation are SB-12 work.
