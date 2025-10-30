# Hardware Abstraction Layer (HAL)

Welcome to the scrappy wiring closet where SeedBox pretends to be agnostic about the
outside world. This HAL layer gives us a clean handshake between the musical brain and
whatever hunk of silicon we strap it to — Teensy on stage, or native mocks when we are
rocking tests on a laptop.

## TL;DR

* `src/hal/hal_audio.*` – owns the audio callback contract. Same interface everywhere,
  thin Teensy stream on hardware, deterministic mock driver on native builds.
* `src/hal/hal_io.*` – coordinates digital I/O pin bookkeeping and edge reporting.
  Hardware uses `digitalRead`/`digitalWrite`; native builds keep a queue you can poke in
  tests.
* Both layers avoid heap churn inside callbacks. No sneaky `new`/`malloc` when the audio
  interrupt fires.

## Audio HAL (`hal_audio`)

The audio HAL exposes a single entrypoint for “please fill this stereo buffer”:

```cpp
#include "hal/hal_audio.h"

void audioTick(const hal::audio::StereoBufferView &buffer, void *);

void bootAudio() {
  hal::audio::init(&audioTick);
  hal::audio::start();
}
```

`StereoBufferView` hands you two float slices (`left`/`right`) and the frame count for the
current block. Fill them with samples in **[-1, 1]**. The call happens on:

* Teensy: inside a custom `AudioStream::update()` callback. The stream grabs pre-allocated
  `audio_block_t`s, calls your function, and converts floats to signed 16-bit data. Timing
  matters — treat it like an ISR. Keep work bounded and avoid any blocking calls.
*Teensy trivia:* The upstream Audio library hides a bunch of DSP nodes (mixers,
  granular effects, etc.) behind the `__ARM_ARCH_7EM__` macro. PlatformIO sometimes drops
  that define when it spins up the IMXRT toolchain, so `HardwarePrelude.h` pulls in
  `<Arduino.h>` first, peeks at the board macros (`__IMXRT1062__`, `ARDUINO_TEENSY40/41`), and
  force-enables the flag when we're clearly on 7EM silicon. If CI ever screams about
  abstract mixers, start your debugging tour there.
* Native: a deterministic mock. `hal::audio::mockPump(frames)` allocates scratch buffers
  *before* invoking your callback. This keeps callbacks free from dynamic allocation while
  letting tests drive arbitrary block sizes.

### Timing Cheatsheet

| Build        | Sample rate             | Frames per block                           | Notes |
| ------------ | ----------------------- | ------------------------------------------ | ----- |
| Teensy (`SEEDBOX_HW`) | `AUDIO_SAMPLE_RATE_EXACT` (44.1-ish kHz) | `AUDIO_BLOCK_SAMPLES` (128 frames) | Update fires from the audio interrupt. Budget microseconds, not milliseconds. |
| Native       | Defaults to 48 kHz (tweak with `mockSetSampleRate`) | Defaults to 128 (override per pump call) | Invoke `mockPump` manually in tests to simulate time advancing. |

**Rule of thumb:** Do your DSP math and bail. Any heavy lifting or allocation should be
moved out of the callback and fed through lock-free queues if needed.

## Digital I/O HAL (`hal_io`)

`hal_io` keeps a registry of pins plus a single edge callback. You describe the pins once
and then poll to capture changes:

```cpp
#include "hal/hal_io.h"

using hal::io::PinNumber;
using hal::io::DigitalConfig;

constexpr DigitalConfig kPins[] = {
    {0, true, true},   // Encoder A (input, pull-up)
    {1, true, true},   // Encoder B
    {2, true, false},  // Gate input
    {3, false, false}, // Gate output
};

void onEdge(PinNumber pin, bool level, std::uint32_t micros, void *user_data);

void bootIO() {
  hal::io::init(kPins, std::size(kPins));
  hal::io::setDigitalCallback(&onEdge);
}

void loopTick() {
  hal::io::poll();
}
```

* Hardware builds (`SEEDBOX_HW`) call `pinMode` during `init`, then `poll()` reads each
  input and triggers `onEdge` whenever the level flips. The callback receives the
  timestamp from `micros()` so you can debounce with real timing data. Writes go straight
  to `digitalWrite`.
* Native builds stash pin state in-memory. `poll()` flushes a queue of synthetic events.
  Use `hal::io::mockSetDigitalInput(pin, level, timestamp)` inside tests to schedule
  edges, then call `poll()` to deliver them.

As with audio, `poll()` avoids heap allocations. Events are queued ahead of time and the
callback just sees the payload.

## Testing & Mocking Rituals

* **Audio:** Call `hal::audio::init(callback)`, `mockSetSampleRate()` if you need a custom
  rate, and `mockPump(frames)` to trigger the callback. The mock reuses scratch buffers so
  your callback stays allocation-free.
* **Digital I/O:** Describe your pins, register a callback, then use
  `mockSetDigitalInput()` to enqueue edges. Each call stores the timestamp you pass in.
  After `poll()`, you can assert on side-effects or recorded events.

Keep tests explicit about timing — pass microsecond values into the mocks so the stories
in your assertions read like a lab notebook.

## Why this HAL exists

The SeedBox codebase serves two masters: the live hardware build and a fast, portable
native build. These HAL layers keep the split tidy. Every time we add a new hardware touch
point we wire it through here, document the contract, and provide a native shim so tests
can stay loud and proud.

