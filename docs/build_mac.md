# macOS build targets (VST3 + standalone)

These commands produce both artifacts with an actual VST3 binary inside the
bundle. For a release-ready macOS build, configure universal slices explicitly.

For current status and support boundaries, see:

- [Current state](CurrentState.md)
- [Stability and support](StabilityAndSupport.md)
- [JUCE build guide](juce_build.md)
- [JUCE manual smoke checklist](JUCESmokeChecklist.md)

## Configure

```bash
cmake -S . -B build/juce \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DSEEDBOX_SIM=ON \
  -DQUIET_MODE=OFF \
  -DSEEDBOX_VERSION="$(git rev-parse --short HEAD)"
```

On Apple Silicon, swap `x86_64;arm64` for `arm64` if you only need a native
local Tahoe build.

## Build

```bash
cmake --build build/juce --target SeedboxApp SeedboxVST3_VST3
```

## Verify the VST3 executable exists

```bash
ls -l build/juce/SeedboxVST3_artefacts/<CONFIG>/VST3/SeedBox.vst3/Contents/MacOS/
```

## Quick verification checklist

1. Configure with the intended architecture set.
2. Build both `SeedboxApp` and `SeedboxVST3_VST3`.
3. Confirm the VST3 bundle contains a real executable under `Contents/MacOS`.
4. If you built a universal binary, run `lipo -info` on the executable.
5. Optional but useful: build `seedbox_juce_vst3_probe`, render
   `build/juce_probe/seedbox_vst3_probe.wav`, and play it with `afplay` for a
   fast artifact-audio receipt.
6. Treat this as a build/artifact proof plus a lightweight audio receipt, then
   do manual host playback tests if you need runtime confidence on a specific
   machine.

## Optional: local artifact-audio probe

```bash
cmake --build build/juce --target seedbox_juce_vst3_probe
./build/juce/seedbox_juce_vst3_probe \
  --plugin build/juce/SeedboxVST3_artefacts/<CONFIG>/VST3/SeedBox.vst3 \
  --output build/juce_probe/seedbox_vst3_probe.wav
afplay build/juce_probe/seedbox_vst3_probe.wav
```

This does not replace a real DAW smoke pass, but it does prove that the built
VST3 bundle can be instantiated, emit a non-silent signal, and route that WAV
through the Mac's normal output path.

## Optional: copy to the user VST3 folder

Use the helper script to install the built bundle into
`~/Library/Audio/Plug-Ins/VST3`.

```bash
scripts/install_vst3_user.sh
```

You can also point it at a specific build output:

```bash
scripts/install_vst3_user.sh build/juce/SeedboxVST3_artefacts/Release/VST3/SeedBox.vst3
```
