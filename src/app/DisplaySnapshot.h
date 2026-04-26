#pragma once

//
// DisplaySnapshot
// ---------------
// Compact OLED/debug-frame text snapshot. AppState and the renderers share
// this struct so the display path can stay deterministic and testable.
namespace seedbox {

struct DisplaySnapshot {
  char title[17];
  char status[17];
  char metrics[17];
  char nuance[17];
};

}  // namespace seedbox
