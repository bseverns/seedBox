# JUCE bridge crib notes

Welcome to the laptop-friendly front door. This folder is the "plug it into a DAW" twin of the Teensy firmware:

- `SeedboxAudioProcessor` wraps the exact same `AppState`/engine stack the embedded build uses. The processor eats JUCE audio + MIDI buffers, forwards them through `hal::audio`/`MidiRouter`, and spits the results back at the host.
- `SeedboxAudioProcessorEditor` keeps the UI intentionally small: a master-seed control and a scrolling snapshot of what the firmware thinks the OLED should show. Lean is deliberate — the code is meant to be read, tweaked, and learned from. A new seed-control board sits underneath the dropdowns so you can manually wiggle the same pitch/density/probability knobs the HAL expects from hardware, making it clear how host automation maps onto the embedded genome.
- `SeedboxApplication` is a JUCEApplication that replaces the Arduino `setup`/`loop` dance. It wires an `AudioProcessorPlayer` into the native device manager so the engine runs with whatever buffer size and sample rate your machine advertises.

Design vibe: half studio logbook, half teaching lab. Nothing is hidden behind magic factory functions, and comments try to explain _why_ the plumbing looks the way it does. If you want to hack on the engines from a DAW, start here and go wild.
