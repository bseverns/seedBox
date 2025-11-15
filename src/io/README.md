# IO plumbing cheat sheet

Welcome to the IO corner of SeedBox: part switching yard, part field notebook. This is where the firmware learns how to speak MIDI, juggle presets, and keep real hardware honest. Treat it like lab notes you can scribble in while you solder.

## Store vs. Storage vs. friends

* **`seedbox::io::Store`** is the bare-metal contract. It exposes `list`, `load`, and `save` so the rest of the app can ask “what preset slots exist?” without caring whether the bytes live in EEPROM, on SD, or inside a simulator blob. Hardware builds get EEPROM + SD drivers; native builds inject host shims so tests never need a Teensy nearby.【F:include/io/Store.h†L18-L79】
  * `StoreNull` stays read-only for labs that want to fake “no disk inserted”.【F:include/io/Store.h†L32-L40】
  * `StoreEeprom` wraps the Teensy’s EEPROM (or a mock array on desktop) and automatically refuses writes when `QUIET_MODE` is on **and** we’re running on real hardware, which keeps classroom rigs from being overwritten mid-lesson.【F:include/io/Store.h†L42-L63】【F:src/io/Store.cpp†L444-L520】
  * `StoreSd` mirrors an SD card; on the simulator it maps to a host directory so tests can still poke JSON files without touching silicon.【F:include/io/Store.h†L65-L79】
* **`Storage::*` helpers** are the compatibility layer for legacy call sites. They parse URIs like `"eeprom:lead"` or `"sd://banks/jam.json"`, hydrate `AppState` snapshots, and honor the same quiet-mode write lock that the stores enforce. Loading stays available in quiet mode so you can browse banks even when writes are blocked.【F:src/io/Storage.h†L9-L27】【F:src/io/Storage.cpp†L1-L420】
* **`MidiRouter`** orchestrates every MIDI transport we expose. It boots USB + TRS hardware backends, injects CLI stand-ins when we compile for the native test harness, and offers per-page routing tables plus channel maps so labs can teach how transport/clock mirroring really works. The CLI adapter doubles as a teachable stub for integration tests: queue fake events, read back what would have hit the wire.【F:src/io/MidiRouter.h†L3-L199】

`include/io/Store.h` is the canonical spec for storage backends. If you invent a new persistent medium, implement the interface there and wire it through `Storage.cpp`; the helper functions will keep old UI code happy while you extend the router or stores.

## Quiet-mode guard rails

`QUIET_MODE` defaults to `1`, meaning “don’t let tests or curious students accidentally scribble over hardware”. Both `StoreEeprom::save` and `Storage::saveScene` short-circuit writes whenever we’re running on a real device with quiet mode still enabled. Native builds skip the guard so regression suites can round-trip JSON fixtures on disk.【F:src/io/Store.cpp†L444-L520】【F:src/io/Storage.cpp†L385-L420】

Need the router active for a bench test? Compile with `-DQUIET_MODE=0` as described in the TRS sync lab notes. Otherwise, expect saves to silently no-op while loads continue to work.

## Where the tests slam the backends

The native golden harness leans hard on the CLI backends: it pushes clock/start CC traffic through `MidiRouter::cliBackend(...)`, asserts the routed messages, and even runs panic to verify we flush latched notes. Peek at the clock routing, channel-map, and transport mirror tests for copyable patterns.【F:tests/native_golden/test_main.cpp†L201-L250】

UI regression tests boot the full simulator (`tests/test_app`) and hammer the same plumbing via the native board shim. They also guard the quiet-mode flag so we don’t regress the “read-only while muted” story.【F:tests/test_app/test_app.cpp†L1-L120】【F:tests/test_app/test_mn42_control.cpp†L1-L120】【F:tests/test_app/test_mn42_control.cpp†L121-L210】

## Wiring + deeper dives

* Need the literal wiring for TRS clock mirroring? The [TRS clock sync bench jam](../../docs/hardware/trs_clock_sync/README.md) spells out the pinout, build flags, and observations you should capture during lab runs.【F:docs/hardware/trs_clock_sync/README.md†L1-L64】
* Storage roadmap + quiet-mode philosophy live in [`docs/roadmaps/storage.md`](../../docs/roadmaps/storage.md). Pair it with the ethics + assumptions docs if you’re curious why we default to read-only hardware builds.【F:docs/roadmaps/storage.md†L1-L60】【F:docs/assumptions.md†L1-L32】【F:docs/ethics.md†L1-L28】
* For controller handshakes (MN42 and friends), keep [`docs/interop_mn42.md`](../../docs/interop_mn42.md) nearby. The simulator tests under `tests/test_app` reference the same map, so you can bounce between docs and code when tuning CC behaviour.【F:docs/interop_mn42.md†L100-L140】【F:tests/test_app/test_mn42_control.cpp†L1-L120】

Got a better metaphor, or found a corner where the guard rails wobble? Drop notes right here. This folder is intentionally half studio log, half teaching guide.
