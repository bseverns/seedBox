# Front panel cheat sheet — riffs, not secrets

The SeedBox surface is tiny on purpose, which means every gesture is doing
double-duty. Treat this page like the annotated set list: a quick reminder of
what the buttons and encoders pull off mid-jam, and why you'd reach for them.

## Control inventory

Firmware, native scripts, and JUCE use the inventory in
[`PanelControls.h`](../include/hal/PanelControls.h).

| Panel legend | Native token | GPIO (A / B / switch for encoders) |
| --- | --- | --- |
| Seed/Bank | `seed` | 0 / 1 / 2 |
| Density | `density` | 3 / 4 / 5 |
| Tone/Tilt | `tone` | 24 / 26 / 27 |
| FX/Mutate | `fx` | 6 / 9 / 30 |
| Tap Tempo | `tap` | 31 |
| Shift | `shift` | 32 |
| Alt Seed | `alt` | 33 |
| Live Capture | `capture` | 34 |

The first four controls each include a push switch. The last four are standalone
momentary buttons. All switches are active-low. There is no separate Reseed or
Lock button; lock state remains available through application/host APIs.

## Core gestures

| Move | What happens |
| --- | --- |
| Seed/Bank, Density, Tone/Tilt, FX/Mutate switch (press from HOME) | Enter SEEDS, ENGINE, PERF, UTIL respectively. |
| Seed/Bank switch (hold outside Storage) | Request one new master seed; locked seed content stays fixed. |
| Tap Tempo (double press) | Enter SETTINGS; double press again to return HOME. |
| Shift (hold) | Return HOME from the supported performance modes. |
| Alt Seed (hold) | Open Storage. |
| Seed/Bank switch (release in Storage) | Under 450 ms: recall; at least 450 ms: save the active preset. |
| Live Capture (press) | Capture/reseed using live input. |
| Live Capture (hold) | Panic after the initial capture action. |

Native examples: `btn capture down`, `wait 40ms`, `btn capture up`;
`enc density -2` turns Density two steps. Run scripts through Board and tick the
application as shown in the [HAL lab](tutorials/hal_poke_lab.md).

## Engine page: knobs with teeth

The ENGINE mode stopped being a spectator sport. Park the focused seed on a
Euclid or Burst engine and the encoders start nudging real parameters instead of
just listing them:

| Engine | Density encoder | Tone/Tilt encoder | FX encoder |
| --- | --- | --- | --- |
| Euclid | Steps (±1 per detent) | Fills (±1) | Rotate (±1) |
| Burst | Cluster count (±1, clamped 1–16) | Spacing (+/− ≈5ms per click) | — |

Locked seeds still ignore the tweaks, and the OLED hint rail calls out the
controls the moment you land on ENGINE so nobody forgets which knob is live.

## Swing edit pop-over

Long-press **Tap** and the rig drops into a dedicated Swing page. It's a popup,
not a full mode swap, so you keep your place in the broader UI stack while
you tweak the groove.

| Control | Action |
| --- | --- |
| Tap (long press) | Enter the Swing editor. |
| Seed encoder (turn) | Coarse swing edits, ±5% per detent. |
| Density encoder (turn) | Fine swing edits, ±1% per detent. |
| Tap (short press) | Exit back to whatever page you were on. |

While you're inside, the OLED hints shout `Tap: exit swing` and
`Seed:5% Den:1%` so nobody forgets which knob does what. The edit is hot — the
internal clock and MIDI clock-out inherit the new swing percent immediately, so
schedule a metronome if you need receipts.

> Pro tip: the swing editor reports in percentages but stores the normalized
> value (0.00–0.99). Tests and `captureDisplaySnapshot` read from that same
> source, so the UI, engines, and documentation stay in sync.

Add more moves here whenever the panel choreography evolves. Docs + code should
always co-sign each other, especially when you're demoing live.
