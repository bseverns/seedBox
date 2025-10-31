#pragma once

#include "app/AppState.h"
#include "app/UiState.h"
#include "ui/TextFrame.h"

namespace ui {

#ifdef SEEDBOX_HW
class Adafruit_SH1107;
#endif

class OledView {
public:
#ifdef SEEDBOX_HW
  explicit OledView(Adafruit_SH1107& display);
#else
  OledView() = default;
#endif
  void init();
  void present(const AppState::DisplaySnapshot& snapshot, const UiState& state);
  void tick();

private:
#ifdef SEEDBOX_HW
  void flush();

  Adafruit_SH1107* display_{nullptr};
  TextFrame pending_{};
  TextFrame rendered_{};
  bool dirty_{false};
  std::uint32_t lastFlushUs_{0};
#endif
};

}  // namespace ui

