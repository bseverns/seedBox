# Toolchain + Dependency Freeze Log

Welcome to the noisy little corner where we keep the build stack honest. Below are the exact PlatformIO packages the repo expects **today**. If you ever wonder “what version of X did we ship with?”, this is the authoritative answer until you regenerate the list.

## Pinned versions (2025-10-12)

| Package | Version | Notes |
| --- | --- | --- |
| `platformio/platform-teensy` | `5.0.0` | Matches the current `platform.json` on the upstream Teensy platform feed. |
| `platformio/framework-arduinoteensy` | `1.159.0` | Arduino 1.59 core used by the Teensy 4.0 target. |
| `platformio/platform-native` | `1.2.1` | Host build harness for the `native` env. |
| `teensy/Audio` | `1.3` | Version string from the library’s `library.properties`. |
| `teensy/OctoWS2811` | `1.5` | Ditto. |
| `teensy/Encoder` | `1.4.3` | Ditto. |
| `fortyseveneffects/MIDI Library` | `5.0.2` | Latest tagged release published upstream; update if PlatformIO resolves a newer major. |
| `bblanchon/ArduinoJson` | `7.4.2` | Current JSON core release. |
| `greiman/SdFat` | `2.3.1` | Matches upstream `library.properties`. |
| `adafruit/Adafruit SSD1306` | `2.5.15` | OLED driver. |
| `adafruit/Adafruit GFX Library` | `1.12.3` | Required by SSD1306. |
| `adafruit/Adafruit BusIO` | `1.17.4` | GFX/SSD1306 dependency. |

> **Heads-up:** The PlatformIO public registry currently returns HTTP 403 from inside this sandbox. Those versions above were scraped directly from each project’s `library.properties`/`platform.json`. When you’re on a network that can reach `https://api.registry.platformio.org`, re-run the steps below and update the table + `platformio.ini` together.

## Regenerating the table and `platformio.ini`

1. Install the PlatformIO CLI (`pip install platformio`) if it is not already on your PATH.
2. From the repo root, resolve the Teensy environment so PlatformIO downloads everything it needs:
   ```bash
   pio run -e teensy40
   ```
   If you only need metadata, `pio pkg install -e teensy40` works too and skips compilation.
3. Print the resolved package list (this is the canonical source for the pinned versions):
   ```bash
   pio pkg list
   ```
4. Capture the build configuration for posterity (great for diffing flag or board changes):
   ```bash
   pio run -t envdump -e teensy40
   ```
5. Update `platformio.ini` with the versions you just observed. Keep `native` and `teensy40` sections explicit—no shared `[env]` blocks hiding defaults.
6. While you are in there, sanity-check the hardware build flags. We now force-include `include/SeedboxTeensyCompat.h` from the
   Teensy environments so every single translation unit (including the vendored audio library) inherits the usual IDs
   (`ARDUINO_TEENSY40`, `TEENSYDUINO`, `__ARM_ARCH_7EM__`, `__IMXRT1062__`). PlatformIO has a habit of losing those defines in CI;
   when that happens the PJRC headers quietly downgrade to forward declarations—no `AudioMixer4::gain`, no granular engine, just
   sad abstract shells. The shim keeps the DSP graph tangible even when the toolchain forgets who it is.
7. Refresh this document with the new versions and any weirdness you hit (403s, missing packages, etc.). Future-you will be grateful for the breadcrumbs.

Stay curious, take notes, and don’t be afraid to scribble in the margins. That’s how this place stays half lab notebook, half zine.
