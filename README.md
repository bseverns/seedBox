# SeedBox — PlatformIO Scaffold (Teensy 4.0 + Native Sim)

This is a ready-to-build scaffold for **SeedBox** on **Teensy 4.0** with a **native (host) test env**.
It compiles in two modes:

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

## Notes

- Hardware audio is stubbed behind `SEEDBOX_HW` macro; native stubs use a monotonic sample counter.
- First engine is a **Sampler**; file is included only in hardware env by `build_src_filter`.
- Extend with Granular/Resonator engines later; keep IO and DSP modular.
