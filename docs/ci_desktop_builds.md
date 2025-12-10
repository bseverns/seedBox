# Desktop CI: loud, literal, and future-proof

This repo already runs PlatformIO tests; the new desktop CI keeps the JUCE side
honest. Think of it as a backstage checklist taped to the amp: clear about what
we build, which arch we aim at, and how to debug it if something pops.

## What each workflow does

- **macOS universal (x86_64 + arm64).** Builds both the VST3 bundle (explicitly
  drives the format-specific `SeedboxVST3_VST3` target so the actual plugin
  binary exists) and the standalone app from the same CMake tree with
  `CMAKE_OSX_ARCHITECTURES="x86_64;arm64"` on the macOS 14 runner. We stick with
  the macOS 14 SDK because macOS 15 obsoleted the CoreGraphics
  `CGWindowListCreateImage` API JUCE still leans on. The deployment target
  remains 11.0 so both Intel and Apple Silicon hosts stay happy, and we upload
  the bundled artifacts so testers can drag-drop without a local toolchain. CI
  now hunts for the VST3 bundle with `find` and then hunts for the actual
  binary inside the bundle before running `lipo`, so we catch staging-layout
  changes instead of bombing out with a missing file—feel free to use the same
  trick locally when JUCE shuffles its output folders.
- **Linux host dependency sweep.** Installs JUCE’s usual suspects
  (`libx11-dev`, `libgtk-3-dev`, `libwebkit2gtk-4.1-dev`, `pkg-config`, etc.),
  configures the JUCE targets, and builds both the plugin and the app to ensure
  the headers and pkg-config hints stay lined up. We feed the
  `pkg-config --libs libcurl` output into the linker flags so JUCE’s
  `WebInputStream` symbols resolve cleanly instead of dying at link time. We
  also export a
  `PKG_CONFIG_PATH` that points at the default `/usr/lib/x86_64-linux-gnu` and
  `/usr/share/pkgconfig` roots and run `pkg-config --cflags --libs gtk+-3.0`
  explicitly so broken GTK discovery fails fast instead of puzzling you with a
  missing `gtk/gtk.h` 15 minutes into a build.
- **Windows host dependency sweep.** Leans on the Visual Studio toolchain that
  GitHub Actions ships with (explicitly boots the MSVC developer command
  prompt), layers Ninja on top, and drives the same CMake targets so we know
  JUCE + Windows stay in lockstep with the rest of the project.

## Tips for running these steps locally

- **macOS universal builds**: pass `-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"`
  when configuring and remember to build the format-specific target
  (`cmake --build build/juce --target SeedboxVST3_VST3`) so the bundle actually
  contains a binary. `lipo -info` on the VST3 binary inside the bundle should
  list both architectures. On CI we use the macOS 14 runner so JUCE can still
  rely on the CoreGraphics window snapshot APIs that vanished in the macOS 15
  SDK.
- **Linux builds**: if you hit missing X11/WebKit dev packages, mirror the
  `apt-get` list from the workflow (note the `libwebkit2gtk-4.1-dev` rename and
  the explicit `pkg-config` install on Ubuntu 24.04) and rerun CMake
  out-of-tree (`cmake -S . -B build/juce -G Ninja ...`). If GTK/WebKit still
  refuse to show up, echo the CI trick locally: set
  `PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig`
  and run `pkg-config --cflags --libs gtk+-3.0` plus
  `pkg-config --cflags --libs webkit2gtk-4.1`—you should see include paths like
  `-I/usr/include/gtk-3.0` and `-I/usr/include/webkitgtk-4.1`. No output?
  Install the packages or add the matching pkg-config search path before
  blaming JUCE. If CMake still pretends GTK/WebKit do not exist, bolt the
  pkg-config output straight into your configure line with
  `-DCMAKE_CXX_FLAGS_INIT="$(pkg-config --cflags gtk+-3.0 webkit2gtk-4.1)"` and
  `-DCMAKE_SHARED_LINKER_FLAGS_INIT="$(pkg-config --libs gtk+-3.0 webkit2gtk-4.1) $(pkg-config --libs libcurl)"`—
  it’s loud but guarantees JUCE compiles against the headers we just installed
  and links against libcurl instead of exploding with undefined `curl_*`
  references at the final link step.
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
