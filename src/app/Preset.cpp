#include "app/Preset.h"
#include <algorithm>
#include <string>
#include <string_view>
#include "engine/Granular.h"

#include <ArduinoJson.h>

namespace {

template <typename T>
T clampValue(T v, T lo, T hi) {
  return std::max(lo, std::min(hi, v));
}

}  // namespace

namespace seedbox {

std::vector<std::uint8_t> Preset::serialize() const {
  JsonDocument doc;
  doc["slot"] = slot;
  doc["masterSeed"] = masterSeed;
  doc["focusSeed"] = focusSeed;
  JsonObject clockObj = doc["clock"].to<JsonObject>();
  clockObj["bpm"] = clock.bpm;
  clockObj["followExternal"] = clock.followExternal;
  clockObj["debugMeters"] = clock.debugMeters;
  clockObj["transportLatch"] = clock.transportLatch;
  doc["page"] = static_cast<std::uint8_t>(page);

  JsonArray enginesArr = doc["engineSelections"].to<JsonArray>();
  for (std::uint8_t v : engineSelections) {
    enginesArr.add(v);
  }

  JsonArray seedsArr = doc["seeds"].to<JsonArray>();
  for (const Seed& s : seeds) {
    JsonObject seedObj = seedsArr.add<JsonObject>();
    seedObj["id"] = s.id;
    seedObj["prng"] = s.prng;
    seedObj["pitch"] = s.pitch;
    seedObj["envA"] = s.envA;
    seedObj["envD"] = s.envD;
    seedObj["envS"] = s.envS;
    seedObj["envR"] = s.envR;
    seedObj["density"] = s.density;
    seedObj["probability"] = s.probability;
    seedObj["jitterMs"] = s.jitterMs;
    seedObj["tone"] = s.tone;
    seedObj["spread"] = s.spread;
    seedObj["engine"] = s.engine;
    seedObj["sampleIdx"] = s.sampleIdx;
    seedObj["mutateAmt"] = s.mutateAmt;

    JsonObject granularObj = seedObj["granular"].to<JsonObject>();
    granularObj["grainSizeMs"] = s.granular.grainSizeMs;
    granularObj["sprayMs"] = s.granular.sprayMs;
    granularObj["transpose"] = s.granular.transpose;
    granularObj["windowSkew"] = s.granular.windowSkew;
    granularObj["stereoSpread"] = s.granular.stereoSpread;
    const std::uint8_t serializedSource =
        (s.granular.source == static_cast<std::uint8_t>(GranularEngine::Source::kSdClip))
            ? static_cast<std::uint8_t>(GranularEngine::Source::kSdClip)
            : static_cast<std::uint8_t>(GranularEngine::Source::kLiveInput);
    granularObj["source"] = serializedSource;
    std::uint8_t serializedSlot = s.granular.sdSlot;
    if (GranularEngine::kSdClipSlots > 0) {
      serializedSlot = static_cast<std::uint8_t>(serializedSlot % GranularEngine::kSdClipSlots);
    }
    granularObj["sdSlot"] = serializedSlot;

    JsonObject resonatorObj = seedObj["resonator"].to<JsonObject>();
    resonatorObj["exciteMs"] = s.resonator.exciteMs;
    resonatorObj["damping"] = s.resonator.damping;
    resonatorObj["brightness"] = s.resonator.brightness;
    resonatorObj["feedback"] = s.resonator.feedback;
    resonatorObj["mode"] = s.resonator.mode;
    resonatorObj["bank"] = s.resonator.bank;
  }

  std::string json;
  serializeJson(doc, json);
  return std::vector<std::uint8_t>(json.begin(), json.end());
}

bool Preset::deserialize(const std::vector<std::uint8_t>& bytes, Preset& out) {
  if (bytes.empty()) {
    return false;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, bytes.data(), bytes.size());
  if (err) {
    return false;
  }
  Preset next{};
  next.slot = doc["slot"].as<std::string>();
  next.masterSeed = doc["masterSeed"].as<std::uint32_t>();
  next.focusSeed = doc["focusSeed"].as<std::uint8_t>();
  next.page = static_cast<PageId>(doc["page"].as<std::uint8_t>());
  JsonObject clockObj = doc["clock"].as<JsonObject>();
  if (!clockObj.isNull()) {
    next.clock.bpm = clockObj["bpm"].as<float>();
    next.clock.followExternal = clockObj["followExternal"].as<bool>();
    next.clock.debugMeters = clockObj["debugMeters"].as<bool>();
    next.clock.transportLatch = clockObj["transportLatch"].as<bool>();
  }

  next.engineSelections.clear();
  JsonArray enginesArr = doc["engineSelections"].as<JsonArray>();
  if (!enginesArr.isNull()) {
    for (JsonVariant v : enginesArr) {
      next.engineSelections.push_back(v.as<std::uint8_t>());
    }
  }

  JsonArray seedsArr = doc["seeds"].as<JsonArray>();
  if (!seedsArr.isNull()) {
    for (JsonVariant v : seedsArr) {
      JsonObject seedObj = v.as<JsonObject>();
      Seed s{};
      s.id = seedObj["id"].as<std::uint32_t>();
      s.prng = seedObj["prng"].as<std::uint32_t>();
      s.pitch = seedObj["pitch"].as<float>();
      s.envA = seedObj["envA"].as<float>();
      s.envD = seedObj["envD"].as<float>();
      s.envS = seedObj["envS"].as<float>();
      s.envR = seedObj["envR"].as<float>();
      s.density = seedObj["density"].as<float>();
      s.probability = seedObj["probability"].as<float>();
      s.jitterMs = seedObj["jitterMs"].as<float>();
      s.tone = seedObj["tone"].as<float>();
      s.spread = seedObj["spread"].as<float>();
      s.engine = seedObj["engine"].as<std::uint8_t>();
      s.sampleIdx = seedObj["sampleIdx"].as<std::uint8_t>();
      s.mutateAmt = seedObj["mutateAmt"].as<float>();

      JsonObject granularObj = seedObj["granular"].as<JsonObject>();
      if (!granularObj.isNull()) {
        s.granular.grainSizeMs = granularObj["grainSizeMs"].as<float>();
        s.granular.sprayMs = granularObj["sprayMs"].as<float>();
        s.granular.transpose = granularObj["transpose"].as<float>();
        s.granular.windowSkew = granularObj["windowSkew"].as<float>();
        s.granular.stereoSpread = granularObj["stereoSpread"].as<float>();
        const std::uint8_t rawSource = granularObj.containsKey("source")
                                           ? granularObj["source"].as<std::uint8_t>()
                                           : static_cast<std::uint8_t>(GranularEngine::Source::kLiveInput);
        const std::uint8_t sanitizedSource =
            (rawSource == static_cast<std::uint8_t>(GranularEngine::Source::kSdClip))
                ? static_cast<std::uint8_t>(GranularEngine::Source::kSdClip)
                : static_cast<std::uint8_t>(GranularEngine::Source::kLiveInput);
        s.granular.source = sanitizedSource;

        if (granularObj.containsKey("sdSlot")) {
          const std::uint8_t rawSlot = granularObj["sdSlot"].as<std::uint8_t>();
          if (GranularEngine::kSdClipSlots > 0) {
            s.granular.sdSlot = static_cast<std::uint8_t>(rawSlot % GranularEngine::kSdClipSlots);
          } else {
            s.granular.sdSlot = rawSlot;
          }
        } else {
          s.granular.sdSlot = 0;
        }
      }

      JsonObject resonatorObj = seedObj["resonator"].as<JsonObject>();
      if (!resonatorObj.isNull()) {
        s.resonator.exciteMs = resonatorObj["exciteMs"].as<float>();
        s.resonator.damping = clampValue(resonatorObj["damping"].as<float>(), 0.f, 1.f);
        s.resonator.brightness = clampValue(resonatorObj["brightness"].as<float>(), 0.f, 1.f);
        s.resonator.feedback = clampValue(resonatorObj["feedback"].as<float>(), 0.f, 0.99f);
        s.resonator.mode = resonatorObj["mode"].as<std::uint8_t>();
        s.resonator.bank = resonatorObj["bank"].as<std::uint8_t>();
      }
      next.seeds.push_back(s);
    }
  }

  out = std::move(next);
  return true;
}

}  // namespace seedbox
