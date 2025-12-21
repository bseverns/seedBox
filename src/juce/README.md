# JUCE bridge crib notes

Welcome to the laptop-friendly front door. This folder is the "plug it into a DAW" twin of the Teensy firmware:

- `SeedboxAudioProcessor` wraps the exact same `AppState`/engine stack the embedded build uses. The processor eats JUCE audio + MIDI buffers, forwards them through `hal::audio`/`MidiRouter`, and spits the results back at the host.
- `SeedboxAudioProcessorEditor` keeps the UI intentionally small: a master-seed control and a scrolling snapshot of what the firmware thinks the OLED should show. Lean is deliberate — the code is meant to be read, tweaked, and learned from. No extra HAL macros are exposed here; the laptop build is meant to focus on the high-level mixer/transport controls a DAW can drive without emulating hardware-only sysex.
- `SeedboxApplication` is a JUCEApplication that replaces the Arduino `setup`/`loop` dance. It wires an `AudioProcessorPlayer` into the native device manager so the engine runs with whatever buffer size and sample rate your machine advertises.

Design vibe: half studio logbook, half teaching lab. Nothing is hidden behind magic factory functions, and comments try to explain _why_ the plumbing looks the way it does. If you want to hack on the engines from a DAW, start here and go wild.

Persistence: VST3 state rides along with the host chunk via the APVTS tree plus a base64 preset blob (includes per-seed engines and BPM). The standalone now keeps a tiny `ApplicationProperties` file that restores the last editor page + window size so you drop back into your last teaching groove instead of the default HOME screen.

## Hardware panel-first view (new default)

The editor now boots straight into a single-page "panel" that mirrors the SVG sketch in the brief. Think of it as a desk-sized control surface: one rounded rectangle, an OLED rail, four jumbo encoders, five tactile buttons, and a run of jack badges. The legacy multi-page UI still exists behind `-DSEEDBOX_LEGACY_UI=1` if you want the old tabbed tour.

- **Knobs:** Seed Bank (focus 1–4 + click-to-cycle), Density, Tone/Tilt (Shift = fine tilt), FX/Mutate (Spread; click cycles the engine).
- **Buttons:** Tap (long-press toggles transport latch), Shift + Alt as momentary modifiers, Reseed (Alt saves a quick preset), Lock (Alt recalls the quick preset) tied to the per-seed lock state.
- **OLED:** Monospace snapshot of the firmware display, refreshed ~15 Hz so you still see page hints/metrics.
- **Jacks:** Decorative circles with light-touch helpers (MIDI clock follow/clock source toggles, audio device nudge) to mimic the panel art.
- **Shortcuts:** Space = latch transport, `T` = tap tempo, `O` mirrors the Tone/Tilt button, `1`–`4` focus seeds, `E` cycle engines, arrows nudge the last touched control. Shift/Alt keys still mirror the hardware modifiers.

## How to hear audio (monitoring + test tone priority)

- **Standalone:** launch the app, flip to the SETTINGS page, and pick a stereo input + output in the inline Audio/MIDI selector. The headphone jack popup is still around for a quick restart or summary, but the selector now lives in the page body for faster onboarding. With inputs enabled, the processor copies input → output every block _unless_ the test tone or engines are actively generating sound. Test tone/engines always win; passthrough fills the gap when they are silent. Mono inputs get mirrored to stereo automatically.
- **VST3:** drop the plugin on a DAW track, make sure the track is record/monitor-enabled, and confirm the VST3 reports a stereo input bus. Monitoring is always on in the plugin as well; the same “engine or test tone beats passthrough” rule applies.
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
- In a DAW, enable track monitoring and make sure the VST3 reports a stereo input bus. The plugin heartbeat (`app_.tick`) now runs every block so mode changes and key chords reflected in the UI actually drive state.
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
