# Cross-body parity records

A parity record compares one Seed Card across `native`, `juce`, and `hardware`.
It records lineage, scheduling, engine choice, control behavior, and output
characteristics separately. Matching behavior does not mean matching samples:
audio renderers, host buffers, converters, and analog hardware can differ.

Each body is `observed`, `unavailable`, or `not_run`. A record may claim
behavioral parity only when at least two bodies have observed evidence for every
comparison dimension. A missing body is useful evidence about coverage, not a
reason to fill in a pass.

Run `python3 scripts/validate_cross_body_parity.py` after adding a record.
