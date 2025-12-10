#pragma once

//
// Build-time switches.
// --------------------
// One header to rule the build flags and teach their stories. This is the
// canonical crib sheet mirrored by the README's build flag table. Touch this
// file whenever you add a new `-D` toggle so hardware hackers, teachers, and CI
// all stay in sync.

// Hardware flag — mirrors the README row "SEEDBOX_HW". When enabled the build
// pulls in Teensy-only IO and assumes the real rig is attached.
#ifndef SEEDBOX_HW
#define SEEDBOX_HW 0
#endif

// Native simulation helper. Keeps hardware glue stubbed while letting us flex
// the DSP + scheduling logic on desktop builds.
#ifndef SEEDBOX_SIM
#define SEEDBOX_SIM 0
#endif

// JUCE desktop builds. This flag lets the audio/MIDI plumbing swap over to
// `juce::AudioDeviceManager` without dragging Arduino headers into the mix.
#ifndef SEEDBOX_JUCE
#define SEEDBOX_JUCE 0
#endif

// Quiet mode flips off serial/MIDI chatter so labs can run the firmware without
// hardware connected. See README "QUIET_MODE" entry for the longer tale.
#ifndef QUIET_MODE
#define QUIET_MODE 1
#endif

// Golden fixtures opt-in. When true, tests may write comparison blobs into
// `artifacts/` so reviewers can diff logs and renders. Documented in README as
// "ENABLE_GOLDEN".
#ifndef ENABLE_GOLDEN
#define ENABLE_GOLDEN 0
#endif

// Clock debug breadcrumbs. Future UI/clock toggles land here so labs can chase
// transport edge cases without rummaging through platformio.ini.
#ifndef SEEDBOX_DEBUG_CLOCK_SOURCE
#define SEEDBOX_DEBUG_CLOCK_SOURCE 0
#endif

// UI instrumentation placeholder. Wire new UI debug overlays into this switch
// so native + hardware builds share the same control surface story.
#ifndef SEEDBOX_DEBUG_UI
#define SEEDBOX_DEBUG_UI 0
#endif

#ifdef __cplusplus

namespace SeedBoxConfig {

constexpr bool kHardwareBuild = (SEEDBOX_HW != 0);
constexpr bool kSimBuild = (SEEDBOX_SIM != 0);
constexpr bool kJuceBuild = (SEEDBOX_JUCE != 0);
constexpr bool kQuietMode = (QUIET_MODE != 0);
constexpr bool kGoldenArtifacts = (ENABLE_GOLDEN != 0);
constexpr bool kClockDebug = (SEEDBOX_DEBUG_CLOCK_SOURCE != 0);
constexpr bool kUiDebug = (SEEDBOX_DEBUG_UI != 0);

static_assert(SEEDBOX_HW == 0 || SEEDBOX_HW == 1,
              "SEEDBOX_HW must be 0 or 1");
static_assert(SEEDBOX_SIM == 0 || SEEDBOX_SIM == 1,
              "SEEDBOX_SIM must be 0 or 1");
static_assert(SEEDBOX_JUCE == 0 || SEEDBOX_JUCE == 1,
              "SEEDBOX_JUCE must be 0 or 1");
static_assert(QUIET_MODE == 0 || QUIET_MODE == 1,
              "QUIET_MODE must be 0 or 1");
static_assert(ENABLE_GOLDEN == 0 || ENABLE_GOLDEN == 1,
              "ENABLE_GOLDEN must be 0 or 1");
static_assert(SEEDBOX_DEBUG_CLOCK_SOURCE == 0 ||
                  SEEDBOX_DEBUG_CLOCK_SOURCE == 1,
              "SEEDBOX_DEBUG_CLOCK_SOURCE must be 0 or 1");
static_assert(SEEDBOX_DEBUG_UI == 0 || SEEDBOX_DEBUG_UI == 1,
              "SEEDBOX_DEBUG_UI must be 0 or 1");
static_assert(!(kHardwareBuild && kSimBuild),
              "SEEDBOX_HW and SEEDBOX_SIM cannot both be enabled");
static_assert(!(kHardwareBuild && kJuceBuild),
              "SEEDBOX_HW and SEEDBOX_JUCE cannot both be enabled");
static_assert(!(kSimBuild && kJuceBuild),
              "SEEDBOX_SIM and SEEDBOX_JUCE cannot both be enabled at once");

struct FlagSummary {
    const char *name;
    bool enabled;
    const char *story;
};

inline constexpr FlagSummary kFlagMatrix[] = {
    {"SEEDBOX_HW", kHardwareBuild,
     "Hardware build. Talks to real IO (Teensy pins, codecs, storage)."},
    {"SEEDBOX_SIM", kSimBuild,
     "Native sim build. Stubs hardware so DSP can run on desktop."},
    {"SEEDBOX_JUCE", kJuceBuild,
     "JUCE desktop build. Talks to host audio/MIDI instead of Arduino headers."},
    {"QUIET_MODE", kQuietMode,
     "Mute extra logs + hardware callbacks. Keeps lab sessions polite."},
    {"ENABLE_GOLDEN", kGoldenArtifacts,
     "Emit regression fixtures into artifacts/ for review."},
    {"SEEDBOX_DEBUG_CLOCK_SOURCE", kClockDebug,
     "Serial prints for transport decisions. Trace MIDI clock hand-offs."},
    {"SEEDBOX_DEBUG_UI", kUiDebug,
     "Future UI instrumentation hook. Overlay debug glyphs when true."},
};

}  // namespace SeedBoxConfig

#else  // __cplusplus

#include <stdbool.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define SEEDBOX_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define SEEDBOX_STATIC_ASSERT(cond, msg) typedef char SeedBoxStaticAssert__[(cond) ? 1 : -1]
#endif

#define SeedBoxConfig_kHardwareBuild ((SEEDBOX_HW) != 0)
#define SeedBoxConfig_kSimBuild ((SEEDBOX_SIM) != 0)
#define SeedBoxConfig_kJuceBuild ((SEEDBOX_JUCE) != 0)
#define SeedBoxConfig_kQuietMode ((QUIET_MODE) != 0)
#define SeedBoxConfig_kGoldenArtifacts ((ENABLE_GOLDEN) != 0)
#define SeedBoxConfig_kClockDebug ((SEEDBOX_DEBUG_CLOCK_SOURCE) != 0)
#define SeedBoxConfig_kUiDebug ((SEEDBOX_DEBUG_UI) != 0)

SEEDBOX_STATIC_ASSERT(SEEDBOX_HW == 0 || SEEDBOX_HW == 1,
                      "SEEDBOX_HW must be 0 or 1");
SEEDBOX_STATIC_ASSERT(SEEDBOX_SIM == 0 || SEEDBOX_SIM == 1,
                      "SEEDBOX_SIM must be 0 or 1");
SEEDBOX_STATIC_ASSERT(SEEDBOX_JUCE == 0 || SEEDBOX_JUCE == 1,
                      "SEEDBOX_JUCE must be 0 or 1");
SEEDBOX_STATIC_ASSERT(QUIET_MODE == 0 || QUIET_MODE == 1,
                      "QUIET_MODE must be 0 or 1");
SEEDBOX_STATIC_ASSERT(ENABLE_GOLDEN == 0 || ENABLE_GOLDEN == 1,
                      "ENABLE_GOLDEN must be 0 or 1");
SEEDBOX_STATIC_ASSERT(SEEDBOX_DEBUG_CLOCK_SOURCE == 0 ||
                          SEEDBOX_DEBUG_CLOCK_SOURCE == 1,
                      "SEEDBOX_DEBUG_CLOCK_SOURCE must be 0 or 1");
SEEDBOX_STATIC_ASSERT(SEEDBOX_DEBUG_UI == 0 || SEEDBOX_DEBUG_UI == 1,
                      "SEEDBOX_DEBUG_UI must be 0 or 1");
SEEDBOX_STATIC_ASSERT(!((SEEDBOX_HW != 0) && (SEEDBOX_SIM != 0)),
                      "SEEDBOX_HW and SEEDBOX_SIM cannot both be enabled");
SEEDBOX_STATIC_ASSERT(!((SEEDBOX_HW != 0) && (SEEDBOX_JUCE != 0)),
                      "SEEDBOX_HW and SEEDBOX_JUCE cannot both be enabled");
SEEDBOX_STATIC_ASSERT(!((SEEDBOX_SIM != 0) && (SEEDBOX_JUCE != 0)),
                      "SEEDBOX_SIM and SEEDBOX_JUCE cannot both be enabled");

typedef struct {
    const char *name;
    bool enabled;
    const char *story;
} SeedBoxConfig_FlagSummary;

static const SeedBoxConfig_FlagSummary SeedBoxConfig_kFlagMatrix[] = {
    {"SEEDBOX_HW", SeedBoxConfig_kHardwareBuild,
     "Hardware build. Talks to real IO (Teensy pins, codecs, storage)."},
    {"SEEDBOX_SIM", SeedBoxConfig_kSimBuild,
     "Native sim build. Stubs hardware so DSP can run on desktop."},
    {"SEEDBOX_JUCE", SeedBoxConfig_kJuceBuild,
     "JUCE desktop build. Talks to host audio/MIDI instead of Arduino headers."},
    {"QUIET_MODE", SeedBoxConfig_kQuietMode,
     "Mute extra logs + hardware callbacks. Keeps lab sessions polite."},
    {"ENABLE_GOLDEN", SeedBoxConfig_kGoldenArtifacts,
     "Emit regression fixtures into artifacts/ for review."},
    {"SEEDBOX_DEBUG_CLOCK_SOURCE", SeedBoxConfig_kClockDebug,
     "Serial prints for transport decisions. Trace MIDI clock hand-offs."},
    {"SEEDBOX_DEBUG_UI", SeedBoxConfig_kUiDebug,
     "Future UI instrumentation hook. Overlay debug glyphs when true."},
};

#undef SEEDBOX_STATIC_ASSERT

#endif  // __cplusplus
