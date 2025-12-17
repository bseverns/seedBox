#if !SEEDBOX_HW

#include "hal/Board.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <deque>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hal {

namespace {

// NativeBoard plays puppet master for the simulator.  Instead of watching real
// GPIO edges we pre-chew a script that describes button presses and encoder
// spins.  Each ScriptEvent is a bite-sized instruction for the poll loop.
struct ScriptEvent {
  enum class Type { Wait, Button, Encoder } type{Type::Wait};
  std::uint64_t duration_us{0};
  Board::ButtonID button{Board::ButtonID::TapTempo};
  bool pressed{false};
  Board::EncoderID encoder{Board::EncoderID::SeedBank};
  int32_t encoder_delta{0};
};

constexpr std::array<std::pair<const char*, Board::ButtonID>, 8> kButtonLookup{{
    {"seed", Board::ButtonID::EncoderSeedBank},
    {"density", Board::ButtonID::EncoderDensity},
    {"tone", Board::ButtonID::EncoderToneTilt},
    {"fx", Board::ButtonID::EncoderFxMutate},
    {"tap", Board::ButtonID::TapTempo},
    {"shift", Board::ButtonID::Shift},
    {"alt", Board::ButtonID::AltSeed},
    {"capture", Board::ButtonID::LiveCapture},
}};

constexpr std::array<std::pair<const char*, Board::EncoderID>, 4> kEncoderLookup{{
    {"seed", Board::EncoderID::SeedBank},
    {"density", Board::EncoderID::Density},
    {"tone", Board::EncoderID::ToneTilt},
    {"fx", Board::EncoderID::FxMutate},
}};

std::string toLower(std::string_view in) {
  // Micro helper: normalise tokens from the script parser.  Everything below
  // expects lowercase strings so students can type "Tap" or "tap" without
  // tripping over case sensitivity.  We keep it tiny on purpose so folks can
  // single-step through it in a debugger and see `std::transform` in action.
  std::string out(in);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

class NativeBoard final : public Board {
public:
  void poll() override {
    // The simulator advances time in 10ms slices.  Every poll call jumps the
    // clock forward and then chews through as much of the scripted input as
    // possible.  This mirrors the "tight loop" vibe of embedded firmware
    // without forcing the host CPU to sleep.  If you want to explain game loop
    // patterns to students, this is an approachable anchor point.
    now_us_ += poll_period_us_;
    now_ms_ = static_cast<std::uint32_t>(now_us_ / 1000u);

    // Dev-note for the lab: each poll step nibbles through the scripted queue
    // until a `wait` tells us to pause.  That keeps simulated time moving at
    // the same cadence as the hardware board class.
    processScript();
  }

  ButtonSample sampleButton(ButtonID id) const override {
    // Teaching hook: `ButtonID` is an enum that lines up with the physical
    // front panel.  We translate it into an index, sanity-check bounds, and
    // return whatever the script last wrote.  The empty ButtonSample fallback
    // acts like a hardware input that was never wired.
    const std::size_t idx = static_cast<std::size_t>(id);
    if (idx >= button_samples_.size()) {
      return ButtonSample{};
    }
    return button_samples_[idx];
  }

  int32_t consumeEncoderDelta(EncoderID id) override {
    // Encoders report movement as deltas, not absolute positions.  We stash the
    // accumulated delta, zero the bucket (so the caller "consumes" it), and
    // hand the value back.  The guard rails are intentionally boring: stay
    // inside the array or return 0 so demos don't crash on typoed IDs.
    const std::size_t idx = static_cast<std::size_t>(id);
    if (idx >= encoder_deltas_.size()) {
      return 0;
    }
    const int32_t delta = encoder_deltas_[idx];
    encoder_deltas_[idx] = 0;
    return delta;
  }

  bool tapTempoActive() const override {
    // Convenience shim so higher layers can ask "is the tap button down right
    // now?" without spelunking through the sample buffer.
    return button_samples_[static_cast<std::size_t>(ButtonID::TapTempo)].pressed;
  }

  std::uint32_t nowMillis() const override { return now_ms_; }
  std::uint64_t nowMicros() const override { return now_us_; }

  void reset() {
    // Hard reset for the simulator: wipe any pending scripted events, reset the
    // virtual GPIO snapshots, and slam the clock back to zero.  This keeps
    // tests hermetic and mirrors the "pull the USB cable" ritual students know
    // from real hardware.
    script_.clear();
    std::fill(button_samples_.begin(), button_samples_.end(), ButtonSample{});
    std::fill(encoder_deltas_.begin(), encoder_deltas_.end(), 0);
    now_us_ = 0;
    now_ms_ = 0;
  }

  // Accept CLI script lines like "btn seed down" or "wait 10ms".  By keeping
  // the format human readable we can teach automation without burying folks in
  // binary blobs.
  void feed(std::string_view line) {
    // Entry point for scripted input.  Each line is a mini DSL command like
    // "wait 10ms" or "btn seed down".  By keeping the parser here and
    // readable we can show new firmware folks how to translate human-friendly
    // text into structured events without needing a full parser generator.
    auto trimmed = trim(line);
    if (trimmed.empty() || trimmed.front() == '#') {
      return;
    }
    std::istringstream stream{std::string(trimmed)};
    std::string command;
    stream >> command;
    command = toLower(command);
    if (command == "wait" || command == "sleep") {
      double value = 0.0;
      std::string unit;
      stream >> value >> unit;
      if (unit.empty()) {
        unit = "ms";
      }
      unit = toLower(unit);
      std::uint64_t duration = 0;
      if (unit == "ms" || unit == "millis" || unit == "milliseconds") {
        duration = static_cast<std::uint64_t>(value * 1000.0);
      } else {
        duration = static_cast<std::uint64_t>(value);
      }
      ScriptEvent evt;
      evt.type = ScriptEvent::Type::Wait;
      evt.duration_us = duration;
      script_.push_back(evt);
      return;
    }

    if (command == "btn" || command == "button") {
      std::string idToken;
      std::string stateToken;
      stream >> idToken >> stateToken;
      if (idToken.empty() || stateToken.empty()) {
        return;
      }
      if (auto optId = parseButton(idToken)) {
        const bool pressed = (toLower(stateToken) == "down" || toLower(stateToken) == "press" || toLower(stateToken) == "on");
        ScriptEvent evt;
        evt.type = ScriptEvent::Type::Button;
        evt.button = *optId;
        evt.pressed = pressed;
        script_.push_back(evt);
      }
      return;
    }

    if (command == "enc" || command == "encoder") {
      std::string idToken;
      int delta = 0;
      stream >> idToken >> delta;
      if (idToken.empty()) {
        return;
      }
      if (auto optId = parseEncoder(idToken)) {
        ScriptEvent evt;
        evt.type = ScriptEvent::Type::Encoder;
        evt.encoder = *optId;
        evt.encoder_delta = delta;
        script_.push_back(evt);
      }
      return;
    }
  }

  // Unit tests sometimes need to jump the simulated clock ahead without
  // chewing through poll() cycles.  fastForward makes that explicit.
  void fastForward(std::uint64_t micros) {
    // Classroom cheat code: jump the simulated clock forward without burning
    // CPU on poll() calls.  Handy for testing timeouts or envelope decay logic
    // where you only care about "time later" not the steps in between.
    now_us_ += micros;
    now_ms_ = static_cast<std::uint32_t>(now_us_ / 1000u);
  }

  void setButton(ButtonID id, bool pressed) {
    const std::size_t idx = static_cast<std::size_t>(id);
    if (idx >= button_samples_.size()) {
      return;
    }
    auto& sample = button_samples_[idx];
    if (sample.pressed == pressed) {
      return;
    }
    sample.pressed = pressed;
    sample.timestamp_us = now_us_;
  }

private:
  static std::string_view trim(std::string_view text) {
    // Trim leading and trailing whitespace for the script parser.  Written in
    // plain C++17 so it is a friendly demo of string_view mutation.
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
      text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
      text.remove_suffix(1);
    }
    return text;
  }

  static std::optional<Board::ButtonID> parseButton(std::string_view token) {
    // Map human-friendly button nicknames to the canonical enum.  Returning
    // std::nullopt instead of throwing keeps the flow approachable for labs and
    // allows students to experiment with failure cases without crashing.
    const std::string key = toLower(token);
    for (const auto& entry : kButtonLookup) {
      if (key == entry.first) {
        return entry.second;
      }
    }
    return std::nullopt;
  }

  static std::optional<Board::EncoderID> parseEncoder(std::string_view token) {
    // Same idea as parseButton but for the four encoders.  Keeping separate
    // lookup tables makes it obvious how to extend the script language if the
    // front panel grows more controls.
    const std::string key = toLower(token);
    for (const auto& entry : kEncoderLookup) {
      if (key == entry.first) {
        return entry.second;
      }
    }
    return std::nullopt;
  }

  void processScript() {
    // Core of the native stack: walk through the queued ScriptEvents and apply
    // them to the simulated hardware state.  Because we munch events until a
    // "wait" asks us to pause, this function doubles as a lightweight
    // scheduler demo.
    while (!script_.empty()) {
      auto& evt = script_.front();
      if (evt.type == ScriptEvent::Type::Wait) {
        if (evt.duration_us > poll_period_us_) {
          // Hold off until the scripted wait has fully elapsed.  We subtract the
          // poll period and bail so the caller can spin the main loop again,
          // mimicking a delay in hardware time.
          evt.duration_us -= poll_period_us_;
          break;
        }
        if (evt.duration_us > 0) {
          now_us_ += evt.duration_us;
          now_ms_ = static_cast<std::uint32_t>(now_us_ / 1000u);
        }
        script_.pop_front();
        continue;
      }

      if (evt.type == ScriptEvent::Type::Button) {
        // Mirror button presses directly into the sample buffer so AppState can
        // pretend it is talking to the hardware board.
        writeButton(evt.button, evt.pressed);
        script_.pop_front();
        continue;
      }

      if (evt.type == ScriptEvent::Type::Encoder) {
        // Encoders accumulate delta steps that the UI consumes later.  This
        // mirrors how the Teensy path buckets quadrature steps per poll.
        const std::size_t idx = static_cast<std::size_t>(evt.encoder);
        encoder_deltas_[idx] += evt.encoder_delta;
        script_.pop_front();
        continue;
      }
    }
  }

  void writeButton(ButtonID id, bool pressed) {
    // Internal helper that mirrors a button press into the sampled state.  The
    // timestamp uses the simulator clock so lessons about debouncing still
    // apply even when everything is synthetic.
    const std::size_t idx = static_cast<std::size_t>(id);
    if (idx >= button_samples_.size()) {
      return;
    }
    // Same layout as the hardware board: `pressed` is level, timestamp is the
    // simulator's notion of "now".
    button_samples_[idx].pressed = pressed;
    button_samples_[idx].timestamp_us = now_us_;
  }

  std::deque<ScriptEvent> script_{};
  std::array<ButtonSample, 8> button_samples_{};
  std::array<int32_t, 4> encoder_deltas_{};
  std::uint64_t now_us_{0};
  std::uint32_t now_ms_{0};
  const std::uint32_t poll_period_us_{10000};
};

NativeBoard& instance() {
  // Classic Meyers singleton so the shim has a single source of truth.  Keeping
  // it local avoids global constructors while giving students a quick primer on
  // how singletons are usually built in C++.
  static NativeBoard board;
  return board;
}

}  // namespace

Board& board() { return instance(); }
Board& nativeBoard() { return instance(); }

void nativeBoardFeed(const std::string& line) {
  // External hook for the simulator CLI.  Feeds a single script line into the
  // queue so integration tests can choreograph button presses.
  instance().feed(line);
}
void nativeBoardReset() {
  // Public reset for labs: cleans slate between test runs so demos stay
  // deterministic.
  instance().reset();
}
void nativeBoardFastForwardMicros(std::uint64_t delta) {
  // Expose the fast-forward cheat to other modules without forcing them to
  // reach into the singleton directly.
  instance().fastForward(delta);
}

void nativeBoardSetButton(Board::ButtonID id, bool pressed) { instance().setButton(id, pressed); }

std::vector<std::string> nativeEnumerateControllers() {
  // Native desktop builds don't talk to real hardware yet, so this hook stays
  // intentionally sparse.  It still exists so the front panel can surface a
  // menu that mirrors the embedded UX and can be filled in once controller
  // discovery lands.
  return {};
}

}  // namespace hal

#endif  // !SEEDBOX_HW

