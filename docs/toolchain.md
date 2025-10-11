# Toolchain ledger

We pin the exact versions that SeedBox ships with so the native and Teensy
builds stay reproducible.

## Current pins

| Target | Platform | Framework package | Notes |
| ------ | -------- | ----------------- | ----- |
| `native` | `native@1.2.1` | _(none — bare-metal g++ build)_ | Uses host g++ with the HAL mocks and standard library. |
| `teensy40` | `teensy@5.0.0` | `platformio/framework-arduinoteensy@~1.159.0` | Teensy 4.0 USB MIDI/Serial profile. |

### Libraries

```
# shared across envs
bblanchon/ArduinoJson@7.4.2

# teensy40 only
PaulStoffregen/Audio@1.3
PaulStoffregen/OctoWS2811@1.5
PaulStoffregen/Encoder@1.4.3
fortyseveneffects/MIDI Library@5.0.2
greiman/SdFat@2.3.1
adafruit/Adafruit SSD1306@2.5.15
adafruit/Adafruit GFX Library@1.12.3
adafruit/Adafruit BusIO@1.17.4
```

We pull Paul Stoffregen's libraries directly from the PlatformIO registry so the
versions line up with the Teensyduino bundle. If the registry ever hiccups,
swap the spec for a `https://github.com/...git#<commit>` reference and note it
in this ledger.

## Reproducing the pin set

These commands print the resolved package metadata. Run them after bumping
versions to update the table above.

```bash
pio pkg list -e native
pio pkg list -e teensy40
pio run -e native -t envdump
pio run -e teensy40 -t envdump
```

PlatformIO writes package versions to `.pio` during resolution. Capture the
output and commit any updated version strings.

## Updating pins intentionally

1. Modify the relevant entry in `platformio.ini`.
2. Run `pio pkg update -e <env>` to fetch the new package.
3. Re-run the commands above to verify the resolved versions.
4. Update this file and cross-check the resulting binaries via native tests and
   (when possible) hardware smoke builds.

We avoid automatic updates so studio sessions stay deterministic.
