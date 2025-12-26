# JUCE builds without the mystery meat

Think of this page as the hostel kitchen logbook for desktop builds: messy, loud,
and meant to teach. The top-level `CMakeLists.txt` now pulls JUCE via
`FetchContent`, spins up both a VST3 plugin and a standalone app, and threads the
same build flags PlatformIO uses so the DAW story matches the firmware truth.

## Quick start

1. **Clone dependencies.** The CMake project fetches JUCE straight from GitHub.
   Make sure you have Git and CMake ≥ 3.22 on your path.
2. **Configure out-of-tree.**

   ```bash
   cmake -S . -B build/juce \
     -DSEEDBOX_SIM=ON \
     -DQUIET_MODE=OFF \
     -DSEEDBOX_VERSION="$(git rev-parse --short HEAD)"
   ```

   Defaults already keep `QUIET_MODE=OFF` and `SEEDBOX_DEBUG_UI=OFF`, so the
   line above is mostly there to make intent loud and obvious while you learn.
   Quiet mode is a firmware safety belt; on desktop we want audio and MIDI to
   breathe unless you explicitly shut the valves.

   > Shell gotchas: keep the trailing `\` characters flush against the newline.
   > A stray space after the backslash makes zsh try to run `-DQUIET_MODE=...`
   > as a command (that “command not found” you might have seen). If you ever
   > see CMake claim it is forcing flags you did not set, nuke `build/juce` and
   > re-run the configure step so a stale cache cannot lie to you.

3. **Build the standalone app.**

   ```bash
   cmake --build build/juce --target SeedboxApp
   ```

4. **Build the VST3.**

   ```bash
   cmake --build build/juce --target SeedboxVST3
   ```

Both targets live in the same build tree. The plugin now drops its bundle into
`build/juce/SeedboxVST3_artefacts/<CONFIG>/VST3/SeedBox.vst3` (swap `<CONFIG>`
for `Debug`, `Release`, etc.) so you always know where the payload landed even
if JUCE’s automatic user-level copy goes missing on macOS.

## CI artifact vibe check: universal vs arm64

The CI pipeline uploads two macOS bundles because reality is messier than a
single zip:

- **`seedbox-macos-universal`** is the big tent. It ships a universal binary
  bundle with *both* `x86_64` and `arm64` slices, so Intel Macs and Apple
  Silicon can run the same artifact without a translation layer.
- **`seedbox-macos-arm64`** is the lean street-cut. CI takes the universal
  bundle, thins it down with `lipo -thin arm64`, and re-packages the VST3 + app
  so you can grab a smaller, Apple Silicon-only download without carrying the
  Intel luggage.

Translation: universal is compatibility-first, arm64 is bandwidth + startup
speed-first. Pick the vibe that matches your users.

Need the short answer for actually _hearing_ things and driving the UI? Jump to
[`src/juce/README.md`](../src/juce/README.md) for the monitoring flow, page
tour, keyboard map, and persistence notes (test tone beats passthrough; engines
write back into per-seed state; standalone remembers your last page/window).

## Flag mirroring (PlatformIO → CMake)

Native determinism depends on the same defines PlatformIO injects. The CMake
options below are wired straight into the JUCE targets so the simulator behaves
exactly like `pio test -e native`:

| CMake option | Mirrors PlatformIO flag | Default | Why you care |
| --- | --- | --- | --- |
| `SEEDBOX_JUCE` | `-D SEEDBOX_JUCE=1` | `ON` | Flags the JUCE build path and keeps the Arduino-only headers on the bench. |
| `SEEDBOX_SIM` | `-D SEEDBOX_SIM=1` | `ON` | Enables the native board + audio shims used by tests and the DAW bridge. |
| `QUIET_MODE` | `-D QUIET_MODE=1` | `OFF` | Keeps serial/MIDI spam muted so hosts do not get flooded. Turn it on if you want the studio rig to stay politely silent. |
| `SEEDBOX_VERSION` | `-D SEEDBOX_VERSION="${PIO_SRC_REV}"` | `"dev"` | Propagates a version stamp into presets and debug logs. |
| `SEEDBOX_PROJECT_ROOT_HINT` | `-D SEEDBOX_PROJECT_ROOT_HINT` | repo root | Lets tests and fixtures resolve paths the same way PlatformIO does. |
| `ENABLE_GOLDEN` | `-D ENABLE_GOLDEN=1` | `OFF` | Opt-in for writing golden artifacts from host builds. |
| `ARDUINOJSON_USE_DOUBLE` | `-D ARDUINOJSON_USE_DOUBLE=1` | `ON` | Keeps JSON parsing identical to the embedded toolchain. |
| `SEEDBOX_HW` | `-D SEEDBOX_HW=1` | `OFF` | Pulls in Teensy-only glue; leave off for JUCE unless you like missing headers. |
| `SEEDBOX_DEBUG_CLOCK_SOURCE` | `-D SEEDBOX_DEBUG_CLOCK_SOURCE=1` | `OFF` | Extra clock breadcrumbs when you need them. |
| `SEEDBOX_DEBUG_UI` | `-D SEEDBOX_DEBUG_UI=1` | `OFF` | Enables the JUCE UI debug HUD overlay (live state readouts). Hardware overlay still future-facing. |

The helper function in `CMakeLists.txt` converts each `ON/OFF` switch into `0/1`
defines so `SeedBoxConfig.h` stays happy.

## Targets and sources

- **`SeedboxApp`** is a JUCEApplication front door that wires the existing
  `SeedboxAudioProcessor` into a `AudioProcessorPlayer` plus the host device
  manager. It uses the same engine code as the Teensy build; only the platform
  shim changes.
- **`SeedboxVST3`** is the DAW-facing sibling. It exposes the
  `SeedboxAudioProcessor` as a VST3 instrument, with MIDI I/O enabled and the
  editor using the minimal teaching UI from `src/juce/`.
- The core engine lives in `seedbox_core` so the plugin and app share every
  translation unit except the firmware `main.cpp` and the Teensy-only
  `board_teensy.cpp`.

## Bundled assets (BinaryData)

JUCE bakes runtime assets into `BinaryData` so nothing depends on external files
at load time. We start with `assets/juce_motd.txt` (a lightweight “read this
first” tooltip for the Master Seed knob) and wire it into the editor via
`juce_add_binary_data`. Drop more files into `assets/` and the build will fold
them into both targets automatically once you add them to `juce_add_binary_data`.

## Rebuild rituals

- Re-run `cmake -S . -B build/juce` any time you toggle options or add source
  files so the globbed lists refresh.
- Nuking `build/juce` is safe if you want a clean slate; `FetchContent` will
  rehydrate JUCE on the next configure.
- The VST3 target pins its copy directory to
  `build/juce/SeedboxVST3_artefacts/<CONFIG>/VST3`, which doubles as the
  built-bundle output location. Peek there if your DAW does not see a fresh
  build.

## Host wiring crib notes (carry-overs from the old guide)

The CMake plumbing above is new; the host-side expectations are the same ones
spelled out in the original JUCE guide. Greatest hits, kept verbatim so the
context survives the refresh:

- `-DSEEDBOX_JUCE=1` keeps the Arduino headers benched and lights up the
  JUCE-specific glue. Pair it with `SEEDBOX_SIM=1` (default) and leave
  `SEEDBOX_HW` off so the simulator shims stay active.
- To run the native harness yourself: instantiate
  `seedbox::juce_bridge::JuceHost` with your `AppState`, call
  `initialiseWithDefaults()` to open the default stereo output + MIDI pair, or
  `configureForTests(sampleRate, blockSize)` if you only want CI to exercise the
  wiring without touching CoreAudio/ASIO/JACK.
- Let JUCE's `audioDeviceAboutToStart` tell us the real sample rate and block
  size. We mirror those straight into `hal::audio::configureHostStream`, so the
  engines prep against the exact numbers your host picked instead of assuming a
  hard-coded 48 kHz / 128 frame world.
- We render directly into the host-provided float buffers via
  `hal::audio::renderHostBuffer`, clamping to stereo (if JUCE only hands us mono
  we mirror the left channel). Latency conversations stay honest because we
  never resample or re-block behind JUCE's back.
- The JUCE backend is a first-class `MidiRouter` backend. Inbound messages ride
  a tiny queue inside `JuceMidiBackend` and flush during the audio callback so
  clock/transport stay in lockstep with your DAW. Outbound messages mirror the
  router's guard rails (channel maps, panic logic) and flow through
  `juce::MidiOutput`, so you can still demo the MN42 handshake without a Teensy
  on the table.
- `hal::audio::framesPerBlock()` and `hal::audio::sampleRate()` reflect whatever
  the host picked. If you need to force a rate for a demo, pass it into
  `configureForTests` and watch the HAL mirror it immediately.
- Keep the Teensy-only includes under `#if SEEDBOX_HW`. The JUCE build never
  pulls `HardwarePrelude` or SGTL5000 headers; the shared engines stay agnostic.
- Call `midi.poll()` somewhere in your host loop if you swap out `JuceHost` for
  custom glue — the router queues events until you explicitly flush them.
