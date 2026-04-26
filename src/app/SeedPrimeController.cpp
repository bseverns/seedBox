#include "app/SeedPrimeController.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>

#include "util/RNG.h"

namespace {
constexpr uint32_t kSaltPitch = 0xA1B2C3D4u;
constexpr uint32_t kSaltDensity = 0xB2C3D4E5u;
constexpr uint32_t kSaltProbability = 0xC3D4E5F6u;
constexpr uint32_t kSaltJitter = 0xD4E5F601u;
constexpr uint32_t kSaltTone = 0xE5F601A2u;
constexpr uint32_t kSaltSpread = 0xF601A2B3u;
constexpr uint32_t kSaltGranularSize = 0x0A1B2C3Du;
constexpr uint32_t kSaltGranularSpray = 0x1B2C3D4Eu;
constexpr uint32_t kSaltGranularTranspose = 0x2C3D4E5Fu;
constexpr uint32_t kSaltGranularWindow = 0x3D4E5F60u;
constexpr uint32_t kSaltGranularSpread = 0x4E5F6011u;
constexpr uint32_t kSaltGranularSource = 0x5F601122u;
constexpr uint32_t kSaltGranularSlot = 0x60112233u;
constexpr uint32_t kSaltResonatorExcite = 0x71122334u;
constexpr uint32_t kSaltResonatorDamping = 0x81223345u;
constexpr uint32_t kSaltResonatorBrightness = 0x92334456u;
constexpr uint32_t kSaltResonatorFeedback = 0xA3344557u;
constexpr uint32_t kSaltResonatorBank = 0xC5566779u;

float deterministicNormalizedValue(uint32_t masterSeed, std::size_t slot, uint32_t salt) {
  uint32_t state = masterSeed ? masterSeed : 0x5EEDB0B1u;
  state ^= static_cast<uint32_t>(slot) * 0x9E3779B9u;
  state ^= salt;
  const uint32_t hashed = RNG::xorshift(state);
  constexpr float kNormalizedRange = 1.0f / 16777216.0f;
  return (hashed >> 8) * kNormalizedRange;
}

float mixEntropy(float base, float randomValue, float entropy) {
  const float clamped = std::clamp(entropy, 0.0f, 1.0f);
  return base + (randomValue - base) * clamped;
}

uint8_t deterministicBucket(uint32_t masterSeed, std::size_t slot, uint32_t salt, uint8_t bucketCount) {
  if (bucketCount == 0) {
    return 0;
  }
  const float normalized = deterministicNormalizedValue(masterSeed, slot, salt);
  const uint32_t candidate = static_cast<uint32_t>(normalized * static_cast<float>(bucketCount));
  return static_cast<uint8_t>(std::min<uint32_t>(candidate, bucketCount - 1));
}

constexpr std::array<SeedPrimeController::Mode, 4> kPrimeModes{{
    SeedPrimeController::Mode::kLfsr,
    SeedPrimeController::Mode::kTapTempo,
    SeedPrimeController::Mode::kPreset,
    SeedPrimeController::Mode::kLiveInput,
}};
}  // namespace

SeedPrimeController::Mode SeedPrimeController::rotateMode(Mode current, int step) const {
  const int count = static_cast<int>(kPrimeModes.size());
  auto it = std::find(kPrimeModes.begin(), kPrimeModes.end(), current);
  const int index = (it == kPrimeModes.end())
                        ? 0
                        : static_cast<int>(std::distance(kPrimeModes.begin(), it));
  int next = index + step;
  if (next < 0) {
    next = (next % count) + count;
  }
  next %= count;
  return kPrimeModes[static_cast<std::size_t>(next)];
}

const char* SeedPrimeController::modeLabel(Mode mode) const {
  switch (mode) {
    case Mode::kTapTempo: return "Tap";
    case Mode::kPreset: return "Preset";
    case Mode::kLiveInput: return "Live";
    case Mode::kLfsr:
    default: return "LFSR";
  }
}

std::vector<Seed> SeedPrimeController::buildLfsrSeeds(std::uint32_t masterSeed,
                                                      std::size_t count,
                                                      float entropy,
                                                      float mutationRate) const {
  std::vector<Seed> seeds;
  seeds.reserve(count);
  std::uint32_t state = masterSeed ? masterSeed : 0x5EEDB0B1u;
  const float clampedEntropy = std::clamp(entropy, 0.0f, 1.0f);
  const float clampedMutation = std::clamp(mutationRate, 0.0f, 1.0f);
  for (std::size_t i = 0; i < count; ++i) {
    Seed seed{};
    seed.id = static_cast<uint32_t>(i);
    seed.source = Seed::Source::kLfsr;
    seed.lineage = masterSeed;
    seed.prng = RNG::xorshift(state);
    seed.engine = 0;
    seed.sampleIdx = static_cast<uint8_t>(i % 16);

    const float randomPitch = static_cast<float>(static_cast<int32_t>(RNG::xorshift(state) % 25) - 12);
    const float basePitch = -12.f + deterministicNormalizedValue(masterSeed, i, kSaltPitch) * 24.f;
    seed.pitch = mixEntropy(basePitch, randomPitch, clampedEntropy);

    const float randomDensity = 0.5f + 0.75f * RNG::uniform01(state);
    const float baseDensity = 0.5f + deterministicNormalizedValue(masterSeed, i, kSaltDensity) * 0.75f;
    seed.density = mixEntropy(baseDensity, randomDensity, clampedEntropy);

    const float randomProbability = RNG::uniform01(state);
    const float baseProbability = deterministicNormalizedValue(masterSeed, i, kSaltProbability);
    seed.probability = mixEntropy(baseProbability, randomProbability, clampedEntropy);

    const float randomJitter = 2.0f + 12.0f * RNG::uniform01(state);
    const float baseJitter = 2.0f + deterministicNormalizedValue(masterSeed, i, kSaltJitter) * 12.0f;
    seed.jitterMs = mixEntropy(baseJitter, randomJitter, clampedEntropy);

    const float randomTone = RNG::uniform01(state);
    const float baseTone = deterministicNormalizedValue(masterSeed, i, kSaltTone);
    seed.tone = mixEntropy(baseTone, randomTone, clampedEntropy);

    const float randomSpread = 0.1f + 0.8f * RNG::uniform01(state);
    const float baseSpread = 0.1f + deterministicNormalizedValue(masterSeed, i, kSaltSpread) * 0.8f;
    seed.spread = mixEntropy(baseSpread, randomSpread, clampedEntropy);

    const float randomMutate = 0.05f + 0.15f * RNG::uniform01(state);
    seed.mutateAmt = std::clamp(mixEntropy(clampedMutation, randomMutate, clampedEntropy), 0.0f, 1.0f);

    const float randomSize = 35.f + 120.f * RNG::uniform01(state);
    const float baseSize = 35.f + deterministicNormalizedValue(masterSeed, i, kSaltGranularSize) * 120.f;
    seed.granular.grainSizeMs = mixEntropy(baseSize, randomSize, clampedEntropy);

    const float randomSpray = 4.f + 24.f * RNG::uniform01(state);
    const float baseSpray = 4.f + deterministicNormalizedValue(masterSeed, i, kSaltGranularSpray) * 24.f;
    seed.granular.sprayMs = mixEntropy(baseSpray, randomSpray, clampedEntropy);

    const float randomTranspose = static_cast<float>(static_cast<int32_t>(RNG::xorshift(state) % 13) - 6);
    const float baseTranspose =
        static_cast<float>(static_cast<int32_t>(deterministicBucket(masterSeed, i, kSaltGranularTranspose, 13)) - 6);
    seed.granular.transpose = mixEntropy(baseTranspose, randomTranspose, clampedEntropy);

    const float randomWindow = (RNG::uniform01(state) * 2.f) - 1.f;
    const float baseWindow = (deterministicNormalizedValue(masterSeed, i, kSaltGranularWindow) * 2.f) - 1.f;
    seed.granular.windowSkew = mixEntropy(baseWindow, randomWindow, clampedEntropy);

    const float randomStereo = 0.2f + 0.7f * RNG::uniform01(state);
    const float baseStereo = 0.2f + deterministicNormalizedValue(masterSeed, i, kSaltGranularSpread) * 0.7f;
    seed.granular.stereoSpread = mixEntropy(baseStereo, randomStereo, clampedEntropy);

    const float randomSource = RNG::uniform01(state);
    const float baseSource = deterministicNormalizedValue(masterSeed, i, kSaltGranularSource);
    const float sourceThreshold = mixEntropy(baseSource, randomSource, clampedEntropy);
    seed.granular.source = (sourceThreshold > 0.4f)
                               ? static_cast<uint8_t>(GranularEngine::Source::kSdClip)
                               : static_cast<uint8_t>(GranularEngine::Source::kLiveInput);

    const uint8_t randomSdSlot = static_cast<uint8_t>(RNG::xorshift(state) % GranularEngine::kSdClipSlots);
    const float baseSd = static_cast<float>(deterministicBucket(masterSeed, i, kSaltGranularSlot,
                                                                 GranularEngine::kSdClipSlots));
    const float mixedSdSlot = mixEntropy(baseSd, static_cast<float>(randomSdSlot), clampedEntropy);
    seed.granular.sdSlot = static_cast<uint8_t>(std::clamp(
        static_cast<int>(std::round(mixedSdSlot)), 0, GranularEngine::kSdClipSlots - 1));

    const float randomExcite = 2.0f + 10.0f * RNG::uniform01(state);
    const float baseExcite = 2.0f + deterministicNormalizedValue(masterSeed, i, kSaltResonatorExcite) * 10.0f;
    seed.resonator.exciteMs = mixEntropy(baseExcite, randomExcite, clampedEntropy);

    const float randomDamping = RNG::uniform01(state);
    const float baseDamping = deterministicNormalizedValue(masterSeed, i, kSaltResonatorDamping);
    seed.resonator.damping = mixEntropy(baseDamping, randomDamping, clampedEntropy);

    const float randomBrightness = RNG::uniform01(state);
    const float baseBrightness = deterministicNormalizedValue(masterSeed, i, kSaltResonatorBrightness);
    seed.resonator.brightness = mixEntropy(baseBrightness, randomBrightness, clampedEntropy);

    const float randomFeedback = 0.55f + 0.4f * RNG::uniform01(state);
    const float baseFeedback = 0.55f + deterministicNormalizedValue(masterSeed, i, kSaltResonatorFeedback) * 0.4f;
    seed.resonator.feedback = std::min(mixEntropy(baseFeedback, randomFeedback, clampedEntropy), 0.99f);

    const float randomMode = RNG::uniform01(state);
    const float baseMode = static_cast<float>(i % 2);
    const float modeMix = mixEntropy(baseMode, randomMode, clampedEntropy);
    seed.resonator.mode = static_cast<uint8_t>(modeMix > 0.5f ? 1 : 0);

    const uint8_t randomBank = static_cast<uint8_t>(RNG::xorshift(state) % 6);
    const float baseBank = static_cast<float>(deterministicBucket(masterSeed, i, kSaltResonatorBank, 6));
    const float mixedBank = mixEntropy(baseBank, static_cast<float>(randomBank), clampedEntropy);
    seed.resonator.bank = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(mixedBank)), 0, 5));

    seeds.push_back(seed);
  }
  return seeds;
}

std::vector<Seed> SeedPrimeController::buildTapTempoSeeds(std::uint32_t masterSeed,
                                                          std::size_t count,
                                                          float bpm,
                                                          float entropy,
                                                          float mutationRate) const {
  auto seeds = buildLfsrSeeds(masterSeed, count, entropy, mutationRate);
  const float safeBpm = bpm > 1.f ? bpm : 120.f;
  const float densityScale = safeBpm / 120.f;
  const uint32_t lineageTag = static_cast<uint32_t>(std::max(0.f, safeBpm * 100.f));
  for (auto& seed : seeds) {
    seed.source = Seed::Source::kTapTempo;
    seed.lineage = lineageTag;
    seed.density = std::clamp(seed.density * densityScale, 0.25f, 6.0f);
    seed.jitterMs = std::max(0.5f, seed.jitterMs * 0.5f);
  }
  return seeds;
}

std::vector<Seed> SeedPrimeController::buildPresetSeeds(std::uint32_t masterSeed,
                                                        std::size_t count,
                                                        const std::vector<Seed>& presetSeeds,
                                                        std::uint32_t presetId,
                                                        float entropy,
                                                        float mutationRate) const {
  if (presetSeeds.empty()) {
    return buildLfsrSeeds(masterSeed, count, entropy, mutationRate);
  }

  std::vector<Seed> seeds;
  seeds.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    const Seed& templateSeed = presetSeeds[i % presetSeeds.size()];
    Seed seed = templateSeed;
    seed.id = static_cast<uint32_t>(i);
    seed.source = Seed::Source::kPreset;
    seed.lineage = presetId;
    if (seed.prng == 0) {
      uint32_t lineageSeed = masterSeed ^ (presetId + static_cast<uint32_t>(i * 97));
      uint32_t rngState = lineageSeed;
      if (rngState == 0) {
        rngState = masterSeed;
      }
      seed.prng = RNG::xorshift(rngState);
    }
    seeds.push_back(seed);
  }
  return seeds;
}

std::vector<Seed> SeedPrimeController::buildLiveInputSeeds(std::uint32_t masterSeed,
                                                           std::size_t count,
                                                           std::uint32_t liveCaptureLineage,
                                                           std::uint8_t liveCaptureSlot,
                                                           float entropy,
                                                           float mutationRate) const {
  auto seeds = buildLfsrSeeds(masterSeed, count, entropy, mutationRate);
  const uint32_t lineage = liveCaptureLineage ? liveCaptureLineage : masterSeed;
  const uint8_t slot = liveCaptureSlot;
  for (auto& seed : seeds) {
    seed.source = Seed::Source::kLiveInput;
    seed.lineage = lineage;
    seed.sampleIdx = slot;
    seed.granular.source = static_cast<uint8_t>(GranularEngine::Source::kLiveInput);
    seed.granular.sdSlot = slot;
  }
  return seeds;
}

std::vector<Seed> SeedPrimeController::buildSeeds(Mode mode,
                                                  std::uint32_t masterSeed,
                                                  std::size_t count,
                                                  float entropy,
                                                  float mutationRate,
                                                  float tapTempoBpm,
                                                  std::uint32_t liveCaptureLineage,
                                                  std::uint8_t liveCaptureSlot,
                                                  const std::vector<Seed>& presetSeeds,
                                                  std::uint32_t presetId) const {
  switch (mode) {
    case Mode::kTapTempo:
      return buildTapTempoSeeds(masterSeed, count, tapTempoBpm, entropy, mutationRate);
    case Mode::kPreset:
      return buildPresetSeeds(masterSeed, count, presetSeeds, presetId, entropy, mutationRate);
    case Mode::kLiveInput:
      return buildLiveInputSeeds(masterSeed, count, liveCaptureLineage, liveCaptureSlot, entropy, mutationRate);
    case Mode::kLfsr:
    default:
      return buildLfsrSeeds(masterSeed, count, entropy, mutationRate);
  }
}
