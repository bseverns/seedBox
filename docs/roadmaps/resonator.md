# Option C — Resonator / ping machine blueprint

The `engine/Resonator.*` pair is the home for the ping engine. Same story as the
granular system: we keep the skeleton hot so the DSP bits can plug in without
reshaping the app.

## What we're building

- Short excitation bursts (2–12 ms) that feed either a Karplus-Strong string or
  a small modal bank.
- Deterministic trigger rate driven by the scheduler, with seed density /
  probability acting as the on/off filter.
- Overnight-safe metallic arpeggios: deterministic RNG keeps the jam from
  drifting when you reseed.

### Signal chain status report

- `ResonatorBank::init` now spins up the whole Karplus-Strong + modal network on
  Teensy builds. Each voice owns a white-noise burst generator, ADSR envelope,
  tilt filter, short delay line, a four-filter modal bank, and a voice mixer
  patched into the main stereo buses. Native builds keep the full planning data
  without dragging in Teensy headers so docs/tests can interrogate the plans.
- Burst envelopes convert `Seed::resonator.exciteMs` into milliseconds, and we
  stash deterministic handles/start times so the allocator can be reasoned about
  from the sim build.

## Voice allocation rules

- Default pool: 10 voices on hardware, 4 in the sim (`ResonatorBank::setMaxVoices`).
- Overflow steals the oldest ringing voice; modal clouds stay stable even when
  hammered.
- Each voice stores frequency, damping, brightness, feedback, and bank/mode so
  we can swap DSP topologies later without touching the scheduler.

## Seed genome mapping

| Seed field | How it's used |
|------------|---------------|
| `Seed::pitch` | Sets base frequency (A2 * 2^(n/12)). |
| `Seed::resonator.exciteMs` | Converted to samples for the burst envelope. |
| `Seed::resonator.damping` | Normalized into `minDamping_..maxDamping_` range. |
| `Seed::resonator.brightness` | Future tilt into modal partial amplitudes. |
| `Seed::resonator.feedback` | Sets global feedback (clamped below 1.0). |
| `Seed::resonator.mode` | 0 = Karplus-Strong string, 1 = modal preset. |
| `Seed::resonator.bank` | Index into a modal preset table (guarded + documented). |

### Modal preset atlas

We shipped a first cut of playable modal banks so contributors (and students)
can hear material changes the moment they wire in DSP assets:

| Index | Name          | Ratio set                     | Notes |
|-------|---------------|-------------------------------|-------|
| 0     | Brass shell   | 1.0, 2.01, 2.55, 3.90         | Dark-ish drum shell with long sustain. |
| 1     | Glass harp    | 1.0, 1.5, 2.5, 3.5            | Bright, shimmering bell partials. |
| 2     | Kalimba tine  | 1.0, 2.0, 3.0, 4.2            | Mid-forward pluck with rounded highs. |
| 3     | Chime tree    | 1.0, 2.63, 3.91, 5.02         | Thin, glassy cascade for airy chords. |
| 4     | Aluminum bar  | 1.0, 3.0, 5.8, 9.2            | Metallic bar with extended bloom. |
| 5     | Detuned duo   | 1.0, 1.01, 1.98, 2.97         | Beating string pair that gets weird fast. |

## Event path

1. Scheduler decides a seed fires.
2. `EngineRouter` routes engine `2` (PING) to `ResonatorBank::trigger`.
3. Voice plan is stored (start sample, frequency, envelopes, modal gains, etc.).
4. Teensy Audio graph already exists: the trigger call writes those plans into
   the burst envelope, delay line, and modal bank so hardware + sim share the
   exact same control story.

## Remaining homework

- Refine the DSP implementations (custom waveguide vs. stock Teensy nodes) once
  we drop real assets in — everything is plumbed, now it just needs tone love.
- Add serial logging hooks so we can live-debug modal selection / decay when we
  start tuning by ear.
- Extend the preset atlas with community submissions; the resolver already
  guards indices so we can safely grow the table.
