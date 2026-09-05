# SB-01 — Board owns physical control input

Implemented, validated, committed, and pushed before the SB-03 reconciliation.

Physical control input follows `pin → Board → InputEvents → InputGestureRouter / ModeEventRouter → AppState`.
`AppState` no longer configures panel inputs, subscribes to digital GPIO callbacks,
measures physical holds, or polls the GPIO layer. Both firmware ticks and host
maintenance poll their injected Board and consume its input events. TeensyBoard
initializes pin modes once; Bounce attaches to those already configured inputs.
The status LED remains an application-controlled output.

The removed legacy mapping assigned Reseed to pin 2 and Lock to pin 3. Board
already uses those pins for the Seed encoder switch and Density quadrature A.
The legacy initialization replaced the GPIO configuration and its callback
could interpret encoder activity as unrelated seed actions.

Current behavior:

- A short Seed switch press follows existing mode navigation.
- Holding Seed outside Storage requests one reseed through the gesture router.
- In Storage, releasing Seed before 450 ms recalls the active preset (or
  `default`); releasing after a longer hold saves it. Board time replaces the
  old frame-count threshold. The initial press and long-press event are consumed
  without navigation, recall, or reseed.
- Live Capture retains its existing capture and panic gestures.
- Slot/global lock APIs and host controls remain available. No physical Lock
  button is assigned by the current Board contract.
  Pin 3 must not be used as an implicit Lock button.

Follow-up completed: [SB-03](SB-03-panel-reconciliation.md) preserved this Board
layout and recorded it in `PanelControls.h`; [SB-04](SB-04-panel-contract-tests.md)
added contract and independent-control checks. No separate SB-02 decision was
recorded. A future physical Lock assignment would be a contract change.

Regression coverage in `tests/test_app/test_board_input_ownership.cpp` checks
that AppState preserves existing GPIO configuration, raw pin 2/3 edges cannot
reseed or lock, a short Seed press navigates without reseeding, Density turns do
not lock seeds, and a host-maintenance Seed hold reseeds only once. The scripted
panel walkthrough saves and recalls through Board events rather than mock GPIO.

Validation recorded for SB-01:

- Native application suite: **67/67 passed**, including both ownership
  regressions and the scripted panel walkthrough.
- `pio run -e teensy40`: **passed**. Firmware was compiled, not flashed.
- `git diff --check`: passed.

On this Mac, native tests required `SDKROOT` pointing to
`/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk` and temporary `gcc` / `g++`
aliases pointing directly to Apple clang, with the CommandLineTools binaries on
PATH. This bypassed a local Rosetta `xcrun` architecture mismatch; repository
build settings were not changed.
