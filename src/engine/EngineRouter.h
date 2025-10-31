#pragma once

//
// EngineRouter
// ------------
// This class is the switchboard between the transport layer and the DSP toys.
// PatternScheduler hands it seeds with sample-accurate timestamps, and the
// router figures out which engine should respond.  The registry + metadata
// makes it trivial to bolt on new engines without rewriting AppState demos.
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Seed.h"
#include "engine/BurstEngine.h"
#include "engine/EuclidEngine.h"
#include "engine/Granular.h"
#include "engine/Resonator.h"
#include "engine/Sampler.h"
#include "util/Annotations.h"

class EngineRouter {
public:
  enum class Mode : std::uint8_t { kSim, kHardware };

  static constexpr std::uint8_t kSamplerId = 0;
  static constexpr std::uint8_t kGranularId = 1;
  static constexpr std::uint8_t kResonatorId = 2;
  static constexpr std::uint8_t kEuclidId = 3;
  static constexpr std::uint8_t kBurstId = 4;

  void init(Mode mode);

  void setSeedCount(std::size_t count);
  void assignSeed(std::size_t index, std::uint8_t engineId);
  void setSeedLock(std::size_t index, bool locked);
  void setGlobalLock(bool locked);
  void reseed(std::uint32_t masterSeed);

  void triggerSeed(const Seed& seed, std::uint32_t whenSamples);

  SEEDBOX_MAYBE_UNUSED static void dispatchThunk(void* ctx, const Seed& seed, std::uint32_t whenSamples);

  Sampler& sampler() { return *sampler_; }
  const Sampler& sampler() const { return *sampler_; }
  GranularEngine& granular() { return *granular_; }
  const GranularEngine& granular() const { return *granular_; }
  ResonatorBank& resonator() { return *resonator_; }
  const ResonatorBank& resonator() const { return *resonator_; }
  EuclidEngine& euclid() { return *euclid_; }
  const EuclidEngine& euclid() const { return *euclid_; }
  BurstEngine& burst() { return *burst_; }
  const BurstEngine& burst() const { return *burst_; }

  std::size_t engineCount() const;
  std::uint8_t sanitizeEngineId(std::uint8_t engineId) const;
  std::string_view engineShortName(std::uint8_t engineId) const;
  const std::string& engineName(std::uint8_t engineId) const;

private:
  struct RegisteredEngine {
    std::uint8_t id{0};
    std::string name;
    std::array<char, 4> shortName{};
    std::unique_ptr<Engine> instance;
  };

  Engine* findEngine(std::uint8_t engineId);
  const Engine* findEngine(std::uint8_t engineId) const;
  void registerEngine(std::uint8_t id, std::string_view name, std::string_view shortName,
                      std::unique_ptr<Engine> engine);

  Mode mode_{Mode::kSim};
  std::map<std::uint8_t, RegisteredEngine> registry_{};
  Sampler* sampler_{nullptr};
  GranularEngine* granular_{nullptr};
  ResonatorBank* resonator_{nullptr};
  EuclidEngine* euclid_{nullptr};
  BurstEngine* burst_{nullptr};
  std::vector<std::uint8_t> seedAssignments_{};
  std::vector<bool> seedLocks_{};
  bool globalLock_{false};
  std::uint32_t lastMasterSeed_{0};
};

