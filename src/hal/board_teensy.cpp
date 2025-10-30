#ifdef SEEDBOX_HW

#include "hal/Board.h"

#include <array>
#include <Arduino.h>
#include <Bounce2.h>

#include "hal_io.h"

namespace hal {

namespace {

constexpr std::uint8_t kButtonCount = 7;
constexpr std::uint8_t kEncoderCount = 4;
constexpr io::PinNumber kStatusLedPin = 13;

struct PinGroup {
  Board::ButtonID button_id;
  io::PinNumber pin_switch;
  io::PinNumber pin_a;
  io::PinNumber pin_b;
};

constexpr std::array<PinGroup, kEncoderCount> kEncoders{{
    {Board::ButtonID::EncoderSeedBank, 2, 0, 1},
    {Board::ButtonID::EncoderDensity, 5, 3, 4},
    {Board::ButtonID::EncoderToneTilt, 27, 24, 26},
    {Board::ButtonID::EncoderFxMutate, 30, 6, 9},
}};

constexpr std::array<std::pair<Board::ButtonID, io::PinNumber>, 3> kStandaloneButtons{{
    {Board::ButtonID::TapTempo, 31},
    {Board::ButtonID::Shift, 32},
    {Board::ButtonID::AltSeed, 33},
}};

constexpr std::array<io::DigitalConfig, kEncoderCount * 3 + kStandaloneButtons.size() + 1> buildPinConfig() {
  std::array<io::DigitalConfig, kEncoderCount * 3 + kStandaloneButtons.size() + 1> configs{};
  std::size_t idx = 0;
  for (const auto& group : kEncoders) {
    configs[idx++] = io::DigitalConfig{group.pin_switch, true, true};
    configs[idx++] = io::DigitalConfig{group.pin_a, true, true};
    configs[idx++] = io::DigitalConfig{group.pin_b, true, true};
  }
  for (const auto& btn : kStandaloneButtons) {
    configs[idx++] = io::DigitalConfig{btn.second, true, true};
  }
  configs[idx++] = io::DigitalConfig{kStatusLedPin, false, false};
  return configs;
}

constexpr auto kPinConfig = buildPinConfig();

class TeensyBoard final : public Board {
public:
  TeensyBoard() {
    io::init(kPinConfig.data(), kPinConfig.size());
    last_micros_tick_ = static_cast<std::uint32_t>(::micros());
    for (std::size_t i = 0; i < kEncoders.size(); ++i) {
      auto& bounce = encoder_switches_[i];
      bounce.attach(kEncoders[i].pin_switch, INPUT_PULLUP);
      bounce.interval(5);
      encoder_states_[i] = readEncoderRaw(static_cast<EncoderID>(i));
    }
    for (std::size_t i = 0; i < kStandaloneButtons.size(); ++i) {
      auto& bounce = buttons_[i];
      bounce.attach(kStandaloneButtons[i].second, INPUT_PULLUP);
      bounce.interval(5);
    }
  }

  void poll() override {
    io::poll();
    const auto rawMicros = static_cast<std::uint32_t>(::micros());
    const std::uint32_t delta = rawMicros - last_micros_tick_;
    last_micros_tick_ = rawMicros;
    advanceClock(delta);

    for (std::size_t i = 0; i < kEncoders.size(); ++i) {
      encoder_switches_[i].update();
      const std::uint8_t now = readEncoderRaw(static_cast<EncoderID>(i));
      const std::uint8_t last = encoder_states_[i];
      if (now != last) {
        const int8_t step = decodeQuadrature(last, now);
        if (step != 0) {
          encoder_deltas_[i] += step;
        }
        encoder_states_[i] = now;
      }
      button_samples_[static_cast<std::size_t>(kEncoders[i].button_id)] =
          buildSample(encoder_switches_[i].read() == LOW);
    }

    for (std::size_t i = 0; i < kStandaloneButtons.size(); ++i) {
      buttons_[i].update();
      button_samples_[static_cast<std::size_t>(kStandaloneButtons[i].first)] =
          buildSample(buttons_[i].read() == LOW);
    }
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

  std::uint32_t nowMillis() const override { return static_cast<std::uint32_t>(millis_accum_); }
  std::uint64_t nowMicros() const override { return micros_accum_; }

private:
  void advanceClock(std::uint32_t delta) {
    if (delta == 0) {
      return;
    }
    micros_accum_ += delta;
    const std::uint32_t accum = micros_residual_ + delta;
    millis_accum_ += accum / 1000u;
    micros_residual_ = accum % 1000u;
  }

  static std::uint8_t readEncoderRaw(EncoderID id) {
    const auto& pins = kEncoders[static_cast<std::size_t>(id)];
    const bool a = io::readDigital(pins.pin_a);
    const bool b = io::readDigital(pins.pin_b);
    return static_cast<std::uint8_t>((a ? 1 : 0) << 1 | (b ? 1 : 0));
  }

  static int8_t decodeQuadrature(std::uint8_t last, std::uint8_t now) {
    static constexpr int8_t kTable[4][4] = {
        {0, -1, +1, 0},
        {+1, 0, 0, -1},
        {-1, 0, 0, +1},
        {0, +1, -1, 0},
    };
    return kTable[last & 0x3][now & 0x3];
  }

  ButtonSample buildSample(bool pressed) const {
    ButtonSample s{};
    s.pressed = pressed;
    s.timestamp_us = micros_accum_;
    return s;
  }

  std::array<ButtonSample, kButtonCount> button_samples_{};
  std::array<int32_t, kEncoderCount> encoder_deltas_{};
  std::array<std::uint8_t, kEncoderCount> encoder_states_{};
  std::array<Bounce, kEncoderCount> encoder_switches_{};
  std::array<Bounce, kStandaloneButtons.size()> buttons_{};
  std::uint32_t last_micros_tick_{0};
  std::uint64_t micros_accum_{0};
  std::uint64_t millis_accum_{0};
  std::uint32_t micros_residual_{0};
};

TeensyBoard& instance() {
  static TeensyBoard board;
  return board;
}

}  // namespace

Board& board() { return instance(); }

}  // namespace hal

#endif  // SEEDBOX_HW

