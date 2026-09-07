# SB-09 — Seed phenotype layer

`SeedPhenotype` is a small intent layer above the executable `Seed` genome.
Its seven normalized traits—energy, stability, brightness, space, roughness,
recurrence, and tension—derive correlated scheduler, envelope, timbre, spatial,
granular, and resonator values through `applyPhenotype`.

The phenotype is not stored on `Seed` and does not replace it. The derived
genome remains the runtime, persistence, and evidence object. Applying a
phenotype preserves seed ID, PRNG state, source, lineage, engine, sample choice,
and granular source/slot. This gives future mutation and ancestry work a clear
semantic input without changing today's routing contract.

The native tests prove clamping, preserved identity, deterministic mapping, and
the intended energy/space/stability correlations.
