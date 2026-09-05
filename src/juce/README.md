# JUCE bridge crib notes

Welcome to the laptop-friendly front door. This folder is the "plug it into a DAW" twin of the Teensy firmware:

- `SeedboxAudioProcessor` wraps the exact same `AppState`/engine stack the embedded build uses. The processor eats JUCE audio + MIDI buffers, forwards them through `hal::audio`/`MidiRouter`, and spits the results back at the host.
- `SeedboxAudioProcessorEditor` keeps the UI intentionally small: a master-seed control and a scrolling snapshot of what the firmware thinks the OLED should show. Lean is deliberate — the code is meant to be read, tweaked, and learned from. No extra HAL macros are exposed here; the laptop build is meant to focus on the high-level mixer/transport controls a DAW can drive without emulating hardware-only sysex.
- `SeedboxApplication` is a JUCEApplication that replaces the Arduino `setup`/`loop` dance. It wires an `AudioProcessorPlayer` into the native device manager so the engine runs with whatever buffer size and sample rate your machine advertises.

Design vibe: half studio logbook, half teaching lab. Nothing is hidden behind magic factory functions, and comments try to explain _why_ the plumbing looks the way it does. If you want to hack on the engines from a DAW, start here and go wild.

Persistence: VST3 state rides along with the host chunk via the APVTS tree plus a base64 preset blob (includes per-seed engines and BPM). The standalone now keeps a tiny `ApplicationProperties` file that restores the last editor page + window size so you drop back into your last teaching groove instead of the default HOME screen.

## Hardware panel-first view (new default)

The default panel mirrors [the panel drawing](../../assets/front-panel-map.svg):
four encoder/switch assemblies and four standalone momentary buttons. Labels,
script tokens, and GPIO assignments come from
[`PanelControls.h`](../../include/hal/PanelControls.h).

- **Encoders:** Seed/Bank, Density, Tone/Tilt, FX/Mutate. Drag to turn; right-click
  and hold the knob to operate its integrated switch. Rotations and switches
  travel through Board and the same page/gesture policy as firmware.
- **Buttons:** Tap Tempo, Shift, Alt Seed, Live Capture. Holding Seed/Bank
  reseeds outside Storage. Holding Alt Seed opens Storage, where releasing the
  Seed/Bank switch recalls (under 450 ms) or saves (at least 450 ms).
- **Capture:** pressing Live Capture captures/reseeds; continuing to hold fires
  panic. There are no standalone Reseed or Lock buttons in the physical layout.
- **Timing:** desktop panel holds use elapsed time; native scripted tests retain
  deterministic 10 ms steps. Every mouse edge is sampled immediately.
- **OLED:** the application display snapshot shows mode, status, and hints.
- **Jacks:** decorative badges open desktop audio/MIDI helpers.
- **Desktop shortcuts:** Space toggles transport; `T` taps tempo, `O` mirrors the
  Tone/Tilt switch, `1`–`4` focus seeds, `E` cycles engines, and arrows turn the
  last touched knob one step. These host shortcuts supplement the panel.

The optional legacy editor (`-DSEEDBOX_LEGACY_UI=1`) retains its host parameter
pages. The [panel cheat sheet](../../docs/panel_cheat_sheet.md) describes physical
controls; host parameter pages below are additional desktop editing surfaces.

## How to hear audio (monitoring + test tone priority)

- **Standalone:** launch the app, flip to the SETTINGS page, and pick a stereo input + output in the inline Audio/MIDI selector. The headphone jack popup is still around for a quick restart or summary, but the selector now lives in the page body for faster onboarding. With inputs enabled, the processor copies input → output every block _unless_ the test tone or engines are actively generating sound. Test tone/engines always win; passthrough fills the gap when they are silent. Mono inputs get mirrored to stereo automatically.
- **VST3:** drop the plugin on a DAW track as an insert effect, make sure the track is record/monitor-enabled, and confirm the VST3 reports a stereo input bus. Monitoring is always on in the plugin as well; the same “engine or test tone beats passthrough” rule applies.
- **Quick sanity:** flip test tone on, hear the synthesized whine, then disable it and speak/play into the selected input. You should immediately hear dry input unless the engines are busy.

## Page tour (what each mode actually surfaces)

- **HOME** – Master seed knob (automatable), transport latch toggle, BPM readout + clock source, focused-seed readout, and the OLED snapshot. Great for a “just jam” landing pad.
- **SEEDS** – Master seed plus focus selector and a "Randomize focused seed" button that reseeds the current slot. Gate controls live here too: a beat/bar division dropdown for the live-input reseed gate and a logarithmic "Gate floor" slider so you can decide how hot the input needs to be before a reseed is queued. Use this page to audition genomes without touching the engine lane _and_ to tune the reseed cadence/sensitivity when you’re teaching the gate.
- **ENGINE** – Engine dropdown, granular source slider, and the live engine knob grid. Knobs write straight into the focused seed (tone/density/probability/etc. depending on engine). Helper bullets summarize the macro mapping for the selected engine.
- **PERF** – Tempo slider + tap-tempo button, transport latch toggle, and the home BPM/clock/focus readouts. Treat it as your performance desk.
- **SWING** – Swing macro plus quantize scale/root selectors. Swing shares space with PERF so tempo nudging and groove sculpting can happen together.
- **SETTINGS** – External clock and "follow external" toggles, the inline AudioDeviceSelector (standalone only) with device/channel picks, and a clock source readout. The headphone jack popup still exists for quick restart/summary, but the full picker now rides on this page so students don't have to chase a hidden menu.
- **UTIL** – Debug meters toggle plus a panic/reset button. The helper text panel remains visible here so you still get a cheatsheet while debugging.

## Quick desktop sanity checks

- Fire up the standalone: the device selector now asks for two inputs and two outputs so you can monitor a synth or mic straight through the plugin. The processor renders engine/test tone into a scratch buffer first; if that buffer is silent we blast the input straight to the output bus (mono duplicated if needed). If the engine or test tone is active it takes priority.
- In a DAW, enable track monitoring and make sure the VST3 reports a stereo input bus. The plugin audio heartbeat (`app_.tickHostAudio()`) now runs every block, while a processor-owned JUCE timer services `serviceHostMaintenance()` off the audio callback so deferred upkeep no longer depends on an open editor.
- Host parameter changes still mutate the runtime immediately where that cost is small, but the maintenance timer now owns the heavier seed-state sync path plus a small fixed-size deferred set for master-seed reseeds, focused-engine changes, quantize, live-input gate settings, and granular-source stepping.
- The editor/panel now consume the cached display snapshot the maintenance timer already builds. OLED/debug text only rebuilds when the runtime marks that snapshot dirty; the output meter still updates live on the UI timer.
- In `SeedBoxConfig::kUiDebug` builds, the overlay now also shows host diagnostics for dropped MIDI messages and fail-closed oversize blocks. The overlay reads those through the shared `AppState::DiagnosticsSnapshot::host` snapshot, so they are not editor-local only.
- The main editor display and desktop panel now also surface a light warning when those host counters are nonzero, so callback trouble is visible even outside the debug overlay.
- ENGINE knobs in the editor are live. Tweaks write straight into the focused seed, stash per-seed properties in the APVTS state tree (`seed0` .. `seed3`), and get replayed when the DAW reloads a preset.

## Keyboard shortcuts (desktop stand-in for panel combos)

- Spacebar toggles the transport latch/run flag.
- `T` taps tempo only — no more surprise Tone/Tilt toggles while you chase a beat.
- `O` mirrors the Tone/Tilt button so you can still hammer Tone + modifiers from the keyboard.
- `1`–`4` jump focus to the matching seed.
- `E` cycles engines without mousing the dropdown.
- Arrow keys nudge the currently visible control (engine knobs get first dibs, then tempo/swing) in small increments.
- Hold `O`, `S`, or `A` to mirror the Tone/Shift/Alt buttons from the panel — helpful when you want to mash combos while typing.

## Persistence: what sticks and where

- **VST3:** The APVTS tree captures standard parameters plus per-seed children (`seed0`–`seed3`), internal tempo, and the serialized preset blob. Save a DAW session or preset and you get seeds/engines/tempo back on reload with no extra switches.
- **Standalone:** `ApplicationProperties` writes a tiny settings file (`SeedBox.settings` under your OS-specific app data folder) that remembers the last visible page and window geometry. Close/reopen and you land exactly where you left off.
- **Good practice:** After tweaking seeds/engines, save a DAW preset and quit/relaunch the standalone once to confirm your page + window state come back. That’s the fast way to prove persistence is healthy before handing this to students.

## Keyboard shortcuts (desktop stand-in for panel combos)

- Spacebar toggles the transport latch/run flag.
- `T` taps tempo only — no more surprise Tone/Tilt toggles while you chase a beat.
- `O` mirrors the Tone/Tilt button so you can still hammer Tone + modifiers from the keyboard.
- `1`–`4` jump focus to the matching seed.
- `E` cycles engines without mousing the dropdown.
- Arrow keys nudge the currently visible control (engine knobs get first dibs, then tempo/swing) in small increments.

## Manual smoke tests (host + standalone)

1. Standalone monitoring: launch the app, pick an input and output in the audio settings, talk/play — dry input should hit the output until you enable test tone or the engines make sound. Close and reopen: the window size and last page should come back.
2. VST3 monitoring: drop the plugin on a DAW track, enable input monitoring, and confirm passthrough works. Save the DAW project and reload — focused seed, engine tweaks, and tempo should restore thanks to the APVTS + preset blob combo.
3. Tempo + transport + seed focus: use the PERF/HOME tempo slider and tap button, spacebar to latch/stop, and keys 1–4 to change focus; watch the UI reflect each move (tick-driven).
4. Engine edits: adjust ENGINE knobs for the focused seed, flip engines with `E`, and confirm edits persist when you reseed or reload a DAW session.
