#pragma once

#ifndef QUIET_MODE
#define QUIET_MODE 0
#endif

namespace seedbox {

constexpr bool quietModeEnabled() {
  return QUIET_MODE != 0;
}

}  // namespace seedbox
