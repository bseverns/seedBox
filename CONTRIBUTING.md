# Contributing to SeedBox

Welcome, friend. This repo is a studio notebook and a teaching notebook rolled
into one. Contributions are welcome as long as they keep the vibe generous and
the code reliable.

## Ground rules

- Keep branches small and purposeful. Prefix branch names with your handle and a
  hyphenated summary, e.g. `yourname-hal-audit`.
- Write commit messages in the present tense with a compact subject line and an
  optional body wrapping at 72 columns.
- Submit pull requests that explain *why* the change matters. Link to docs,
  diagrams, and tests.
- Respect the [Code of Conduct](CODE_OF_CONDUCT.md). Punk rock is cool; abuse is
  not.

## Development environments

SeedBox rides on PlatformIO. We ship two pinned environments in
[`platformio.ini`](platformio.ini):

- `native` for host builds, fast iteration, and test coverage.
- `teensy40` for the actual hardware build that targets the Teensy 4.0.

Regenerate pinned versions by following [`docs/toolchain.md`](docs/toolchain.md).

## Running the native test suite

```bash
pio run -e native
pio test -e native
```

Tests must pass with `QUIET_MODE` enabled (default) and with `ENABLE_GOLDEN`
*disabled*. If you need to exercise the golden harness locally, compile with
`-D ENABLE_GOLDEN=1` and be ready to regenerate fixtures (see
[`tests/native_golden/README.md`](tests/native_golden/README.md)).

## Hardware compile smoke test

```bash
pio run -e teensy40
```

We do not require uploads during review. Provide compile logs if something looks
weird.

## Audio-thread safety expectations

- **No dynamic allocation** inside audio callbacks. Pre-size buffers, pools, and
  trackers during setup.
- Keep callback execution under the documented deadlines in
  [`docs/hal.md`](docs/hal.md).
- Document any assumptions inside [`docs/assumptions.md`](docs/assumptions.md)
  so future contributors can reason about the timing budget.

## Docs and tone

- Use Markdown headings generously. Short paragraphs win.
- Insert **TODO** markers for any future audio captures or screenshots.
- Reference `QUIET_MODE`, `SEEDBOX_HW`, and `ENABLE_GOLDEN` explicitly when they
  matter.

Thanks for helping keep SeedBox fun *and* production-ready.
