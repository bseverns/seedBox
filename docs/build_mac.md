# macOS build targets (VST3 + standalone)

These commands always produce both artifacts with an actual VST3 binary inside
the bundle.

## Configure

```bash
cmake -S . -B build/juce \
  -DSEEDBOX_SIM=ON \
  -DQUIET_MODE=OFF \
  -DSEEDBOX_VERSION="$(git rev-parse --short HEAD)"
```

## Build

```bash
cmake --build build/juce --target SeedboxApp SeedboxVST3_VST3
```

## Verify the VST3 executable exists

```bash
ls -l build/juce/SeedboxVST3_artefacts/<CONFIG>/VST3/SeedBox.vst3/Contents/MacOS/
```

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
