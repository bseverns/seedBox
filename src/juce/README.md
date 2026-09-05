# JUCE desktop bridge

The JUCE standalone app and VST3 plugin use the same `AppState` and engine stack
as the firmware. `SeedboxAudioProcessor` handles host audio/MIDI and plugin
state; `SeedboxAudioProcessorEditor` presents the panel and detailed controls;
`SeedboxApplication` connects the standalone processor to the audio device.

## Panel and Detailed Editor

The normal editor contains a hardware-style panel and a **Detailed Editor**
toggle. The Detailed Editor starts expanded (`showAdvanced_ = true`); close it
for the panel-only layout. `SEEDBOX_LEGACY_UI=1` selects the older page-only
layout. These are editor layouts, not different physical control inventories.

The panel follows [the panel drawing](../../assets/front-panel-map.svg) and
[`PanelControls.h`](../../include/hal/PanelControls.h): four encoder/switch
assemblies and four standalone momentary buttons.

- **Encoders:** Seed/Bank, Density, Tone/Tilt, FX/Mutate. Drag to turn;
  right-click and hold to operate the integrated switch. Both paths use Board
  and the firmware's InputEvents/page policy.
- **Buttons:** Tap Tempo, Shift, Alt Seed, Live Capture. Hold Seed/Bank to reseed
  outside Storage. Hold Alt Seed to enter Storage; releasing Seed/Bank recalls
  below 450 ms or saves after at least 450 ms.
- **Live Capture:** a press captures/reseeds; continuing to hold fires panic.
  There are no separate physical Reseed or Lock buttons.
- **Timing and display:** desktop holds use elapsed Board time; native scripted
  tests retain deterministic 10 ms steps. Mouse edges are sampled immediately.
  The OLED uses the cached application snapshot, alongside audio/clock status.
- **Jack badges:** open desktop audio/MIDI helpers.

See the [panel cheat sheet](../../docs/panel_cheat_sheet.md) for the physical
control contract and [SB-04 checks](../../docs/roadmaps/SB-04-panel-contract-tests.md)
for the offscreen panel test.

## Detailed Editor pages

| Page | Additional desktop controls |
| --- | --- |
| HOME | Master seed, transport latch, BPM/clock and focus readouts. |
| SEEDS | Focus and seed edits, live-input gate division and floor. |
| ENGINE | Engine selection, granular source, and the focused engine's parameter controls. |
| PERF | Tempo and tap tempo, transport latch, clock/focus readouts. |
| SWING | Swing, quantize scale, and root. |
| SETTINGS | Clock-follow options, audio/MIDI device selection in standalone, dry fallback, and output test tone. |
| UTIL | Debug meters and panic/reset. |

These host controls supplement the physical panel. Changing a detailed-editor
parameter need not reproduce a physical knob gesture.

## Audio setup and monitoring

In standalone, open SETTINGS in the Detailed Editor and select input/output
devices and channels. The headphone badge also offers device helpers. In a DAW,
insert the VST3 on a track with input monitoring enabled and check its stereo bus
configuration.

The processor renders the effect/engine path first. **Dry Fallback If Silent**
(`forceIdlePassthrough`, default off) copies audible dry input to the output only
when the rendered path is silent. It does not override audible engine or test
output. Mono dry input is duplicated into stereo when fallback is active.
**Output Test Tone** provides an output-path check and is also off by default.

For silence, check device routing, input levels, active seeds, the test-tone
setting, and the output meter. Enable dry fallback if that is the monitoring
behavior you want; dry monitoring is not unconditionally enabled.

## Keyboard shortcuts

- Space toggles the transport latch/run flag.
- `T` taps tempo; `Y` toggles the output test tone.
- Hold `O`, `S`, or `A` to mirror Tone/Tilt, Shift, or Alt Seed switches.
- `1`–`4` select the focused seed; `E` cycles engines.
- Left/right arrows turn the last touched knob in panel-only view. In the
  Detailed Editor they nudge the a page-specific parameter.

## Persistence

VST3 state combines the APVTS parameter tree, per-seed properties, and a
serialized preset blob. Save/reload a DAW session to verify seed, engine, tempo,
and parameter restoration. Standalone `ApplicationProperties` saves the last
mode and window state. Do not assume every temporary editor interaction is
persisted; test the particular state you need to retain.

## Runtime boundaries and diagnostics

Audio blocks call `tickHostAudio()`. A processor-owned JUCE timer performs
`serviceHostMaintenance()` away from the audio callback, including deferred
control work and display updates even when the editor is closed. The heavier
host-control operations are described in the
[host/audio audit](../../docs/architecture/juce_rt_audit.md).

The panel and detailed display show warnings for dropped MIDI or oversized
host blocks. Debug builds can also show the shared host diagnostics overlay.
These counters and the audio meter help distinguish device routing problems
from callback trouble.

## Manual smoke checks

1. In standalone SETTINGS, select audio devices and verify the output test tone.
   Turn it off and check input processing; enable dry fallback to test its
   silent-engine behavior.
2. In the panel-only view, exercise each switch, rotate each encoder, and try
   Seed hold, Live Capture, and Storage save/recall.
3. In the Detailed Editor, change a focused engine parameter and confirm the
   displayed seed state responds.
4. In a DAW, save/reload a session after changing focus, engine, and tempo.
   In standalone, close/reopen and check mode and window restoration.
