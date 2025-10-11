# Toolchain pins and regeneration rituals

This page keeps the PlatformIO stack reproducible. Treat it like the pit crew's
clipboard: the exact versions, how to inspect them, and how to update them
without breaking the vibe.

## Current pins

| Layer | Package | Version | Notes |
| ----- | ------- | ------- | ----- |
| Platform (native) | `platformio/platform-native` | 5.1.1 | Hosts the simulator builds. |
| Platform (hardware) | `platformio/platform-teensy` | 5.2.0 | Targets Teensy 4.0. |
| Framework | `framework-arduinoteensy` | 1.59.0 | Bundled via `platform_packages`. |
| Audio engine | `teensy/Audio` | 1.59.0 | Core DSP + SGTL5000 glue. |
| LED driver | `teensy/OctoWS2811` | 1.5.0 | Optional, pinned for future light rigs. |
| Encoder helper | `teensy/Encoder` | 1.4.2 | Hardware knob reading. |
| MIDI | `fortyseveneffects/MIDI Library` | 6.0.1 | USB + DIN handling. |
| JSON | `bblanchon/ArduinoJson` | 7.0.4 | Deterministic metadata parsing. |
| Storage | `greiman/SdFat` | 2.2.3 | SD card access for samples. |
| Display | `adafruit/Adafruit SSD1306` | 2.5.9 | OLED driver. |
| GFX core | `adafruit/Adafruit GFX Library` | 1.11.10 | Shapes/fonts for the OLED. |
| Bus helper | `adafruit/Adafruit BusIO` | 1.16.1 | Shared I²C/SPI glue. |

## Check the installed versions

```bash
pio pkg list
pio run -e teensy40 -t envdump | grep -E "(platform|framework|lib_deps)"
```

The first command lists cached packages. The `envdump` filter shows the resolved
values for a specific environment so you can copy/paste into docs or bug reports.

## When versions change

1. Update `platformio.ini` with the new explicit versions.
2. Run `pio pkg update` to pull them down.
3. Record the change in `CHANGELOG.md` and mention why (bug fix, security bump,
   new feature).
4. Update this doc and note any migration steps.
5. Re-run the full build matrix.

Keep the pins tight. Deterministic tools make future golden-audio captures easier
and keep MN42 interop predictable.
