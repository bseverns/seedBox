# Security policy

Even playful synth labs deserve responsible disclosure. If you find a
vulnerability, please follow the steps below so we can patch it quickly without
spoiling anyone's jam session.

## Supported versions

SeedBox is pre-release software. We support the `main` branch and the latest
numbered tag. Older tags may receive security backports at our discretion if the
issue is severe and the fix is low-risk.

## Reporting a vulnerability

- Email `seedbox-security@bseverns.dev` with a short summary, reproduction steps,
  and any logs you can share without exposing personal data.
- Encrypt mail if you prefer using [this public key](TODO: publish PGP key).
- We aim to acknowledge new reports within five business days.

Please do **not** open public issues for security bugs until we've shipped a
fix or explicitly agree to disclosure timing.

## Handling process

1. We confirm the report, reproduce the issue, and assess severity.
2. A fix branch lands in private until reviewers are satisfied.
3. We publish a security advisory in the changelog and tag release notes.
4. Credits go to the reporter unless anonymity is requested.

If you discover a vulnerability in a dependency, notify the upstream project as
well. We'll coordinate when possible so the broader community benefits.
