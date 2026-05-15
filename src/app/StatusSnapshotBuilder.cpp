#include "app/StatusSnapshotBuilder.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace {
constexpr std::string_view kDefaultPresetSlot = "default";

std::size_t boundedCStringLength(const char* value, std::size_t maxLen) {
  if (!value) {
    return 0;
  }
  std::size_t len = 0;
  while (len < maxLen && value[len] != '\0') {
    ++len;
  }
  return len;
}

template <std::size_t N>
void writeField(char (&dst)[N], std::string_view text) {
  static_assert(N > 0, "Field must have space for a terminator");
  const std::size_t copyLen = std::min<std::size_t>(text.size(), N - 1u);
  if (copyLen > 0) {
    std::memcpy(dst, text.data(), copyLen);
  }
  dst[copyLen] = '\0';
  for (std::size_t i = copyLen + 1; i < N; ++i) {
    dst[i] = '\0';
  }
}

void appendJsonEscaped(std::string& out, std::string_view value) {
  constexpr char kHex[] = "0123456789ABCDEF";
  for (const char ch : value) {
    switch (ch) {
      case '\"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default: {
        const auto code = static_cast<unsigned char>(ch);
        if (code < 0x20u) {
          out += "\\u00";
          out.push_back(kHex[(code >> 4u) & 0x0Fu]);
          out.push_back(kHex[code & 0x0Fu]);
        } else {
          out.push_back(ch);
        }
        break;
      }
    }
  }
}

void appendJsonStringField(std::string& out, std::string_view key, std::string_view value, bool trailingComma) {
  out.push_back('"');
  out.append(key.data(), key.size());
  out += "\":\"";
  appendJsonEscaped(out, value);
  out.push_back('"');
  if (trailingComma) {
    out.push_back(',');
  }
}

void appendJsonBoolField(std::string& out, std::string_view key, bool value, bool trailingComma) {
  out.push_back('"');
  out.append(key.data(), key.size());
  out += "\":";
  out += value ? "true" : "false";
  if (trailingComma) {
    out.push_back(',');
  }
}

void appendJsonUIntField(std::string& out, std::string_view key, std::uint64_t value, bool trailingComma) {
  out.push_back('"');
  out.append(key.data(), key.size());
  out += "\":";
  out += std::to_string(value);
  if (trailingComma) {
    out.push_back(',');
  }
}

void appendJsonFloatField(std::string& out, std::string_view key, float value, bool trailingComma) {
  std::array<char, 32> scratch{};
  std::snprintf(scratch.data(), scratch.size(), "%.3f", static_cast<double>(value));

  out.push_back('"');
  out.append(key.data(), key.size());
  out += "\":";
  out.append(scratch.data());
  if (trailingComma) {
    out.push_back(',');
  }
}
}  // namespace

void StatusSnapshotBuilder::build(seedbox::StatusSnapshot& out, const Input& input) const {
  out = seedbox::StatusSnapshot{};
  writeField(out.mode, input.mode);
  writeField(out.page, input.page);
  out.masterSeed = input.masterSeed;
  out.activePresetId = input.activePresetId;
  writeField(out.activePresetSlot, input.activePresetSlot.empty() ? kDefaultPresetSlot : input.activePresetSlot);
  out.bpm = input.bpm;
  out.schedulerTick = input.schedulerTick;
  out.hostDiagnostics = input.hostDiagnostics;
  out.externalClockDominant = input.externalClockDominant;
  out.followExternalClockEnabled = input.followExternalClockEnabled;
  out.waitingForExternalClock = input.waitingForExternalClock;
  out.quietMode = input.quietMode;
  out.globalSeedLocked = input.seedLock ? input.seedLock->globalLocked() : false;

  const std::size_t seedCount = input.seeds ? input.seeds->size() : 0u;
  if (seedCount == 0u) {
    out.hasFocusedSeed = false;
    out.focusSeedIndex = input.focusSeed;
    out.focusSeedId = 0;
    out.focusSeedEngineId = EngineRouter::kSamplerId;
    writeField(out.focusSeedEngineName, "None");
    out.focusSeedLocked = false;
    return;
  }

  const std::size_t focusIndex = std::min<std::size_t>(input.focusSeed, seedCount - 1u);
  const Seed& focus = (*input.seeds)[focusIndex];
  out.hasFocusedSeed = true;
  out.focusSeedIndex = static_cast<std::uint8_t>(focusIndex);
  out.focusSeedId = focus.id;
  out.focusSeedEngineId = input.engines ? input.engines->sanitizeEngineId(focus.engine) : focus.engine;
  writeField(out.focusSeedEngineName, engineLongName(out.focusSeedEngineId));
  out.focusSeedLocked = input.seedLock ? input.seedLock->seedLocked(focusIndex) : false;
}

std::string StatusSnapshotBuilder::toJson(const seedbox::StatusSnapshot& status) const {
  const auto mode = std::string_view(status.mode, boundedCStringLength(status.mode, sizeof(status.mode)));
  const auto page = std::string_view(status.page, boundedCStringLength(status.page, sizeof(status.page)));
  const auto slot =
      std::string_view(status.activePresetSlot, boundedCStringLength(status.activePresetSlot, sizeof(status.activePresetSlot)));
  const auto engineName = std::string_view(status.focusSeedEngineName,
                                           boundedCStringLength(status.focusSeedEngineName, sizeof(status.focusSeedEngineName)));

  std::string out;
  out.reserve(512);
  out.push_back('{');
  appendJsonStringField(out, "mode", mode, true);
  appendJsonStringField(out, "page", page, true);
  appendJsonUIntField(out, "masterSeed", status.masterSeed, true);
  appendJsonUIntField(out, "activePresetId", status.activePresetId, true);
  appendJsonStringField(out, "activePresetSlot", slot, true);
  appendJsonFloatField(out, "bpm", status.bpm, true);
  appendJsonUIntField(out, "schedulerTick", status.schedulerTick, true);
  out += "\"hostDiagnostics\":{";
  appendJsonUIntField(out, "midiDroppedCount", status.hostDiagnostics.midiDroppedCount, true);
  appendJsonUIntField(out, "oversizeBlockDropCount", status.hostDiagnostics.oversizeBlockDropCount, true);
  appendJsonUIntField(out, "lastOversizeBlockFrames", status.hostDiagnostics.lastOversizeBlockFrames, true);
  appendJsonUIntField(out, "preparedScratchFrames", status.hostDiagnostics.preparedScratchFrames, false);
  out += "},";
  appendJsonBoolField(out, "externalClockDominant", status.externalClockDominant, true);
  appendJsonBoolField(out, "followExternalClockEnabled", status.followExternalClockEnabled, true);
  appendJsonBoolField(out, "waitingForExternalClock", status.waitingForExternalClock, true);
  appendJsonBoolField(out, "quietMode", status.quietMode, true);
  appendJsonBoolField(out, "globalSeedLocked", status.globalSeedLocked, true);
  appendJsonBoolField(out, "focusSeedLocked", status.focusSeedLocked, true);
  out += "\"focusSeed\":{";
  appendJsonBoolField(out, "present", status.hasFocusedSeed, true);
  appendJsonUIntField(out, "index", status.focusSeedIndex, true);
  appendJsonUIntField(out, "id", status.focusSeedId, true);
  appendJsonUIntField(out, "engineId", status.focusSeedEngineId, true);
  appendJsonStringField(out, "engineName", engineName, false);
  out += "}}";
  return out;
}

const char* StatusSnapshotBuilder::engineLongName(std::uint8_t engine) {
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
