#include "app/DisplaySnapshotBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace {
template <std::size_t N, typename... Args>
std::string_view formatScratch(std::array<char, N>& scratch, const char* fmt, Args&&... args) {
  const int written = std::snprintf(scratch.data(), scratch.size(), fmt, std::forward<Args>(args)...);
  if (written < 0) {
    scratch[0] = '\0';
    return std::string_view{scratch.data()};
  }
  const std::size_t len = std::min<std::size_t>(static_cast<std::size_t>(written), scratch.size() - 1u);
  scratch[len] = '\0';
  return std::string_view{scratch.data(), len};
}

template <std::size_t N>
void writeDisplayField(char (&dst)[N], std::string_view text) {
  const std::size_t n = std::min<std::size_t>(N - 1u, text.size());
  std::memcpy(dst, text.data(), n);
  dst[n] = '\0';
  for (std::size_t i = n + 1; i < N; ++i) {
    dst[i] = '\0';
  }
}

template <std::size_t N>
void writeUiField(std::array<char, N>& dst, std::string_view text) {
  const std::size_t n = std::min<std::size_t>(N - 1u, text.size());
  std::memcpy(dst.data(), text.data(), n);
  dst[n] = '\0';
  for (std::size_t i = n + 1; i < N; ++i) {
    dst[i] = '\0';
  }
}

bool hasSeedContent(const std::vector<Seed>* seeds) {
  if (!seeds) {
    return false;
  }
  return std::any_of(seeds->begin(), seeds->end(), [](const Seed& s) { return s.prng != 0; });
}

}  // namespace

void DisplaySnapshotBuilder::build(seedbox::DisplaySnapshot& out, UiState& uiOut, const Input& input) const {
  std::array<char, 64> scratch{};
  writeDisplayField(out.title, formatScratch(scratch, "SeedBox %06X", input.masterSeed & 0xFFFFFFu));

  const bool seedsPresent = hasSeedContent(input.seeds);
  const std::size_t seedCount = input.seeds ? input.seeds->size() : 0u;
  std::size_t focusIndex = 0;
  if (seedsPresent && seedCount > 0u) {
    focusIndex = std::min<std::size_t>(input.focusSeed, seedCount - 1u);
  }
  const bool globalLocked = input.seedLock ? input.seedLock->globalLocked() : false;
  const bool focusLocked = (input.seedLock && seedsPresent && focusIndex < seedCount)
                               ? input.seedLock->seedLocked(focusIndex)
                               : false;
  const bool anyLockActive = globalLocked || focusLocked;
  const char* gateLabel = gateDivisionLabel(input.gateDivision);
  const char* primeMode = primeModeLabel(input.seedPrimeMode);

  uiOut.mode = UiState::Mode::kPerformance;
  if (input.mode == 6u) {
    uiOut.mode = UiState::Mode::kEdit;
  } else if (anyLockActive) {
    uiOut.mode = UiState::Mode::kEdit;
  }
  if (input.debugMetersEnabled) {
    uiOut.mode = UiState::Mode::kSystem;
  }
  uiOut.bpm = input.bpm;
  uiOut.swing = input.swing;
  uiOut.clock = input.externalClockDominant ? UiState::ClockSource::kExternal : UiState::ClockSource::kInternal;
  uiOut.seedLocked = anyLockActive;

  if (seedsPresent && input.engines) {
    const Seed& s = (*input.seeds)[focusIndex];
    writeUiField(uiOut.engineName, engineLongName(s.engine));
  } else {
    writeUiField(uiOut.engineName, "Idle");
  }

  if (input.mode == 6u) {
    writeUiField(uiOut.pageHints[0], "Tap: exit swing");
    writeUiField(uiOut.pageHints[1], "Seed:5% Den:1%");
  } else if (input.mode == 4u) {
    writeUiField(uiOut.pageHints[0],
                 formatScratch(scratch, "Alt:prime %s", input.seedPrimeBypassEnabled ? "skip" : "fill"));
    writeUiField(uiOut.pageHints[1],
                 formatScratch(scratch, "Tap:%s clock", input.followExternalClockEnabled ? "ext" : "int"));
  } else if (input.currentPage == 1u) {
    writeUiField(uiOut.pageHints[0], "GPIO: recall");
    writeUiField(uiOut.pageHints[1], "Hold GPIO: save");
  } else if (globalLocked) {
    writeUiField(uiOut.pageHints[0], "Pg seeds locked");
    writeUiField(uiOut.pageHints[1], "Pg+Md: unlock all");
  } else if (focusLocked) {
    writeUiField(uiOut.pageHints[0], "Pg focus locked");
    writeUiField(uiOut.pageHints[1], "Pg+Md: unlock");
  } else if (input.mode == 2u && seedsPresent) {
    switch ((*input.seeds)[focusIndex].engine) {
      case EngineRouter::kEuclidId:
        writeUiField(uiOut.pageHints[0], "Den:steps Fx:rot");
        writeUiField(uiOut.pageHints[1], "Tone:fills");
        break;
      case EngineRouter::kBurstId:
        writeUiField(uiOut.pageHints[0], "Den:clusters");
        writeUiField(uiOut.pageHints[1], "Tone:spacing");
        break;
      default: {
        writeUiField(uiOut.pageHints[0], "Tone S:src ALT:d");
        const auto primeHint = formatScratch(scratch, "Tap:%s G%s", primeMode, gateLabel);
        writeUiField(uiOut.pageHints[1], primeHint);
        break;
      }
    }
  } else {
    writeUiField(uiOut.pageHints[0], "Gate:Shift+Den");
    const auto primeHint = formatScratch(scratch, "Tap:%s G%s", primeMode, gateLabel);
    writeUiField(uiOut.pageHints[1], primeHint);
  }

  if (!seedsPresent) {
    const char* mood = input.quietMode ? "quiet" : "empty";
    if (input.seedPrimeBypassEnabled) {
      mood = "bypass";
    }
    if (input.waitingForExternalClock) {
      writeDisplayField(out.status, "WAIT EXT CLK");
    } else {
      writeDisplayField(out.status, formatScratch(scratch, "%s %s", modeLabel(input.mode), mood));
    }
    writeDisplayField(out.metrics, formatScratch(scratch, "SR%.1fkB%02zu", input.sampleRate / 1000.f, input.framesPerBlock));
    writeDisplayField(out.nuance,
                      formatScratch(scratch, "AC%05lluF%05lu",
                                     static_cast<unsigned long long>(input.audioCallbackCount % 100000ULL),
                                     static_cast<unsigned long>(input.frame % 100000ULL)));
    return;
  }

  const Seed& s = (*input.seeds)[focusIndex];
  const std::string_view shortName = input.engines ? engineLabel(*input.engines, s.engine) : std::string_view{"UNK"};
  if (input.waitingForExternalClock) {
    writeDisplayField(out.status, "WAIT EXT CLK");
  } else {
    writeDisplayField(out.status,
                      formatScratch(scratch, "#%02u%.*s%+0.1fst%c", s.id, static_cast<int>(shortName.size()),
                                     shortName.data(), s.pitch, input.ledOn ? '*' : '-'));
  }
  const float density = std::clamp(s.density, 0.0f, 99.99f);
  const float probability = std::clamp(s.probability, 0.0f, 1.0f);
  const Seed* schedulerSeed = input.schedulerSeed;
  const unsigned prngByte = schedulerSeed ? static_cast<unsigned>(schedulerSeed->prng & 0xFFu) : 0u;
  const char gateState = input.gateEdgePending ? '^' : (input.inputGateHot ? '!' : '-');

  if (input.mode == 3u) {
    const auto stats = input.granularStats ? *input.granularStats : GranularEngine::Stats{};
    const unsigned grains = static_cast<unsigned>(stats.grainsPlanned % 1000u);
    const std::size_t lastBin = GranularEngine::Stats::kHistogramBins - 1;
    const unsigned sizeLow = static_cast<unsigned>(stats.grainSizeHistogram[0] + stats.grainSizeHistogram[1]);
    const unsigned sizeHigh = static_cast<unsigned>(stats.grainSizeHistogram[lastBin]);
    const unsigned sprayLow = static_cast<unsigned>(stats.sprayHistogram[0] + stats.sprayHistogram[1]);
    const unsigned sprayHigh = static_cast<unsigned>(stats.sprayHistogram[lastBin]);
    writeDisplayField(out.metrics,
                      formatScratch(scratch, "GV%02u SD%02u GP%03u", stats.activeVoiceCount, stats.sdOnlyVoiceCount,
                                     grains));
    writeDisplayField(out.nuance,
                      formatScratch(scratch, "S%02u|%02uP%02u|%02uF%u%u", sizeLow, sizeHigh, sprayLow, sprayHigh,
                                     static_cast<unsigned>(stats.busiestMixerLoad),
                                     static_cast<unsigned>(stats.mixerGroupsEngaged)));
    return;
  }

  if (input.debugMetersEnabled) {
    float fanout = 0.0f;
    if (s.engine == EngineRouter::kResonatorId && input.engines) {
      fanout = input.engines->resonator().fanoutProbeLevel();
    }
    writeDisplayField(out.metrics,
                      formatScratch(scratch, "D%.1fP%.1fF%.1f%c", density, probability, fanout, gateState));
  } else {
    writeDisplayField(out.metrics,
                      formatScratch(scratch, "D%.1fP%.1fG%s%c", density, probability, gateLabel, gateState));
  }

  const float mutate = std::clamp(s.mutateAmt, 0.0f, 1.0f);
  const float jitterMs = std::clamp(s.jitterMs, 0.0f, 999.9f);
  const unsigned jitterInt = static_cast<unsigned>(std::min(99.0f, std::round(jitterMs)));
  char engineToken[8] = {'-', '-', '-', '-', '\0'};

  if (!input.engines) {
    std::snprintf(engineToken, sizeof(engineToken), "?%03u", static_cast<unsigned>(s.engine % 1000));
  } else {
    switch (s.engine) {
      case 0: {
        const auto voice = input.engines->sampler().voice(static_cast<uint8_t>(focusIndex % Sampler::kMaxVoices));
        std::snprintf(engineToken, sizeof(engineToken), "%c%c%02u", voice.active ? 'S' : 's',
                      voice.usesSdStreaming ? 'D' : 'M', voice.sampleIndex);
        break;
      }
      case 1: {
        const auto voice = input.engines->granular().voice(static_cast<uint8_t>(focusIndex % GranularEngine::kVoicePoolSize));
        GranularEngine::Source seedSource = static_cast<GranularEngine::Source>(s.granular.source);
        if (seedSource != GranularEngine::Source::kSdClip) {
          seedSource = GranularEngine::Source::kLiveInput;
        }
        uint8_t sdSlot = s.granular.sdSlot;
        if (GranularEngine::kSdClipSlots > 0) {
          sdSlot = static_cast<uint8_t>(sdSlot % GranularEngine::kSdClipSlots);
        }
        if (seedSource == GranularEngine::Source::kSdClip && GranularEngine::kSdClipSlots > 1 && sdSlot == 0) {
          sdSlot = 1;
        }
        const bool voiceActive = voice.active && voice.seedId == s.id;
        char sourceTag = (seedSource == GranularEngine::Source::kLiveInput) ? 'L' : 'C';
        if (voiceActive && voice.source == seedSource) {
          if (seedSource == GranularEngine::Source::kSdClip && GranularEngine::kSdClipSlots > 0) {
            sdSlot = static_cast<uint8_t>(voice.sdSlot % GranularEngine::kSdClipSlots);
          }
        }
        std::snprintf(engineToken, sizeof(engineToken), "%c%c%02u", voiceActive ? 'G' : 'g', sourceTag, sdSlot);
        break;
      }
      case 2: {
        const auto voice = input.engines->resonator().voice(static_cast<uint8_t>(focusIndex % ResonatorBank::kMaxVoices));
        const char* preset = input.engines->resonator().presetName(voice.bank);
        char presetA = '-';
        char presetB = '-';
        if (preset && preset[0] != '\0') {
          presetA = preset[0];
          if (preset[1] != '\0') {
            presetB = preset[1];
          }
        }
        const uint8_t modeDigit = static_cast<uint8_t>(std::min<uint8_t>(voice.mode, 9));
        std::snprintf(engineToken, sizeof(engineToken), "%c%c%c%c", voice.active ? 'R' : 'r', presetA, presetB,
                      static_cast<char>('0' + modeDigit));
        break;
      }
      default:
        std::snprintf(engineToken, sizeof(engineToken), "?%03u", static_cast<unsigned>(s.engine % 1000));
        break;
    }
  }

  writeDisplayField(out.nuance,
                    formatScratch(scratch, "Mu%.2f%sR%02XJ%02u", mutate, engineToken, prngByte, jitterInt));
}

const char* DisplaySnapshotBuilder::modeLabel(std::uint8_t mode) {
  switch (mode) {
    case 0: return "HOME";
    case 1: return "SEEDS";
    case 2: return "ENGINE";
    case 3: return "PERF";
    case 4: return "SET";
    case 5: return "UTIL";
    case 6: return "SWING";
    default: return "?";
  }
}

const char* DisplaySnapshotBuilder::gateDivisionLabel(std::uint8_t division) {
  switch (division) {
    case 1: return "1/2";
    case 2: return "1/4";
    case 3: return "BAR";
    case 0:
    default: return "1/1";
  }
}

const char* DisplaySnapshotBuilder::primeModeLabel(std::uint8_t primeMode) {
  switch (primeMode) {
    case 1: return "Tap";
    case 2: return "Preset";
    case 3: return "Live";
    case 0:
    default: return "LFSR";
  }
}

const char* DisplaySnapshotBuilder::engineLongName(std::uint8_t engine) {
  switch (engine) {
    case 0: return "Sampler";
    case 1: return "Granular";
    case 2: return "Resonator";
    case 3: return "Euclid";
    case 4: return "Burst";
    case 5: return "Toy";
    default: return "Unknown";
  }
}

std::string_view DisplaySnapshotBuilder::engineLabel(const EngineRouter& router, std::uint8_t engine) {
  const std::uint8_t sanitized = router.sanitizeEngineId(engine);
  const std::string_view label = router.engineShortName(sanitized);
  if (label.empty()) {
    return std::string_view{"UNK"};
  }
  return label;
}
