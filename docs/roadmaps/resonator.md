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
| `Seed::resonator.bank` | Index into a modal preset table (todo). |

## Event path

1. Scheduler decides a seed fires.
2. `EngineRouter` routes engine `2` (PING) to `ResonatorBank::trigger`.
3. Voice plan is stored (start sample, frequency, etc.).
4. Once the Teensy Audio graph arrives, it will grab these plans to configure a
   burst generator + resonator object.

## Remaining homework

- Build the modal preset table (probably 6-ish metallic banks to start).
- Prototype excitation DSP either with the Teensy Audio library or a custom
  waveguide so we can shape the burst envelope precisely.
- Add debug hooks to dump active voice frequencies over serial for tuning.
