# Front panel cheat sheet — riffs, not secrets

The SeedBox surface is tiny on purpose, which means every gesture is doing
double-duty. Treat this page like the annotated set list: a quick reminder of
what the buttons and encoders pull off mid-jam, and why you'd reach for them.

## Core gestures

| Move | What happens | Why it matters |
| --- | --- | --- |
| Seed encoder button (press) | Hops between SEEDS → ENGINE → PERF → UTIL. | Fast tour of the main modes without fumbling for menus. |
| Tap button (double press) | Toggles SETTINGS, double-tap again to bounce back. | Clock plumbing, debug toggles, and other deep cuts live here. |
| Shift (long press) | Dumps you back to HOME from wherever you wandered. | Panic chord when you need to reorient a class. |
| Reseed button (long press) | Spins a fresh master seed (unless locks block it). | Handy when you want to audition brand-new genomes. |

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
