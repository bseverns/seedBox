# Security Policy

SeedBox ships playful tools, but we take responsible disclosure seriously.

## Supported versions

We currently support the `main` branch. Releases are tagged in
[`CHANGELOG.md`](CHANGELOG.md). Older tags receive fixes on a best-effort basis
only.

## Reporting a vulnerability

- Email `security@seedbox.sound` (placeholder address) with the subject
  `SeedBox Vulnerability Disclosure`.
- Include a proof of concept, affected commit or tag, and any mitigation ideas.
- Encrypt sensitive reports with our PGP key (TODO: publish key in `docs/ethics.md`).
- Please allow us 7 business days to acknowledge, then coordinate a release
  window before going public.

## Safe harbor

We will not pursue legal action against researchers who:

- Make a good faith effort to avoid privacy violations or service disruption.
- Do not exfiltrate data beyond what is necessary to demonstrate the issue.
- Give us reasonable time to remediate before disclosure.

## Preferred scope

- Native builds (`pio run -e native`) and HAL mocks.
- Teensy 4.0 firmware (`pio run -e teensy40`).
- Docs, tooling, and PlatformIO scripts.

Out of scope: third-party libraries, upstream Teensy hardware, or unrelated
internet services.

Thanks for keeping SeedBox weird *and* safe.
