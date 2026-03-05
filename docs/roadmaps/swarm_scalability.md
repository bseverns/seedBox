# Swarm scalability roadmap

Swarm-style triggering is currently useful for demos but not yet production
grade for dense, multi-source experiments. This roadmap defines the hardening
path.

## Targets

- Keep trigger scheduling jitter under 2 ms at reference load.
- Keep trigger collision drop rate under 0.1%.
- Support at least 3 concurrent trigger sources without starvation.

## Workstream A: latency + throughput instrumentation

1. Add a burst/swarm stress harness in native tests.
2. Record per-trigger enqueue-to-fire latency histograms.
3. Export metrics as CI artifacts so regressions are visible.

Definition of done:
- Every CI run publishes baseline latency histograms for the stress suite.

## Workstream B: collision handling policy

1. Define deterministic ordering for same-timestamp trigger collisions.
2. Add explicit queue/back-pressure behavior (drop oldest/newest/coalesce).
3. Lock behavior with tests that replay known collision patterns.

Definition of done:
- Collision policy is documented and enforced by deterministic tests.

## Workstream C: multi-user interaction patterns

1. Define source identity and fair-share rules for simultaneous performers.
2. Add per-source quotas to prevent one controller from monopolizing bursts.
3. Add observability fields so UI can show "who owns the queue."

Definition of done:
- Multi-source runs stay stable under load with no source starvation.

## Open questions

- Should burst queues be per-engine, per-seed, or globally brokered?
- Do we prioritize temporal accuracy or fairness under overload?
- What safety fallback should panic trigger when queues saturate?
