# SeedBox documentation staging ground

Welcome to the paper trail. If the firmware repo is the synth, this folder is
the zine you read on the tour van between gigs. Every doc here mirrors the
teach-forward tone from **MOARkNOBS-42**: no mystery meat, just receipts,
rationale, and battle plans.

## How this folder is organized

- `builder_bootstrap.md`
  - The onboarding field manual. Covers environment setup, wiring diagrams, and
    lab ideas for students. Read this first if you're powering hardware.
- `roadmaps/`
  - Living design documents for in-flight engines. Each roadmap walks through
    intent, constraints, and open TODOs so future contributors can land new
    DSP without guessing.
  - Start with [`roadmaps/granular.md`](roadmaps/granular.md) and
    [`roadmaps/resonator.md`](roadmaps/resonator.md). They spell out voice
    budgets, seed genome mapping, and perf guardrails.
- Coming soon: wiring diagrams, calibration checklists, and whatever else the
  hardware build learns the hard way.

## When to touch these docs

Update a roadmap the second you change a contract in code. Treat the docs like
unit tests for intent—if you tweak a struct field, log the new rationale here so
the next engineer (maybe future-you at 3 AM) can follow the bread crumbs.

## Tone + expectations

- Be loud about assumptions. If you pick a CPU budget, explain why.
- Teach as you document. Pretend the reader knows C++ but hasn't earned their
  seed-forging badge yet.
- Keep the punk edge: celebrate hacks, call out ugly corners, but always point
to the cleanup plan.

When in doubt, over-explain. Clarity wins jams.
