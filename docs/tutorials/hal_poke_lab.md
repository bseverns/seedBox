# HAL poke lab — bend the mocks to your will

The HAL stack is the switchboard that keeps SeedBox bilingual: it speaks to real
Teensy hardware without breaking stride when you flip to the simulator. This lab
hands you a soldering-iron-free sandbox. We’ll boot the mock audio path, inject
Board button events, and script a loop that mirrors the panel choreography seen
in the integration tests.

## TL;DR

- `hal::audio::init` + `start` stash your callback and arm both hardware and
  simulator backends; native builds mirror the block size + clock math without
  allocating inside the callback.【F:src/hal/hal_audio.cpp†L86-L152】
- `hal::audio::mockPump(frames)` advances the sample clock and feeds your
  callback floating-point buffers — perfect for unit tests that want to sniff
  DSP output without wiring a codec.【F:src/hal/hal_audio.cpp†L139-L152】
- Physical panel tests use `hal::nativeBoardFeed` or
  `hal::nativeBoardSetButton`, then tick the application. Board and InputEvents
  own control sampling and gesture recognition.
- `tests/test_app/test_app.cpp` demonstrates navigation, reseeding, and Storage
  save/recall through that pipeline. Raw GPIO mocks are only for testing the
  GPIO layer; they do not drive application controls.

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

## 3. Script the Board surface

Use the same named controls as the physical panel. For example, a Seed switch
hold requests a reseed outside Storage:

```c++
#include "app/AppState.h"
#include "hal/Board.h"

void rehearseReseed() {
  hal::nativeBoardReset();
  AppState app;
  app.initSim();
  hal::nativeBoardFeed("btn seed down");
  hal::nativeBoardFeed("wait 520ms");
  hal::nativeBoardFeed("btn seed up");
  for (int i = 0; i < 96; ++i) {
    app.tick();
  }
}
```

`btn` accepts `seed`, `density`, `tone`, `fx`, `tap`, `shift`, `alt`, and
`capture`. `enc` accepts `seed`, `density`, `tone`, and `fx` with a signed delta,
for example `enc density -2`. `wait 520ms` advances scripted Board time.

## 4. Rehearse Storage save and recall

The integration walkthrough holds Alt to enter Storage, then holds the Seed
switch to save or briefly presses it to recall. InputEvents reports release
with a duration measured by the Board clock. A hold of at least 450 ms saves;
a shorter press recalls. Both actions happen on release.

Slot and global lock state can be exercised through `seedPageToggleLock` and
`seedPageToggleGlobalLock`. Board currently has no dedicated physical Lock
button. Pins 2 and 3 are the Seed switch and Density encoder A respectively;
never initialize them as independent Reseed/Lock controls in application code.

## 5. Run the regression

```bash
pio test -e native --filter test_app
```

The suite includes a scripted panel walkthrough and checks that raw GPIO edges
cannot reseed or lock the application. The full control inventory and wiring
are in [the builder primer](../builder_bootstrap.md).
