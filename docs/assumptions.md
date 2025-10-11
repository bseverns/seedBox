# Assumption ledger

We log the bets that keep SeedBox snappy and musical. When a bet breaks, update
this page instead of letting lore rot in comments.

## Timing

- Audio callback budget: 128 samples @ 48 kHz → 2.67 ms per buffer.
- Reserve at least 25% headroom for jitter and IO slop.
- MIDI handling is interrupt-driven on Teensy; native builds poll the mock queue
  once per `AppState::tick()`.

## Memory

- No dynamic allocation inside audio callbacks. Pools are warmed during setup.
- `QUIET_MODE` disables SD writes and networking so native tests run repeatably.
- Grain engines assume 36 simultaneous voices on hardware, 12 on native.

## Interop

- MN42 handshake uses CC#14/21/51 and NRPN `0x77/0x01`.
- External clock takes precedence only after the first tick arrives.

## Testing

- Golden audio fixtures are TODO. Harness exists under `tests/native_golden`.
- Native tests run with deterministic RNG seeds.

## TODO bets

- TODO: Document SD card throughput limits once we profile the HAL storage seam.
- TODO: Revisit buffer counts when we add new engines or reverb tails.
