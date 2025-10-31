#include "ui/OledView.h"

#ifdef SEEDBOX_HW
#include <Adafruit_SH110X.h>
#include <Arduino.h>
#include <algorithm>
#include "SeedBoxConfig.h"

namespace {
constexpr std::uint32_t kFlushIntervalUs = 1500;
constexpr int16_t kLineHeight = 10;
constexpr int16_t kStatusInsetY = 1;
}

namespace ui {

OledView::OledView(Adafruit_SH1107& display) : display_(&display) {}

void OledView::init() {
  if (!display_) {
    return;
  }
  display_->clearDisplay();
  display_->setRotation(0);
  display_->setTextSize(1);
  display_->setTextWrap(false);
  display_->setTextColor(SH110X_WHITE);
  display_->display();
  rendered_.lineCount = 0;
  pending_.lineCount = 0;
  dirty_ = false;
  lastFlushUs_ = micros();
}

void OledView::present(const AppState::DisplaySnapshot& snapshot, const UiState& state) {
  if (!display_) {
    return;
  }
  const TextFrame frame = ComposeTextFrame(snapshot, state);
  if (frame != pending_) {
    pending_ = frame;
    dirty_ = (pending_ != rendered_);
  }
}

void OledView::tick() {
  if (!display_ || !dirty_) {
    return;
  }
  const std::uint32_t now = micros();
  if (now - lastFlushUs_ < kFlushIntervalUs) {
    return;
  }
  flush();
}

void OledView::flush() {
  if (!display_ || !dirty_) {
    return;
  }

  display_->clearDisplay();
  display_->setTextSize(1);
  display_->setTextWrap(false);

  int16_t y = 0;
  for (std::size_t i = 0; i < pending_.lineCount; ++i) {
    const bool statusLine = (i == 0);
    const bool quietBanner = SeedBoxConfig::kQuietMode && (i == 1);
    if (statusLine) {
      display_->fillRect(0, 0, display_->width(), kLineHeight, SH110X_WHITE);
      display_->setTextColor(SH110X_BLACK, SH110X_WHITE);
      display_->setCursor(0, kStatusInsetY);
    } else {
      if (quietBanner) {
        display_->fillRect(0, y, display_->width(), kLineHeight, SH110X_BLACK);
        display_->setTextColor(SH110X_WHITE, SH110X_BLACK);
      } else {
        display_->setTextColor(SH110X_WHITE);
      }
      display_->setCursor(0, y);
    }
    display_->print(pending_.lines[i].data());
    y = static_cast<int16_t>(y + kLineHeight);
  }
  display_->setTextColor(SH110X_WHITE);
  display_->display();

  rendered_ = pending_;
  dirty_ = false;
  lastFlushUs_ = micros();
}

}  // namespace ui

#else  // !SEEDBOX_HW

namespace ui {

void OledView::init() {}
void OledView::present(const AppState::DisplaySnapshot&, const UiState&) {}
void OledView::tick() {}

}  // namespace ui

#endif  // SEEDBOX_HW

