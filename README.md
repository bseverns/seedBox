# SeedBox — PlatformIO Scaffold (Teensy 4.0 + Native Sim)

Welcome to the sonic bunker. SeedBox speaks with the same gravelly confidence
as **MOARkNOBS-42**: this project exists to pick seeds, launch voices, and make
noise on purpose. It's a ready-to-build scaffold for **Teensy 4.0** with a
**native (host) test env**. It compiles in two modes:

- `teensy40_usbmidiserial`: real hardware with I²S audio, USB MIDI + Serial.
- `native`: host build for fast testing of schedulers, JSON, CC mapping (no Teensy libs).

## Quickstart

```bash
# 1) Install PlatformIO if needed
pip install -U platformio

# 2) Enter project
cd seedbox

# 3) Install libraries (first time)
pio pkg install

# 4) Run unit tests fast (native)
pio test -e native

# 5) Build hardware target
pio run -e teensy40_usbmidiserial

# 6) Upload (if using teensy-cli)
pio run -e teensy40_usbmidiserial --target upload

# 7) Open serial monitor
pio device monitor -b 115200
```

## Structure

```
scripts/gen_version.py   # emits include/BuildInfo.h from git info
include/                 # public headers
src/                     # implementation: app/ engine/ io/ util/
test/                    # Unity tests (native env)
```

## Seed lifecycle & voice doctrine

Every tick (24 PPQN) we march through the seed list and enforce this order of
operations — no compromises:

1. **Pick** – the scheduler decides which seed wakes up. Density gates and
   probabilities do the filtering so only the chosen ones speak.
2. **Schedule** – translate clock time + jitter into sample-accurate triggers so
   the DSP core can slam voices in on the grid (or just off it, artistically).
3. **Render** – the engine instantiates a voice from the seed's genome and
   routes it through the shared FX chain. This is where the sound actually hits.
4. **Mutate** – if `mutateAmt > 0` the engine walks a reversible random drift
   over whitelisted parameters (micro pitch, tone tilt, ±5% density, etc.) while
   staying deterministic thanks to the stored PRNG seed.

## Minimal DSP attack plan

We favor fast-to-first-sound setups. Pick your poison:

### Option A — Sampler / One-Shot (fastest)

- Preload up to 16 one-shots into RAM for snappy percussion or glitch kits; long
  clips can stream from SD without drama.
- Wrap each trigger with a contour: envelope + tilt-EQ + soft clipper, then dump
  it into the master delay / verb / limiter bus.

### Option B — Granular (expressive)

- Stream source audio from SD and keep 20–40 active grains per Teensy 4.0 for a
  comfy CPU margin.
- Grain size, spray, pitch, window, and stereo spread all come from the seed so
  patterns morph under deterministic control.

### Option C — Resonator / Ping (CPU-light, musical)

- Fire a short excitation burst into a Karplus-Strong string or modal bank.
- Seed pitch plus density/probability dictate the excite rate; perfect for
  metallic arpeggios that run all night.

## Operator console (what you see + touch)

### Display feed — MN42-grade 0.96" SSD1306 I²C OLED

- Uses the exact module from MOARkNOBS-42: 128×64 monochrome, address `0x3C`
  on 3V3 I²C (`SCL=19`, `SDA=18`). No reset pin needed; it auto-boots when the
  Teensy powers up.
- `AppState::captureDisplaySnapshot` prepares four 16-char lines you can blast
  straight to the screen. That snapshot is live in both hardware and native
  builds so your desktop sim mirrors the real thing.
- Line breakdown (mirrors MOARkNOBS ergonomics):
  1. **Title bar** – `SeedBox <masterSeed>` so you always know which RNG epoch
     you're hearing.
  2. **Status** – focused seed number + engine tag (`SMP`, `GRA`, `PING`) and
     current pitch offset in semitones.
  3. **Metrics** – density and probability so you can read the groove at a
     glance.
  4. **Nuance** – jitter (ms) and mutate amount so you can gauge how wild the
     seed is allowed to wander.
- When no seeds are active the display shouts `no seeds loaded` and invites you
  to reseed, which makes debugging obvious during bring-up.

### Controls + pin map

| Control | Teensy pins | Role | Notes |
|---------|-------------|------|-------|
| Encoder 1 (Seed/Bank + push) | A:0, B:1, Push:2 | Scrolls focused seed / bank select | Long-press wiring should call `AppState::reseed(...)` for RNG resets. |
| Encoder 2 (Density/Prob + push) | A:3, B:4, Push:5 | Rotate to balance density vs probability; push cycles parameter focus | Matching MOARkNOBS detent direction. |
| Encoder 3 (Tone/Tilt + push) | A:6, B:7, Push:8 | Sweeps tone tilt + EQ macros | Push toggles coarse / fine. |
| Encoder 4 (FX/Mutate + push) | A:9, B:10, Push:11 | Drives master FX send + mutate amount | Push arm for latch automation. |
| Tap Tempo button | 12 | Human clock in | Short tap = tempo, hold = clock source select. |
| Shift button | 13 | Modifier chord | Use for alternate encoder layers. |
| Alt Seed trigger | 14 | Instant seed swap | Handy for A/B testing. |
| Expression input 1 | 15 (analog) | CV in 0–3.3 V | Map to any macro via CC learn. |
| Expression input 2 | 16 (analog) | CV in 0–3.3 V | Pairs nicely with mutate depth. |

All pin assignments live in `include/HardwareConfig.h` so firmware and CAD stay
in lockstep. Adjust once there and the rest of the code picks it up.

## Seed boot + reseed rituals

- Boot (hardware or sim) calls `AppState::primeSeeds(...)` under the hood via
  `initHardware` / `initSim`. Four seeds spawn immediately from the master RNG
  seed (`0x5EEDB0B1` by default) and feed the scheduler before the first tick.
- The snapshot + scheduler always reference the currently focused seed. Call
  `AppState::setFocusSeed(index)` when an encoder movement changes focus so the
  display tracks the performer.
- To reseed on **hardware**, wire your long-press handler (typically Encoder 1
  push) to `app.reseed(newSeedValue)`. If you pass `0` the app falls back to
  the default master seed, otherwise it uses whatever 32-bit epoch you provide.
- To reseed in the **native sim**, poke the global `app` object from your test
  harness or REPL:

  ```cpp
  app.reseed(0xDEADBEEF);
  app.setFocusSeed(2);
  AppState::DisplaySnapshot snap;
  app.captureDisplaySnapshot(snap);
  ```

  That routine mirrors what the hardware UI will render, so you can script
  regression tests that assert against the display without touching a screen.

- Every reseed regenerates the same deterministic seed table on both targets
  thanks to the shared xorshift RNG, so snapshots, scheduler hits, and audio
  all stay in sync.

## Notes

- Hardware audio is stubbed behind `SEEDBOX_HW`; native builds use a monotonic
  sample counter so schedulers behave.
- The sampler engine is the first citizen; PlatformIO `build_src_filter` keeps
  its hardware-only pieces out of native builds.
- Granular and resonator engines plug into the same seed doctrine, so extending
  the box is just more seeds, not more chaos.
