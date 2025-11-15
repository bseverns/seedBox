# ASCII OLED gallery — banner storytime

TextFrame already scripts the header drama; this cheat sheet just snaps the key
moments so your workshop deck has receipts. Every frame below comes straight out
of `ui::AsciiOledView`, so whatever you rehearse here is exactly what the OLED
will whisper on hardware.【F:src/ui/AsciiOledView.cpp†L8-L24】 Run the new
`test_ui_gallery_snapshots` Unity case to re-print the gallery whenever the UI
state machine evolves:

```bash
pio test -e native --filter test_app
```

That test boots fresh `AppState` instances, pokes the relevant knobs, and dumps
frames with punk-rock bracket labels (`[global-lock]`, `[swing-edit]`,
`[external-clock]`) so you can copy/paste straight into slides or lab notes.

> ⚠️ If your network blocks PlatformIO's package mirror, clone the strings below
> instead of re-running the test. They line up with the firmware defaults baked
> into `AppState::captureDisplaySnapshot` and `ui::ComposeTextFrame`.

## Global seed lock engaged

Flip the global lock and the banner flips to edit mode, paints the `L` flag, and
swaps the hint rail to the "unlock everything" mantra.【F:src/app/AppState.cpp†L1729-L1767】【F:src/ui/TextFrame.cpp†L29-L119】

```text
[global-lock]
EDTI120SW00SAML 
QUIET MODE ARMED
SeedBox EDB0B1
#00SMP+9.0st-
D1.11P0.85N000
Mu0.11sM00R4CJ13
Pg seeds locked
Pg+Md: unlock al
```

* `EDT` + `I` tells the class we're editing while clocking internally.
* The `L` badge sits in column 15, so screen grabs show the padlock instantly.
* Page hints collapse to the lock workflow—note the 16-character trim on
  `Pg+Md: unlock all`.

## Swing editor cameo

Enter swing edit, nudge the groove to 17%, and the header stays in edit mode
while the swing digits pulse to `SW17`. Hints pivot to the tap-to-exit story so
students remember how to bail.【F:src/app/AppState.cpp†L1718-L1784】【F:src/ui/TextFrame.cpp†L49-L87】

```text
[swing-edit]
EDTI120SW17SAM- 
QUIET MODE ARMED
SeedBox EDB0B1
#00SMP+9.0st-
D1.11P0.85N000
Mu0.11sM00R4CJ13
Tap: exit swing
Seed:5% Den:1%
```

* The lock glyph drops back to `-` because only swing edit is active.
* `SW17` proves the typesetter clamps swing values to two digits.
* The hint rail becomes a live swing cheat sheet (`Tap` to bail, `Seed` encoder
  math for depth vs density).

## External clock takeover

Trigger external transport start and the banner earns its `E`, staying in
performance mode while the hints drop back to the default Seed page riffs. This
is the "MIDI clock owns us now" snapshot for any sync lesson.【F:src/app/AppState.cpp†L1125-L1188】【F:src/ui/TextFrame.cpp†L32-L83】

```text
[external-clock]
PRFE120SW00SAM- 
QUIET MODE ARMED
SeedBox EDB0B1
#00SMP+9.0st-
D1.11P0.85N000
Mu0.11sM00R4CJ13
Tone S:src ALT:d
S+A:p Tap:LFSR
```

* `PRF` + `E` = performance page under external clock dominance.
* Same seed metrics and nuance rows—locks and swing modes don’t touch those
  lines, so you get apples-to-apples comparisons during demos.
* Default hints (`Tone…`, `S+A…`) return because nothing special is latched.

## Regenerating or remixing the gallery

Want alternate seeds, a different engine, or a live performance script? Copy the
pattern in `test_ui_gallery_snapshots.cpp`: set up a fresh `AppState`, mutate the
state you care about (`seedPageToggleGlobalLock`, `enterSwingMode`,
`onExternalTransportStart`), and let `AsciiOledView` snapshot the frame before
you screenshot anything.【F:tests/test_app/test_ui_gallery_snapshots.cpp†L1-L96】

Keep the slides loud, the code honest, and the intent obvious.
