# UI Render Zine

This corner of the firmware is the bridge between the synthesizer brain and the 128×64 slab of glass bolted to the front panel. Think of it as a zine issue dedicated to one question: *what does the box want to show right now, and how do we serialize that into pixels or ASCII without lying about the state machine?*

## Signal flow tour

1. **`AppState::captureDisplaySnapshot`** (`src/app/AppState.cpp`) is the storyteller. Every time the UI wants to refresh, the app pumps a `DisplaySnapshot` plus a `UiState` through this function. It decides which mode banner to flash (`PRF`, `EDT`, or `SYS`), whether we're clocking internally or being puppeted by MIDI, and which engine name + helper hints should be scrawled across the page.【F:src/app/AppState.cpp†L1608-L1670】
2. **`UiState`** (`include/app/UiState.h`) is the minimal cheat sheet. It's intentionally tiny—just enough for the renderer to rebuild the header/status rows without copying business logic or running audio-side code.【F:include/app/UiState.h†L4-L33】
3. **`ComposeTextFrame`** (`src/ui/TextFrame.cpp`) is the typesetter. It slams together the status banner, the snapshot strings, and any contextual hints into a deterministic block of 16-character lines. Quiet-mode banners and engine glyphs are all baked here so downstream renderers don't have to second-guess anything.【F:src/ui/TextFrame.cpp†L13-L118】
4. **Renderers**: pick your poison.
   * `ui::OledView` pushes frames to the hardware display, batching updates until the flush timer says it's safe to draw.【F:src/ui/OledView.cpp†L16-L94】
   * `ui::AsciiOledView` mirrors the exact same text layout into an in-memory string vector, optionally logging frames to stdout. That's how tests (and humans without solder fumes) audit the UI.【F:src/ui/AsciiOledView.cpp†L8-L27】

The punchline: hardware and simulation share the same text frame, so you never have to reconcile "real" and "test" renders. If the ASCII view flips, the OLED will too.

## Modes & micro-state narrative

`UiState` is basically a micro state machine glued onto the bigger `AppState`. Here's how the top banner earns each character:

* **Mode tag**: `PRF`, `EDT`, or `SYS`, chosen when `captureDisplaySnapshot` sees swing-edit mode, seed locks, or debug meters toggled.【F:src/app/AppState.cpp†L1635-L1650】
* **Clock glyph**: `I` for internal, `E` for external—driven straight from the scheduler's clock dominance flag.【F:src/app/AppState.cpp†L1645-L1646】【F:src/ui/TextFrame.cpp†L32-L47】
* **Tempo & swing**: clamped and rounded so the banner always fits in sixteen columns.【F:src/ui/TextFrame.cpp†L49-L71】
* **Engine tag**: three alphanumeric characters carved out of the focused engine name and uppercased—`GRA` for Granulator, etc.【F:src/ui/TextFrame.cpp†L33-L47】
* **Lock flag**: `L` lights up if either the global seed lock or the focused seed lock is active.【F:src/app/AppState.cpp†L1629-L1644】【F:src/ui/TextFrame.cpp†L63-L70】

The remaining lines in the frame are verbatim strings written into the `DisplaySnapshot` (title, status, metrics, nuance) followed by the page hints. If quiet mode is built in, line two is hijacked with a warning banner.【F:src/ui/TextFrame.cpp†L84-L108】

## Capturing frames in tests (or REPLs)

Unit tests lean on the ASCII renderer so they can diff frames without toggling hardware:

```c++
AppState app;
app.initSim();               // hydrate the engines + UI cache
AppState::DisplaySnapshot s{};
UiState ui{};
app.captureDisplaySnapshot(s, ui);
ui::AsciiOledView view(false); // false = keep stdout quiet during tests
view.present(s, ui);
std::string latest = view.latest();
```

The regression tests in [`tests/test_app/test_ui_frames.cpp`](../../tests/test_app/test_ui_frames.cpp) call that helper to assert that boot frames mention `SeedBox` and that engine swaps change the rendered banner. Because `AsciiOledView` deduplicates consecutive frames, you can simulate a performance session and only stash the deltas.【F:tests/test_app/test_ui_frames.cpp†L12-L50】【F:src/ui/AsciiOledView.cpp†L18-L24】

Want to peek at live frames while running firmware on a host build? Flip the constructor flag to `true` and every unique frame will print to stdout prefixed with `[oled:N]`. It feels low-tech because it is—and that's why it works in CI just as well as on your laptop.

## Suggested riffs

* When you bolt on new UI states, sketch the story first: what line in the frame should change, and can the state machine narrate it without conditional soup in the renderer?
* Tests don't need to know about pixels. Keep asserting on strings and let `AsciiOledView` prove that the math matches the hardware glyphs.
* If you're prototyping a new layout, edit `ComposeTextFrame` and run the ASCII renderer in a loop. It will show you exactly what the OLED would say, without lifting the soldering iron.

Now go draw something true.
