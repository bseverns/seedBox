# Desktop CI: loud, literal, and future-proof

This repo already runs PlatformIO tests; the new desktop CI keeps the JUCE side
honest. Think of it as a backstage checklist taped to the amp: clear about what
we build, which arch we aim at, and how to debug it if something pops.

## What each workflow does

- **macOS universal (x86_64 + arm64).** Builds both the VST3 bundle and the
  standalone app from the same CMake tree with
  `CMAKE_OSX_ARCHITECTURES="x86_64;arm64"` on the macOS 15 Intel runner. We pin
  the deployment target to 11.0 so both Intel and Apple Silicon hosts stay
  happy, and we upload the bundled artifacts so testers can drag-drop without a
  local toolchain.
- **Linux host dependency sweep.** Installs JUCE’s usual suspects (`libx11-dev`,
  `libwebkit2gtk-4.1-dev`, etc.), configures the JUCE targets, and builds both
  the plugin and the app to ensure the headers and pkg-config hints stay lined
  up.
- **Windows host dependency sweep.** Leans on the Visual Studio toolchain that
  GitHub Actions ships with (explicitly boots the MSVC developer command
  prompt), layers Ninja on top, and drives the same CMake targets so we know
  JUCE + Windows stay in lockstep with the rest of the project.

## Tips for running these steps locally

- **macOS universal builds**: pass `-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"`
  when configuring. `lipo -info` on the VST3 binary inside the bundle should
  list both architectures. On CI we use the Intel macOS 15 runner as a known
  quantity because macOS-13 images are being retired.
- **Linux builds**: if you hit missing X11/WebKit dev packages, mirror the
  `apt-get` list from the workflow (note the `libwebkit2gtk-4.1-dev` rename on
  Ubuntu 24.04) and rerun CMake out-of-tree (`cmake -S . -B build/juce -G Ninja
  ...`).
- **Windows builds**: open a "x64 Native Tools" shell before running CMake (or
  run `vcvarsall.bat x64` in PowerShell) to inherit the MSVC environment, then
  reuse the same flags as the workflow.

## Why bother

Desktop builds are our glue between the firmware world and DAWs. Keeping these
workflows green means:

- VST drops are always reproducible from CI instead of mythical local setups.
- Dependency churn on Linux/Windows gets caught early instead of during a live
  demo.
- The macOS universal artifact doubles as a teaching bundle: unzip, drop into
  a host, and you’re hearing the latest commit with zero extra steps.
