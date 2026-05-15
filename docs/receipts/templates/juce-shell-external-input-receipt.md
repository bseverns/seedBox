# JUCE Shell External Input Receipt Plan

Use this template when SeedBox has been tested through the actual JUCE plugin
or standalone callback path with external input. This is not a native
`seedbox_native_input_probe` receipt. The point is to prove the host/device
shell, callback staging, diagnostics publication, and captured output all work
together.

Copy a filled-in receipt to `docs/receipts/YYYY-MM-DD-juce-shell-external-input.md`.
Do not commit generated WAV captures, DAW project caches, local absolute paths,
or host-specific receipt artifacts unless they are deliberately small text
summaries.

## Receipt Metadata

- Date:
- Git revision:
- Tester:
- Receipt status: pass / pass-with-caveats / fail
- Test duration:

## Host Or App Tested

- Path tested: VST3 plugin / standalone app
- Host app:
  - Name:
  - Version:
  - Audio driver/backend:
  - Project/session sample rate:
  - Project/session block size:
- Standalone app, if used:
  - App bundle or executable:
  - Audio device:
  - Input channel pair:
  - Output channel pair:

## OS And Build

- OS:
- CPU architecture:
- Build command:
- Plugin or standalone path:
- Code signing / quarantine state, if relevant:

## Audio Configuration

- Sample rate:
- Block size:
- Input channel count:
- Output channel count:
- Host transport state: stopped / playing / looping
- Host tempo:
- SeedBox transport settings:
  - Follow host transport:
  - Follow external clock:
  - Clock source external:
  - Transport latch:

## Input Source

- Source type: file playback / live interface input / loopback / test instrument
- Source description:
- Source format, if file-based:
  - Channels:
  - Sample rate:
  - Bit depth:
  - Duration:
  - SHA-256, if practical:
- Routing into SeedBox:
- Input gain staging:

## Output Capture Method

- Capture method: DAW bounce / record-to-track / loopback recorder / standalone device capture
- Capture path:
- Capture format:
  - Channels:
  - Sample rate:
  - Bit depth:
  - Duration:
- Output gain staging:
- Output SHA-256, if practical:

## Host Diagnostics Snapshot

Record a status snapshot after the pass. At minimum, capture the
`hostDiagnostics` object and the focused seed info.

```json
{
  "hostDiagnostics": {
    "midiDroppedCount": null,
    "oversizeBlockDropCount": null,
    "lastOversizeBlockFrames": null,
    "preparedScratchFrames": null
  },
  "focusSeed": {
    "present": null,
    "index": null,
    "engineId": null,
    "engineName": null
  }
}
```

Expected `hostDiagnostics` interpretation:

- `midiDroppedCount`: should stay `0` unless the test intentionally floods MIDI.
- `oversizeBlockDropCount`: should stay `0`; any nonzero value means the host gave the plugin a block larger than prepared scratch storage.
- `lastOversizeBlockFrames`: should stay `0` unless `oversizeBlockDropCount` is nonzero.
- `preparedScratchFrames`: should be at least the configured block size.

## Expected Artifacts

Keep artifacts out of git unless they are intentionally small text evidence.

- Command/build log:
- Host/app screenshot:
- Routing screenshot:
- Status JSON or copied status block:
- Output capture WAV path:
- Output capture hash:
- Optional input source hash:
- Optional short analysis summary:
  - output peak:
  - output RMS:
  - input/output audible difference:

## Pass/Fail Checklist

- [ ] Built the exact plugin or standalone binary being tested.
- [ ] Loaded the plugin in the host or launched the standalone app successfully.
- [ ] Configured the requested sample rate and block size.
- [ ] Routed real external input into SeedBox, not only an internal oscillator.
- [ ] Confirmed SeedBox received input by audible monitoring or meters.
- [ ] Captured output from the JUCE plugin or standalone callback path.
- [ ] Output capture exists and is non-silent.
- [ ] Output is not just unmodified dry passthrough unless the test explicitly requested idle passthrough.
- [ ] Captured or copied final status JSON.
- [ ] `hostDiagnostics` is present in the status snapshot.
- [ ] `hostDiagnostics.oversizeBlockDropCount == 0`.
- [ ] `hostDiagnostics.preparedScratchFrames >= block size`.
- [ ] Documented any MIDI drops, host transport quirks, device underruns, or routing surprises.

## Caveats And Distinctions

- `seedbox_native_input_probe` proves the shared native app/render stack can
  process an external WAV deterministically. It does not instantiate the JUCE
  plugin, use `SeedboxAudioProcessor::processBlock`, run a DAW/device callback,
  or publish JUCE shell diagnostics.
- A JUCE shell receipt must prove the actual callback path:
  - VST3: host -> `SeedboxAudioProcessor::processBlock` -> `hal::audio::renderHostBuffer`
  - Standalone: device callback -> `JuceHost::audioDeviceIOCallbackWithContext` -> `hal::audio::renderHostBuffer`
- `hostDiagnostics` has different value here than in native probe receipts.
  In this receipt it can catch JUCE-shell MIDI queue drops and oversize host
  audio blocks.
- A DAW offline bounce is useful only if it still exercises the plugin callback
  path. Note whether the bounce was offline or real-time.
- A standalone receipt proves the device callback path, not VST3 host behavior.
  A VST3 receipt proves plugin host integration, not physical hardware input
  unless the input source is actually routed from an interface.

## Bottom Line

- Result:
- Evidence summary:
- Follow-up needed:
