# Storage roadmap — presets without panic

SeedBox finally has a persistence spine. This note lays out where the current
implementation stands and how we expect it to grow.

## Today's build

- **Store interface (`include/io/Store.h`).** Three verbs — `list`, `load`,
  `save` — so lessons can hot swap backends without rewriting UI scripts.
- **Backends in play.**
  - `StoreNull`: inert. Great for regression tests that must prove "no storage"
    behaviour.
  - `StoreEeprom`: deterministic byte layout, shared by Teensy and native
    builds. Native runs keep the bytes in RAM; hardware hits the real EEPROM.
  - `StoreSd`: optional but wired up so future labs can target SD cards or host
    directories.
- **Quiet mode guardrails.** `QUIET_MODE` keeps every store read-only, so student
  rigs don't silently overwrite each other.
- **Preset schema (`app/Preset.*`).** JSON payload that captures: clock data,
  router selections, seed genomes, seed-engine overrides, and the current UI
  page. Small enough for EEPROM, explicit enough for classroom walkthroughs.
- **UI flow.** Storage page on the front panel uses the reseed button: short
  press recalls the active slot, long press saves it. Crossfades stretch over 48
  ticks so transitions feel musical, not jarring.

## Near-future riffs

- **Slot naming conventions.** Today we default to `"default"`. A follow-up could
  expose alpha shortcuts (`A`, `B`, `C`) and MIDI-driven naming.
- **Preset banks.** Multi-slot banks stored in EEPROM would let workshops ship
  with a curated starting set. SD could back deeper libraries.
- **Manifest metadata.** Extend the JSON with optional notes (tempo hints,
  authorship) so the OLED can flash context when recalling a preset.
- **Transport-aware crossfade.** Currently we linearly blend parameters. Future
  builds might schedule actual audio crossfades or parameter morph targets per
  engine.

Treat this doc like a living studio notebook — scribble TODOs, drop sketches,
record glitches. Persistence is finally a first-class topic for the course.
