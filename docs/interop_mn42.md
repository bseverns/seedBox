# MN42 handshake — nudging SeedBox from the mothership

SeedBox and MN42 share DNA. This doc captures the MIDI control map so both rigs
can improvise together without guesswork.

## Core handshake

```mermaid
graph TD
  MN42[M.N.42 Controller] -- CC 20 + mode bits --> SeedBox
  SeedBox -- Status ping (CC 100) --> MN42
  SeedBox -- TODO: audio clip URL --> (Future listeners)
```

1. MN42 sends a boot ping: Control Change 20 with value `0x7F`.
2. SeedBox replies with CC 100 containing the active engine ID (0 = Sampler,
   1 = Granular, 2 = Resonator).
3. MN42 uses the shared map in `src/interop/mn42_map.h` to align knob labels and
   mute states.

## Control map

| CC | Meaning | Notes |
| -- | ------- | ----- |
| 20 | Engine cycle | Same encoder as the front panel. Values >= 64 advance. |
| 21 | Density macro | 0-127 mapped to per-seed density. |
| 22 | Probability macro | Controls trigger probability. |
| 23 | Tone/tilt | Nudges timbre. |
| 24 | FX/mutate | Seeds the modulation depth. |
| 100 | Status ping | SeedBox -> MN42, echoes current engine. |

Extend the table carefully. New CCs should land here first with a short blurb
explaining the classroom intent.

## Example exchange

```text
MN42 -> SeedBox: CC20 ch1 value 0x7F   # request engine snapshot
SeedBox -> MN42: CC100 ch1 value 0x01 # currently granular
MN42 -> SeedBox: CC21 ch1 value 0x40  # set density mid-way
SeedBox -> MN42: CC100 ch1 value 0x01 # still granular (ack)
```

## Roadmap

- TODO: add SysEx block to push seed names or random seeds.
- TODO: document how MN42 can request the deterministic golden audio fixtures
  once they exist.
- TODO: host micro "listen here" clips for each engine state.
