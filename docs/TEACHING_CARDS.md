# Teaching cards

Short, printable prompts for the teaching preset bank in `presets/teaching/`.

## 01 Clock + subdivision (`teach-01-clock`)
Objective: Hear how density creates subdivisions against a steady pulse.
Listening cues: Kick on the grid, hats subdividing, resonator bed.
Variations: 1) Nudge seed 1 density from 1.0 to 1.5. 2) Toggle swing to 5%.
3-minute demo script: Start with only seed 0, add seed 1, then seed 2 for syncopation, finish with seed 3 for sustain.
Break it safely: Crank seed 2 jitter to 10 ms, then reset it to 3 ms.
Recovery (panic): Long-press Live Capture to panic, then tap Seed to reseed.

## 02 Euclidean rhythm (`teach-02-euclid`)
Objective: Contrast even spacing with clustered hits.
Listening cues: Rotating accents and changing gap sizes.
Variations: 1) Raise seed 2 probability to 1.0. 2) Drop BPM to 80.
3-minute demo script: Solo each Euclid seed, then layer them until the backbeat (seed 3) anchors the groove.
Break it safely: Set seed 1 density to 2.0 for a busy burst, then return to 0.75.
Recovery (panic): Long-press Live Capture to panic, then tap Seed to reseed.

## 03 Probability gates (`teach-03-probability`)
Objective: Hear probability as a musical filter, not random noise.
Listening cues: Rare hits from seed 0, reliable hits from seed 2.
Variations: 1) Swap probabilities of seeds 0 and 2. 2) Increase seed 3 probability to 0.7.
3-minute demo script: Start with seed 2 only, add seed 1, then introduce seed 0 as a surprise accent, finally bring in seed 3.
Break it safely: Set seed 0 probability to 0.05, then back to 0.35.
Recovery (panic): Long-press Live Capture to panic, then tap Seed to reseed.

## 04 LFSR determinism (`teach-04-lfsr`)
Objective: Show that reseeding reproduces the same groove.
Listening cues: Identical timing and color after a reseed.
Variations: 1) Reseed twice and compare. 2) Change only pitch on seed 1, then reseed to restore.
3-minute demo script: Play, reseed, point out the repeat; tweak one parameter; reseed to prove determinism.
Break it safely: Twist mutate to 0.3 on seed 1, then set it back to 0.0.
Recovery (panic): Long-press Live Capture to panic, then tap Seed to reseed.

## 05 Phase drift / polyrhythm (`teach-05-drift`)
Objective: Hear phase drift between mismatched densities.
Listening cues: Two rhythms slowly sliding past each other.
Variations: 1) Drop seed 2 density to 1.2. 2) Raise seed 0 density to 1.0.
3-minute demo script: Start with seed 0, add seed 1, then seed 2 for drift, and finish with the resonator tail.
Break it safely: Set seed 2 density to 2.5 for a short burst, then return to 1.333.
Recovery (panic): Long-press Live Capture to panic, then tap Seed to reseed.

## 06 Motif + mutation (`teach-06-motif`)
Objective: Contrast stable motif seeds with evolving ones.
Listening cues: Seed 0/1 stay fixed while seed 2/3 drift.
Variations: 1) Raise mutation on seed 2 to 0.35. 2) Lock seed 1 and reseed.
3-minute demo script: Loop the motif, then introduce mutation, then toggle repeat bias vs entropy in the randomness panel.
Break it safely: Push seed 2 transpose to -12, then return to -5.
Recovery (panic): Long-press Live Capture to panic, then tap Seed to reseed.

## 07 Density to dynamics (`teach-07-density`)
Objective: Connect density changes to perceived loudness and energy.
Listening cues: Higher density feels louder, even at the same level.
Variations: 1) Raise seed 0 density to 0.5. 2) Lower seed 3 probability to 0.4.
3-minute demo script: Start sparse, add density in order (0 to 3), then pull them back to show energy control.
Break it safely: Set seed 3 density to 3.0 for one bar, then return to 2.0.
Recovery (panic): Long-press Live Capture to panic, then tap Seed to reseed.

## 08 Constraint improvisation (`teach-08-constraint`)
Objective: Improvise within pitch and density limits.
Listening cues: Small changes feel meaningful because the palette is tight.
Variations: 1) Move seed 1 pitch between 2 and 5 only. 2) Toggle engine 3 between Burst and Euclid.
3-minute demo script: Establish a groove, change one control at a time, then trade roles between seeds 0 and 1.
Break it safely: Push seed 3 density to 1.5 and jitter to 8 ms, then restore.
Recovery (panic): Long-press Live Capture to panic, then tap Seed to reseed.
