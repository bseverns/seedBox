#include "juce/HostControlBridge.h"

#if SEEDBOX_JUCE

namespace seedbox::juce_bridge {

namespace {
constexpr auto kParamMasterSeed = "masterSeed";
constexpr auto kParamFocusSeed = "focusSeed";
constexpr auto kParamSeedEngine = "seedEngine";
constexpr auto kParamSwingPercent = "swingPercent";
constexpr auto kParamQuantizeScale = "quantizeScale";
constexpr auto kParamQuantizeRoot = "quantizeRoot";
constexpr auto kParamTransportLatch = "transportLatch";
constexpr auto kParamClockSourceExternal = "clockSourceExternal";
constexpr auto kParamFollowExternalClock = "followExternalClock";
constexpr auto kParamFollowHostTransport = "followHostTransport";
constexpr auto kParamDebugMeters = "debugMeters";
constexpr auto kParamGranularSourceStep = "granularSourceStep";
constexpr auto kParamGateDivision = "gateDivision";
constexpr auto kParamGateFloor = "gateFloor";
constexpr auto kParamForceIdlePassthrough = "forceIdlePassthrough";
constexpr auto kParamTestTone = "testTone";
}

bool HostControlBridge::handleParameterChange(const juce::String& parameterID, float newValue,
                                              ParameterContext& context) {
  auto& parameterState = context.parameterState;

  if (parameterID == kParamMasterSeed) {
    context.requestMasterSeedReseed(static_cast<std::uint32_t>(newValue));
    parameterState[kParamMasterSeed] = newValue;
    return true;
  }

  if (parameterID == kParamFocusSeed) {
    controlThread_.setFocusSeed(static_cast<std::uint8_t>(newValue));
    parameterState[kParamFocusSeed] = newValue;
    return true;
  }

  if (parameterID == kParamSeedEngine) {
    const auto engineId = static_cast<std::uint8_t>(newValue);
    const auto focusSeed = controlThread_.focusSeed();
    context.requestSeedEngineApply(focusSeed, engineId);
    parameterState[kParamSeedEngine] = newValue;
    return true;
  }

  if (parameterID == kParamSwingPercent) {
    controlThread_.setSwingPercent(newValue);
    parameterState[kParamSwingPercent] = newValue;
    return true;
  }

  if (parameterID == kParamQuantizeScale) {
    context.quantizeScaleParam = static_cast<std::uint8_t>(newValue);
    context.requestQuantizeApply(context.quantizeScaleParam, context.quantizeRootParam);
    parameterState[kParamQuantizeScale] = newValue;
    return true;
  }

  if (parameterID == kParamQuantizeRoot) {
    context.quantizeRootParam = static_cast<std::uint8_t>(newValue);
    context.requestQuantizeApply(context.quantizeScaleParam, context.quantizeRootParam);
    parameterState[kParamQuantizeRoot] = newValue;
    return true;
  }

  if (parameterID == kParamTransportLatch) {
    controlThread_.setTransportLatch(newValue >= 0.5f);
    parameterState[kParamTransportLatch] = newValue;
    return true;
  }

  if (parameterID == kParamClockSourceExternal) {
    controlThread_.setClockSourceExternal(newValue >= 0.5f);
    parameterState[kParamClockSourceExternal] = newValue;
    return true;
  }

  if (parameterID == kParamFollowExternalClock) {
    controlThread_.setFollowExternalClock(newValue >= 0.5f);
    parameterState[kParamFollowExternalClock] = newValue;
    return true;
  }

  if (parameterID == kParamFollowHostTransport) {
    if (newValue < 0.5f) {
      hostPlaying_ = false;
    }
    parameterState[kParamFollowHostTransport] = newValue;
    return true;
  }

  if (parameterID == kParamDebugMeters) {
    controlThread_.setDebugMetersEnabled(newValue >= 0.5f);
    parameterState[kParamDebugMeters] = newValue;
    return true;
  }

  if (parameterID == kParamGranularSourceStep) {
    const float previous = parameterState[kParamGranularSourceStep];
    const auto delta = static_cast<std::int16_t>(newValue - previous);
    if (delta != 0) {
      context.requestGranularSourceStepApply(controlThread_.focusSeed(), delta);
    }
    parameterState[kParamGranularSourceStep] = newValue;
    return true;
  }

  if (parameterID == kParamGateDivision) {
    context.requestGateDivisionApply(static_cast<std::uint8_t>(newValue));
    parameterState[kParamGateDivision] = newValue;
    return true;
  }

  if (parameterID == kParamGateFloor) {
    context.requestGateFloorApply(newValue);
    parameterState[kParamGateFloor] = newValue;
    return true;
  }

  if (parameterID == kParamForceIdlePassthrough) {
    parameterState[kParamForceIdlePassthrough] = newValue;
    return true;
  }

  if (parameterID == kParamTestTone) {
    parameterState[kParamTestTone] = newValue;
    return true;
  }

  return false;
}

void HostControlBridge::syncHostTransport(const juce::AudioPlayHead* playHead, bool followHostTransport) {
  if (!followHostTransport) {
    hostPlaying_ = false;
    return;
  }

  if (!playHead) {
    return;
  }

  if (auto position = playHead->getPosition()) {
    if (auto bpm = position->getBpm()) {
      audioThread_.syncTransportBpm(static_cast<float>(*bpm));
    }

    const bool isPlaying = position->getIsPlaying();
    if (isPlaying != hostPlaying_) {
      hostPlaying_ = isPlaying;
      if (hostPlaying_) {
        audioThread_.transportStart();
      } else {
        audioThread_.transportStop();
      }
    }
  }
}

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
