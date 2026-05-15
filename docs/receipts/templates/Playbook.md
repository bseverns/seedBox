# Native Fixture Sound Playbook

## How To Use This
- These WAVs are regression fixtures, not presets.
- The recipes describe the musical ingredients: engine, seed role, density, clocking, spread, modulation, and reseed behavior.
- Exact samples may differ, but the gesture should match.

## Fixture Recipes
### granular-haze
Sound: stereo grain cloud with slow motion.
How to get there:
- Engine: Granular
- Source: live input or a sustained sample
- Grain size: medium-long
- Spray: moderate to high
- Stereo spread: high
- Density: medium
- Tone: bright enough to keep grains audible
- Movement: slow tone/spread nudges or macro orbit
Listen for: wide cloud, audible motion, no static mono wash.

### resonator-tail
Sound: plucked resonator decay.
How to get there:
- Engine: Resonator
- Input: short transient or pluck-like material
- Feedback: medium-high
- Damping: lower for longer ring, higher for tighter body
- Bank/mode: choose a tuned resonant body
Listen for: clear attack, musical tail, decay shape changes with damping.

### euclid-mask
Sound: gated rhythmic stereo pattern.
How to get there:
- Engine: Euclid
- Density/steps: sparse-to-medium pattern
- Swing: optional
- Pan/spread: move per hit or use macro orbit
Listen for: repeatable rhythmic mask, not random triggering.

### burst-cluster
Sound: clustered trigger flams.
How to get there:
- Engine: Burst
- Density: medium-high
- Jitter/spacing: enough to hear individual hits
- Probability: high for obvious clusters
Listen for: grouped bursts with spacing, not a flat roll.

### engine-hybrid-stack
Sound: layered sampler/resonator/granular stack.
How to get there:
- Slot 1: Sampler for tonal anchor
- Slot 2: Resonator for body/tail
- Slot 3: Granular for haze/texture
- Scheduler: Euclid/Burst gates
- Macro: pan and density movement
Listen for: separate layers sharing one groove.

### long-random-take / reseed-*
Sound: deterministic evolving chaos.
How to get there:
- Start from a playable seed
- Use reseed/live seed prime modes
- Let the machine cycle worlds over time
- Keep density/probability high enough to expose changes
Listen for: changing engine character while staying repeatable.
