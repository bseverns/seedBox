# Ethics and privacy stance

SeedBox is playful, but we treat privacy and agency as first-class.

## Quiet Mode (default)

`QUIET_MODE` compiles in by default. That means:

- No SD card writes.
- No network calls.
- Deterministic RNG seeds seeded with `0x5EEDB0B1` unless overridden.

Disable `QUIET_MODE` by adding `-D QUIET_MODE=0` to `platformio.ini` or by
defining it in a local `seedbox_config.h` (see README for details).

## Data handling principles

- We only store what musicians explicitly ask for (e.g. pattern exports).
- Logs stay on-device unless you copy them off.
- Future analytics hooks will be opt-in with clear toggles.

## Responsible disclosure

Security issues? Read [`SECURITY.md`](../SECURITY.md). We prefer encrypted
reports once the PGP key is published (TODO).

## TODO

- TODO: Publish the PGP public key for security reports.
- TODO: Document how golden audio fixtures will be handled so the dataset stays
  respectful of contributors.
