# SB-04 — Front-panel contract and independent-control tests

The current panel inventory in `include/hal/PanelControls.h` is now checked at
compile time, against builder-facing files, and through native/JUCE input paths.
No physical layout or gesture policy is changed by this step.

## Checks

- **Compiled contract:** all 16 panel GPIO inputs are unique; every Board button
  and encoder is mapped; integrated switches and encoder IDs agree with the
  ordered hardware table; labels and lowercase native tokens are nonempty and
  unique. Explicit enum counts size the input pipeline and native buffers.
- **Builder surfaces:** a small C++ program exports the compiled table to the
  Python checker. The checker compares labels, script tokens, pins, counts, and
  SVG control groups against the panel cheat sheet, SVG, BOM, and builder table.
  It does not parse C++ source text or silently repair mismatches.
- **Negative tests:** 16 deliberate faults verify failures for duplicate GPIO,
  crossed encoder/switch IDs, missing controls, duplicate/empty names, wrong
  counts, and stale documentation or artwork.
- **Native scripted controls:** every one of the eight switches is pressed,
  held, and released independently. Each encoder turns in both directions and
  while its own switch is held. Tests assert exact event identity and count,
  no activity on other controls, one long event per hold, and one consumption of
  each rotary delta.
- **Actual JUCE panel:** CTest instantiates the real component offscreen. It
  checks knob/standalone-button counts, each knob's adjacent visible label,
  button legends, all eight mouse-to-Board switch routes, and a Seed turn into
  the application. No audio device is opened.

## Run locally

```sh
python3 scripts/check_panel_contract.py
python3 -m unittest discover -s scripts/tests -p test_panel_contract.py
pio test -e native --filter test_app

cmake -S . -B build/juce -DBUILD_TESTING=ON
cmake --build build/juce --target seedbox_panel_contract_test
ctest --test-dir build/juce -R '^panel_contract_juce$' --output-on-failure
```

The Python checker needs a C++17 compiler (`c++` by default, or set `CXX`). Its
compiler outputs and mutation fixtures live in temporary directories. On a
headless Linux host, the JUCE GUI test needs a display session such as Xvfb;
CI runs that test in the existing macOS build job.

## CI integration

The fast `front-panel-contract` job checks docs/artwork and exercises the
negative tests on every pull request and push to main. The normal native test
matrix runs the scripted controls; both native and Teensy builds evaluate the
static assertions. The macOS job builds and runs `panel_contract_juce` before
packaging/signing artifacts.

Local validation:

- Compiled contract / builder-surface checker: passed.
- Negative suite: 3 test groups passed, covering 16 injected faults.
- Native application suite: **70/70 passed**.
- Teensy firmware build: passed; no device was flashed.
- JUCE offscreen CTest: **1/1 passed**.
- `git diff --check`: passed.

The CI workflow has been updated but has not been run on GitHub by this change.
These checks cover the
repository contract and software routing, not electrical continuity, switch
bounce on a real harness, or a hardware-in-the-loop bench test.
