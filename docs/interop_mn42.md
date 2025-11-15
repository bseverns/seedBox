# SeedBox ↔ MN42 Interop Cheat Sheet

Welcome to the tiny treaty between SeedBox and the MN42 controller. This page
keeps both synth brains honest about which MIDI CCs they speak, how they wave
hello, and what to poke when you want the rigs to vibe in sync. Think lab notes,
not corporate whitepaper.

## TL;DR handshake

1. **MN42 boots** and spits a Control Change on channel 1, controller 14,
   value `0x01`. That's the punk rock version of “hey, I'm awake.”
2. **SeedBox receives** that CC, routes it through `MidiRouter`, and when its own
   subsystems are caffeinated, it fires back CC 14 with value `0x11`.
3. **Both ends send keep-alives** every few seconds (same CC 14, value `0x7F`).
   Miss two beats and you can assume the other side face-planted.
4. **Optional identity flex:** MN42 drops a short SysEx packet using the
   non-commercial ID `0x7D` followed by the ASCII-ish signature `0x4D 0x4E 0x42`
   (`M N B`). SeedBox ignores any mystery gear without that vibe.

All of those literal numbers live in
[`src/interop/mn42_map.h`](../src/interop/mn42_map.h). Copy-paste the constants
instead of retyping magic values.

## CC map at a glance

| CC | Constant | What it means | Value range |
| --- | --- | --- | --- |
| 14 | `cc::kHandshake` | Boot hello, ack, and keep-alives. | `0x01`, `0x11`, `0x7F` |
| 15 | `cc::kMode` | Bit field describing clock + debug toggles. | Bit mask |
| 16 | `cc::kSeedMorph` | Blend between seed snapshots. | `0–127` |
| 17 | `cc::kTransportGate` | Momentary gate for transport control. | `0` off, `>0` on |

### Mode bit breakdown (CC 15)

| Bit | Constant | When it's set |
| --- | --- | --- |
| `0x01` | `mode::kFollowExternalClock` | MN42 supplies tempo, SeedBox listens. |
| `0x02` | `mode::kExposeDebugMeters` | SeedBox streams raw meters back over SysEx/logs. |
| `0x04` | `mode::kArpAccent` | MN42 accent lane punches harder. |
| `0x08` | `mode::kLatchTransport` | Treat transport like a toggle instead of a momentary. |

### Seed parameter ring (CC 20–28)

MN42’s encoder ring spits the CCs captured in
[`src/interop/mn42_param_map.h`](../src/interop/mn42_param_map.h). Every value
targets the seed currently in focus. The maths is plain on purpose so students
can sketch ranges on a whiteboard before touching code.【F:src/interop/mn42_param_map.h†L8-L58】

| CC | Label | Translation | Intent notes |
| --- | --- | --- | --- |
| 20 | `Engine cycle` | `>=64` spins to the next engine, `<64` backs up. | Keeps parity with the front-panel engine encoder. |
| 21 | `Focus seed` | Divide `0–127` into `seed_count` buckets. | Remote control over which genome the remaining macros nudge. |
| 22 | `Seed pitch` | Map `0–127` → `-24..+24` semitones. | Quick octave sweeps for the focused seed. |
| 23 | `Seed density` | Map `0–127` → `0..8` hits/beat. | Dials the Euclid engine toward drones or sparse hits. |
| 24 | `Seed probability` | Map `0–127` → `0..1`. | Bernoulli gate weight — twist for ghost notes. |
| 25 | `Seed jitter` | Map `0–127` → `0..30ms`. | Humanize timing without rewriting the sequencer. |
| 26 | `Seed tone` | Map `0–127` → `0..1`. | Tilt EQ macro; 0 is dark, 1 is bright. |
| 27 | `Seed spread` | Map `0–127` → `0..1`. | Stereo width from mono (0) to wide (1). |
| 28 | `Seed mutate` | Map `0–127` → `0..1`. | Caps how wild the mutate encoder can swing. |

`GetMutableParamMap()` exposes the same table at runtime so tests or alternate
controllers can remix the assignments without recompiling. AppState grabs those
entries and applies them verbatim when MN42 CCs roll in.【F:src/app/AppState.cpp†L1191-L1291】

## Wiring it up on the SeedBox side

```cpp
#include "interop/mn42_map.h"
#include "io/MidiRouter.h"

void wireRouter(MidiRouter& router) {
  router.setControlChangeHandler([](uint8_t ch, uint8_t cc, uint8_t value) {
    if (ch != seedbox::interop::mn42::kDefaultChannel) return;
    if (cc == seedbox::interop::mn42::cc::kHandshake &&
        value == seedbox::interop::mn42::handshake::kHello) {
      // Kick off your ack logic here.
    }
  });
}
```

## Minimal SysEx handshake packet

```
F0 7D 4D 4E 42 01 F7
```

- `F0`/`F7`: SysEx start/end.
- `7D`: Non-commercial manufacturer ID (agreed on both ends).
- `4D 4E 42`: The `M N B` signature referenced in `handshake::kProduct*`.
- `01`: Payload meaning “MN42 present.”

Respond however you want — CC ack, flashing LEDs, or a triumphant glitch-pop.

## Testing loop ideas

- **Clock follow dry run:** Set `mode::kFollowExternalClock`, spin MN42's tempo,
  and watch SeedBox's scheduler for drift.
- **Debug voyeurism:** Flip `mode::kExposeDebugMeters` to force SeedBox into
  spewing level meters back over whichever channel you're sniffing.
- **Transport mayhem:** Toggle `mode::kLatchTransport` and spam CC 17 to ensure
  your UI respects latch vs. momentary semantics.

Keep tweaking and jotting discoveries. This page should evolve like a gig
setlist — fast edits, honest notes, no preciousness.

## Router defaults and port matrix

SeedBox now routes MIDI through a tiny facade with explicit port capabilities.
Both USB and the TRS Type-A jacks can talk clock, transport, and control
changes. The CLI backend mirrors the same truth table so native tests can drive
either port without needing a Teensy on the desk.

| Port | Clock In/Out | Transport In/Out | CC In/Out | Notes |
| --- | --- | --- | --- | --- |
| `MidiRouter::Port::kUsb` | ✅ / ✅ | ✅ / ✅ | ✅ / ✅ | Backed by `usbMIDI` on hardware, CLI shim in native builds. |
| `MidiRouter::Port::kTrsA` | ✅ / ✅ | ✅ / ✅ | ✅ / ✅ | Lives on Serial7; channel map clamps everything to MN42's lane by default. |

`QUIET_MODE` short-circuits the actual wire writes, but the router still logs
what *would* have gone out so tests can assert on behaviour without buzzing a
real rig.

### PERF page routing snapshot

`AppState::initHardware()` declares a routing matrix per UI page. PERF is the
live set, so it mirrors clock + transport out of both ports while accepting CCs
from either side:

```cpp
std::array<MidiRouter::RouteConfig, MidiRouter::kPortCount> perf{};
const auto usb = static_cast<std::size_t>(MidiRouter::Port::kUsb);
const auto trs = static_cast<std::size_t>(MidiRouter::Port::kTrsA);
perf[usb].acceptClock = perf[trs].acceptClock = true;
perf[usb].acceptTransport = perf[trs].acceptTransport = true;
perf[usb].acceptControlChange = perf[trs].acceptControlChange = true;
perf[usb].mirrorClock = perf[trs].mirrorClock = true;
perf[usb].mirrorTransport = perf[trs].mirrorTransport = true;
```

EDIT/HACK modes fall back to CC-only intake so classroom demos can mash knobs
without surprise tempo hijacks.

### Panic + note-off guardrails

`MidiRouter::panic()` slams an "all notes off" CC to every port/channel that
the router thinks is still alive. Note-offs are tracked with a guard table so
duplicate releases do nothing. Native tests hit this path through the CLI
backend to make sure the guard behaves before we drop it onto hardware.

### Param map scaffold

Future controller integrations can stash CC→parameter descriptions inside
[`src/interop/mn42_param_map.h`](../src/interop/mn42_param_map.h). Right now
it's an empty grid with documentation baked in; feel free to populate it as soon
as the MN42 panel layout locks.
