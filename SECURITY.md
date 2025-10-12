# Security Policy

We take the safety of performers, hackers, and listeners seriously. If you spot
an exploit that could hurt hardware, users, or their data, please let us know
quietly so we can fix it before it spreads.

## Supported versions

SeedBox is a fast-moving lab. We support:

- The `main` branch
- The most recent tagged release (if any)

Older versions might get best-effort fixes, but no promises.

## Reporting a vulnerability

- Email: `security@seedbox.dev`
- Optional backup: open a private GitHub security advisory for the repo.

When you write in, please include:

- A clear description of the issue and the potential impact.
- Steps or code snippets to reproduce the problem.
- Any logs, proof-of-concept patches, or crash dumps that help us verify.

You can encrypt reports using our PGP key fingerprint `F00D 1A7B 1DEA CAFE 5EED`
(ask for the full key if you need it).

## Response process

1. We acknowledge new reports within 5 business days.
2. We investigate, reproduce, and start drafting a fix.
3. We coordinate a release window with you, including CVE requests when needed.
4. Once a fix ships, we credit reporters who want their name in lights.

If a vulnerability also affects upstream dependencies, we coordinate with their
maintainers before disclosing.

## Public disclosure

Please keep reports private until we ship a patch or agree on a public date.
Early disclosure can put live performances at risk.
