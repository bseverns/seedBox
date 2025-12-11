# JUCE parameter choices and combo boxes

> Keep the knobs honest: combo boxes only behave if the underlying parameter spells out every option.

We discovered that `ComboBoxParameterAttachment` quietly ignores plain integer parameters. The fix was to convert any combo-driven parameter to `AudioParameterChoice` and give JUCE the exact labels it expects. This note documents the mapping so future tweaks stay in sync.

## Choice inventories

- **Focus seed** (`focusSeed`): `Seed 1`, `Seed 2`, `Seed 3`, `Seed 4`
- **Seed engine** (`seedEngine`): `Default`, `Grain`, `Chord`, `Drum`, `FM`, `Additive`, `Resonator`, `Noise`
- **Quantize scale** (`quantizeScale`): `Chromatic`, `Major`, `Minor`, `Dorian`, `Lydian`
- **Quantize root** (`quantizeRoot`): `C`, `C#`, `D`, `D#`, `E`, `F`, `F#`, `G`, `G#`, `A`, `A#`, `B`

## Why it matters

- Hosts write automation and presets in zero-based indices; the combo boxes expose one-based IDs for humans. The `AudioParameterChoice` bridge keeps those worlds aligned.
- If you add or reorder options in the editor, mirror the change in the parameter layout or the UI will silently desync. Punk rock is cool; silent failures are not.

When in doubt, check both `SeedboxAudioProcessorEditor` and `SeedboxAudioProcessor::createParameterLayout()` together before shipping a build.
