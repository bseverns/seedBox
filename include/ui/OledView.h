#pragma once

#include "app/AppState.h"
#include "app/UiState.h"
#include "ui/TextFrame.h"

#if SEEDBOX_HW
#include <Adafruit_SH110X.h>
#endif

namespace ui {

class OledView {
public:
#if SEEDBOX_HW
  explicit OledView(Adafruit_SH1107& display);
#else
  OledView() = default;
#endif
  void init();
  void present(const AppState::DisplaySnapshot& snapshot, const UiState& state);
  void tick();

private:
#if SEEDBOX_HW
  void flush();

  Adafruit_SH1107* display_{nullptr};
  TextFrame pending_{};
  TextFrame rendered_{};
  bool dirty_{false};
  std::uint32_t lastFlushUs_{0};
#endif
};

}  // namespace ui

