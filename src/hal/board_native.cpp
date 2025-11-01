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

struct ScriptEvent {
  enum class Type { Wait, Button, Encoder } type{Type::Wait};
  std::uint64_t duration_us{0};
  Board::ButtonID button{Board::ButtonID::TapTempo};
  bool pressed{false};
  Board::EncoderID encoder{Board::EncoderID::SeedBank};
  int32_t encoder_delta{0};
};

constexpr std::array<std::pair<const char*, Board::ButtonID>, 7> kButtonLookup{{
    {"seed", Board::ButtonID::EncoderSeedBank},
    {"density", Board::ButtonID::EncoderDensity},
    {"tone", Board::ButtonID::EncoderToneTilt},
    {"fx", Board::ButtonID::EncoderFxMutate},
    {"tap", Board::ButtonID::TapTempo},
    {"shift", Board::ButtonID::Shift},
    {"alt", Board::ButtonID::AltSeed},
}};

constexpr std::array<std::pair<const char*, Board::EncoderID>, 4> kEncoderLookup{{
    {"seed", Board::EncoderID::SeedBank},
    {"density", Board::EncoderID::Density},
    {"tone", Board::EncoderID::ToneTilt},
    {"fx", Board::EncoderID::FxMutate},
}};

std::string toLower(std::string_view in) {
  std::string out(in);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

class NativeBoard final : public Board {
public:
  void poll() override {
    now_us_ += poll_period_us_;
    now_ms_ = static_cast<std::uint32_t>(now_us_ / 1000u);

    processScript();
  }

  ButtonSample sampleButton(ButtonID id) const override {
    const std::size_t idx = static_cast<std::size_t>(id);
    if (idx >= button_samples_.size()) {
      return ButtonSample{};
    }
    return button_samples_[idx];
  }

  int32_t consumeEncoderDelta(EncoderID id) override {
    const std::size_t idx = static_cast<std::size_t>(id);
    if (idx >= encoder_deltas_.size()) {
      return 0;
    }
    const int32_t delta = encoder_deltas_[idx];
    encoder_deltas_[idx] = 0;
    return delta;
  }

  bool tapTempoActive() const override {
    return button_samples_[static_cast<std::size_t>(ButtonID::TapTempo)].pressed;
  }

  std::uint32_t nowMillis() const override { return now_ms_; }
  std::uint64_t nowMicros() const override { return now_us_; }

  void reset() {
    script_.clear();
    std::fill(button_samples_.begin(), button_samples_.end(), ButtonSample{});
    std::fill(encoder_deltas_.begin(), encoder_deltas_.end(), 0);
    now_us_ = 0;
    now_ms_ = 0;
  }

  void feed(std::string_view line) {
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

  void fastForward(std::uint64_t micros) {
    now_us_ += micros;
    now_ms_ = static_cast<std::uint32_t>(now_us_ / 1000u);
  }

private:
  static std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
      text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
      text.remove_suffix(1);
    }
    return text;
  }

  static std::optional<Board::ButtonID> parseButton(std::string_view token) {
    const std::string key = toLower(token);
    for (const auto& entry : kButtonLookup) {
      if (key == entry.first) {
        return entry.second;
      }
    }
    return std::nullopt;
  }

  static std::optional<Board::EncoderID> parseEncoder(std::string_view token) {
    const std::string key = toLower(token);
    for (const auto& entry : kEncoderLookup) {
      if (key == entry.first) {
        return entry.second;
      }
    }
    return std::nullopt;
  }

  void processScript() {
    while (!script_.empty()) {
      auto& evt = script_.front();
      if (evt.type == ScriptEvent::Type::Wait) {
        if (evt.duration_us > poll_period_us_) {
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
        writeButton(evt.button, evt.pressed);
        script_.pop_front();
        continue;
      }

      if (evt.type == ScriptEvent::Type::Encoder) {
        const std::size_t idx = static_cast<std::size_t>(evt.encoder);
        encoder_deltas_[idx] += evt.encoder_delta;
        script_.pop_front();
        continue;
      }
    }
  }

  void writeButton(ButtonID id, bool pressed) {
    const std::size_t idx = static_cast<std::size_t>(id);
    if (idx >= button_samples_.size()) {
      return;
    }
    button_samples_[idx].pressed = pressed;
    button_samples_[idx].timestamp_us = now_us_;
  }

  std::deque<ScriptEvent> script_{};
  std::array<ButtonSample, 7> button_samples_{};
  std::array<int32_t, 4> encoder_deltas_{};
  std::uint64_t now_us_{0};
  std::uint32_t now_ms_{0};
  const std::uint32_t poll_period_us_{10000};
};

NativeBoard& instance() {
    static NativeBoard board;
    return board;
}

}  // namespace

Board& board() { return instance(); }
Board& nativeBoard() { return instance(); }

void nativeBoardFeed(const std::string& line) { instance().feed(line); }
void nativeBoardReset() { instance().reset(); }
void nativeBoardFastForwardMicros(std::uint64_t delta) { instance().fastForward(delta); }

}  // namespace hal

#endif  // !SEEDBOX_HW

