#pragma once

// Central configuration for build-time switches. QUIET_MODE defaults to on so the
// firmware boots into a non-destructive sandbox unless a builder explicitly opts
// into noisy behaviour (hardware IO, seed persistence, etc.).
#ifndef QUIET_MODE
#define QUIET_MODE 1
#endif

namespace SeedBoxConfig {
constexpr bool kQuietMode = (QUIET_MODE != 0);
}
