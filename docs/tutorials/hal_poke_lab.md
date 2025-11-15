# HAL poke lab — bend the mocks to your will

The HAL stack is the switchboard that keeps SeedBox bilingual: it speaks to real
Teensy hardware without breaking stride when you flip to the simulator. This lab
hands you a soldering-iron-free sandbox. We’ll boot the mock audio path, inject
synthetic GPIO edges, and script a loop that mirrors the panel choreography seen
in the integration tests.

## TL;DR

- `hal::audio::init` + `start` stash your callback and arm both hardware and
  simulator backends; native builds mirror the block size + clock math without
  allocating inside the callback.【F:src/hal/hal_audio.cpp†L86-L152】
- `hal::audio::mockPump(frames)` advances the sample clock and feeds your
  callback floating-point buffers — perfect for unit tests that want to sniff
  DSP output without wiring a codec.【F:src/hal/hal_audio.cpp†L139-L152】
- `hal::io::mockSetDigitalInput` queues timestamped edges so `poll()` delivers
  them in order, just like a frantic panel session. The simulator stores the
  last level per pin so reads stay deterministic.【F:src/hal/hal_io.cpp†L53-L147】
- `tests/test_app/test_app.cpp` already choreographs reseed + lock buttons with
  the mock HAL; treat it as a pattern you can riff on in fresh fixtures.【F:tests/test_app/test_app.cpp†L74-L99】

## 1. Boot the audio sandbox

Start by wiring your DSP callback into the audio HAL. Even on the native build
we mimic the same frame cadence as the Teensy runtime, so timing experiments stay
legit.

```c++
#include "hal/hal_audio.h"

namespace {
void render(const hal::audio::StereoBufferView &buffer, void *user) {
  float phase = *static_cast<float*>(user);
  for (std::size_t i = 0; i < buffer.frames; ++i) {
    const float sample = std::sin(phase);
    buffer.left[i] = sample;
    buffer.right[i] = sample;
    phase += 0.01f;
  }
  *static_cast<float*>(user) = phase;
}
}  // namespace

void boot() {
  static float phase = 0.0f;
  hal::audio::init(&render, &phase);
  hal::audio::start();
}
```

Under the hood `init` resets the shared sample clock, caches the callback, and
locks the simulator to the canonical 48 kHz sample rate while matching hardware
block sizes when you cross-compile for Teensy.【F:src/hal/hal_audio.cpp†L86-L118】
The callback runs inside a pre-allocated buffer (no heap churn), so keep it fast
and side-effect-free.

## 2. Pump frames without hardware

Once the callback is armed, tests can advance time manually. Flip the HAL into
a running state and use the mock pump to feed your buffer.

```c++
boot();
hal::audio::mockPump(256);  // two Teensy-sized blocks in one go
const std::uint32_t ticks = hal::audio::sampleClock();
```

`mockPump` hands over zeroed scratch buffers, calls your callback, and bumps the
sample clock by the requested frame count. That clock mirrors the hardware ISR
path, so you can assert on absolute timing without conditional compilation.【F:src/hal/hal_audio.cpp†L139-L152】
Need a different tempo story? Call `mockSetSampleRate` before the pump and the
rest of the engine maths will follow suit.【F:src/hal/hal_audio.cpp†L133-L137】

## 3. Script panel edges like a proctor

Digital inputs follow the same pattern. Describe your pins once, then inject
edges in timestamp order.

```c++
#include "hal/hal_io.h"

namespace {
constexpr hal::io::DigitalConfig kPins[] = {
    {2, true, true},   // reseed button (input, pull-up)
    {3, true, true},   // lock button (input, pull-up)
};

void onEdge(hal::io::PinNumber pin, bool level, std::uint32_t micros, void *) {
  // record events, update state machine, etc.
}
}  // namespace

void initPanel() {
  hal::io::init(kPins, std::size(kPins));
  hal::io::setDigitalCallback(&onEdge);
}

void fireFakePresses() {
  std::uint32_t now = 1000;
  hal::io::mockSetDigitalInput(3, true, now);
  hal::io::poll();   // delivers the rising edge
  now += 650000;
  hal::io::mockSetDigitalInput(3, false, now);
  hal::io::poll();   // delivers the falling edge
}
```

The simulator keeps a queue of `PendingEvent` structs and only updates the cached
pin level once `poll()` drains them, making the timing math easy to read in your
assertions.【F:src/hal/hal_io.cpp†L81-L147】 Hardware builds skip the queue entirely
and read straight from GPIO, so the same callback sees real switch bounce when you
deploy.【F:src/hal/hal_io.cpp†L81-L113】

## 4. Steal choreography from the integration test

`tests/test_app/test_app.cpp` already demonstrates a full UI journey driven
entirely by mock HAL pokes. One highlight is the lock-button helper: it schedules
a press, ticks the app while the button is held, then releases with a long-delay
stamp so the firmware’s hold logic fires.

```c++
void pressLockButton(AppState &app, PanelClock &clock, bool longPress) {
  clock.advance(1000);
  hal::io::mockSetDigitalInput(kLockPin, true, clock.now());
  app.tick();
  const int idleTicks = longPress ? 48 : 8;
  for (int i = 0; i < idleTicks; ++i) {
    app.tick();
  }
  clock.advance(longPress ? 650000 : 120000);
  hal::io::mockSetDigitalInput(kLockPin, false, clock.now());
  app.tick();
  runTicks(app, 6);
}
```

Use that pattern when you need a reproducible story about long-press logic, seed
locks, or page transitions. The test harness keeps the panel clock in plain
microseconds so every assertion reads like a lab note.【F:tests/test_app/test_app.cpp†L74-L99】

## 5. Run the regression when in doubt

Wrap your experiment in a Unity test, then run just the HAL-heavy suite:

```bash
pio test -e native --filter test_app/test_app.cpp
```

The run boots the simulator board, feeds scripted button presses, and proves the
mock HAL API behaved the same way your doc claims it does. When the Teensy rig is
packed away, this loop keeps the muscle memory fresh.
