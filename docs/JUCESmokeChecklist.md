# JUCE Manual Smoke Checklist

This page is the human-side companion to the repo's JUCE build verification.
CI proves that the desktop targets build and stage artifacts. This checklist is
for the runtime questions CI cannot answer by itself.

Use it after:

- a local JUCE build
- a CI artifact download you plan to demo
- changes to desktop audio, UI, transport, persistence, or host integration

## What this checklist is for

- confirming the standalone app actually opens and passes audio
- confirming the VST3 actually loads in a host and exposes the expected behavior
- recording a dated manual smoke result instead of relying on memory

## Runtime smoke matrix

| Surface | What to verify | Where the repo documents it | Receipt to keep |
| --- | --- | --- | --- |
| Standalone launch | App opens, editor appears, no immediate crash | [`docs/juce_build.md`](juce_build.md), [`src/juce/README.md`](../src/juce/README.md) | OS + build note |
| Audio device selection | Inputs/outputs can be selected in SETTINGS | [`src/juce/README.md`](../src/juce/README.md) | Screenshot or short note |
| Monitoring / passthrough | Dry input passes through when engines/test tone are silent | [`src/juce/README.md`](../src/juce/README.md) | Audio note |
| Test tone priority | Test tone audibly overrides dry passthrough | [`src/juce/README.md`](../src/juce/README.md) | Audio note |
| UI state / panel view | HOME/ENGINE/PERF/etc. update as expected | [`src/juce/README.md`](../src/juce/README.md) | Screenshot or short note |
| Keyboard shortcuts | Space, `T`, `O`, `1`-`4`, `E`, arrows behave as documented | [`src/juce/README.md`](../src/juce/README.md) | Short pass/fail note |
| VST3 host load | Plugin scans and opens in a DAW/host | [`docs/juce_build.md`](juce_build.md) | Host name/version note |
| VST3 monitoring | Input monitoring works with a stereo input bus | [`src/juce/README.md`](../src/juce/README.md) | Host note |
| Persistence | Standalone restores page/window; VST3 restores session/preset state | [`src/juce/README.md`](../src/juce/README.md) | Short reopen/reload note |
| Transport / tempo | Tempo and transport controls visibly/audibly update state | [`src/juce/README.md`](../src/juce/README.md) | Short note or screenshot |

## Fast smoke pass

If you only have five minutes, do this:

1. Open the standalone app.
2. Select input/output devices.
3. Confirm dry monitoring and test tone behavior.
4. Change seed focus, tempo, and one engine control.
5. Close and reopen the app to confirm basic persistence.
6. Load the VST3 in one host and confirm it opens, passes input, and recalls state.

## Copy/paste receipt template

```md
### JUCE smoke receipt — YYYY-MM-DD

- Git revision:
- Build command:
- Artifact path:
- OS:
- Host / DAW:
- Tester:

#### Standalone

- Launch:
- Audio device selection:
- Dry monitoring:
- Test tone priority:
- UI / panel behavior:
- Keyboard shortcuts:
- Persistence after reopen:

#### VST3

- Host scan/load:
- Input monitoring:
- Tempo / transport behavior:
- Engine edit behavior:
- Persistence after session/preset reload:

#### Outcome

- Status: pass / pass-with-caveats / fail
- Caveats:
- Screenshots / logs:
```

## Manual smoke log

Add one row when you do a real runtime pass.

| Date | OS | Host / DAW | Artifact | Result | Receipt location |
| --- | --- | --- | --- | --- | --- |
| 2026-03-22 | macOS 26 / Darwin 25.3.0 arm64 | none; standalone launch only | `build/juce` app + VST3 artifacts | pass-with-caveats | inline receipt below |
| 2026-03-22 | macOS 26 / Darwin 25.3.0 arm64 | REAPER | `build/juce-arm64-fx` VST3 artifact installed to user VST3 folder | pass-with-caveats | inline receipt below |
| 2026-03-22 | macOS 26 / Darwin 25.3.0 arm64 | local JUCE headless probe + system output | `build/juce-arm64-fx` VST3 artifact | pass | inline receipt below |

## Recorded receipt — 2026-03-22

### JUCE smoke receipt — 2026-03-22

- Git revision: `13327f5`
- Build command: `cmake --build build/juce --target SeedboxApp SeedboxVST3_VST3`
- Artifact path:
  - `build/juce/SeedboxApp_artefacts/SeedBox Standalone.app`
  - `build/juce/SeedboxVST3_artefacts/VST3/SeedBox.vst3`
- OS: `Darwin 25.3.0 arm64`
- Host / DAW: none
- Tester: Codex local pass

#### Standalone

- Launch: pass; launched via `open -n ...` and remained alive long enough to
  observe a running process after 5 seconds
- Audio device selection: not verified
- Dry monitoring: not verified
- Test tone priority: not verified
- UI / panel behavior: not verified; AppleScript/UI inspection hit macOS
  Accessibility restrictions
- Keyboard shortcuts: not verified
- Persistence after reopen: not verified

#### VST3

- Host scan/load: not verified
- Input monitoring: not verified
- Tempo / transport behavior: not verified
- Engine edit behavior: not verified
- Persistence after session/preset reload: not verified

#### Outcome

- Status: pass-with-caveats
- Caveats:
  - this pass verified rebuild + bundle integrity + standalone launch, not full
    runtime audio behavior
  - the current `build/juce` cache produced thin `x86_64` app and VST3 binaries,
    not universal or arm64 artifacts
  - no DAW host receipt was captured in this pass
- Screenshots / logs: terminal/session transcript only; no repo-local screenshot
  or audio capture was produced

### JUCE host receipt — 2026-03-22

- Git revision: `13327f5`
- Build command:
  - `cmake -S . -B build/juce-arm64-fx -DCMAKE_OSX_ARCHITECTURES=arm64 -DSEEDBOX_SIM=ON -DSEEDBOX_HW=OFF -DSEEDBOX_JUCE=ON -DQUIET_MODE=OFF -DSEEDBOX_VERSION=13327f5`
  - `cmake --build build/juce-arm64-fx --target SeedboxVST3_VST3`
- Installed artifact path:
  - source: `build/juce-arm64-fx/SeedboxVST3_artefacts/VST3/SeedBox.vst3`
  - installed: `~/Library/Audio/Plug-Ins/VST3/SeedBox.vst3`
- OS: `Darwin 25.3.0 arm64`
- Host / DAW: `REAPER.app`
- Tester: Codex local pass

#### Host-side observations

- Fresh plugin architecture: pass; rebuilt as thin `arm64`
- Fresh plugin install: pass; user VST3 bundle replaced and ad-hoc signed
- Host launch with plugin present: pass; REAPER launched successfully after the
  fresh install
- Host cache update: pass; `~/Library/Application Support/REAPER/reaper-vstplugins_arm64.ini`
  rewrote its `SeedBox.vst3` entry during the launch pass
- Cached plugin entry after launch:
  - `SeedBox.vst3=00A6F8D97ABADC01,1563380293{ABCDEF019182FAEB4253535342534258,SeedBox (BSSoundStudio)`

#### Not verified

- Plugin instantiated on a track
- Audio passed through the plugin in-host
- Editor UI opened inside REAPER
- Transport, monitoring, tempo, or persistence behavior inside REAPER

#### Outcome

- Status: pass-with-caveats
- Caveats:
  - this is a real host-start/cache receipt, not a full instantiated-plugin
    playback receipt
  - REAPER CLI did not expose a clean non-interactive plugin-load path here
  - UI scripting was not available because macOS Accessibility access was not
    available to automation
- Screenshots / logs: terminal/session transcript only; REAPER cache file mtime
  and plugin entry changed during the pass

### JUCE VST3 audio receipt — 2026-03-22

- Git revision: `13327f5`
- Build command:
  - `cmake -S . -B build/juce-arm64-fx -DCMAKE_OSX_ARCHITECTURES=arm64 -DSEEDBOX_SIM=ON -DSEEDBOX_HW=OFF -DSEEDBOX_JUCE=ON -DQUIET_MODE=OFF -DSEEDBOX_VERSION=13327f5`
  - `cmake --build build/juce-arm64-fx --target SeedboxVST3_VST3 seedbox_juce_vst3_probe`
- Artifact path:
  - `build/juce-arm64-fx/SeedboxVST3_artefacts/VST3/SeedBox.vst3`
- Probe output:
  - `build/juce_probe/seedbox_vst3_probe.wav`
- OS: `Darwin 25.3.0 arm64`
- Host / DAW: local JUCE headless probe (`seedbox_juce_vst3_probe`)
- Tester: Codex local pass

#### Probe observations

- VST3 bundle scan: pass
- VST3 instantiation in a host process: pass
- Parameter control: pass; probe enabled the plugin's `Test Tone` parameter
- Offline render: pass; 3-second stereo WAV emitted from the built VST3 bundle
- Render stats:
  - peak: `0.0490013`
  - RMS: `0.0346585`
- Monitor playback: pass; `afplay build/juce_probe/seedbox_vst3_probe.wav`
  completed successfully outside the sandbox after the sandboxed attempt failed
  to open an audio queue

#### What this proves

- The built macOS VST3 artifact can be scanned and instantiated outside the DAW
  cache path.
- The instantiated artifact can process audio and emit a clearly non-silent
  signal.
- The rendered output can be played through the machine's normal audio output.

#### What this still does not prove

- In-host editor behavior inside a DAW
- Live input monitoring through a DAW track
- Session reload/persistence inside a DAW
- Host transport sync behavior

#### Outcome

- Status: pass
- Caveats:
  - this is a strong artifact-audio receipt, but it is still not the same thing
    as a full DAW in-track monitoring pass
  - the audible signal here came from the plugin's built-in test tone, not live
    external input
- Screenshots / logs: terminal/session transcript plus
  `build/juce_probe/seedbox_vst3_probe.wav`

## Current boundary

If this checklist is green, you have a meaningful runtime receipt for that OS
and host. You still do not have a blanket guarantee for every host, interface,
or desktop environment.
