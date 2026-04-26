#include "juce/HostControlBridge.h"

#if SEEDBOX_JUCE

namespace seedbox::juce_bridge {

namespace {
constexpr auto kParamTransportLatch = "transportLatch";
constexpr auto kParamClockSourceExternal = "clockSourceExternal";
constexpr auto kParamFollowExternalClock = "followExternalClock";
constexpr auto kParamFollowHostTransport = "followHostTransport";
}

bool HostControlBridge::handleParameterChange(const juce::String& parameterID, float newValue) {
  if (parameterID == kParamTransportLatch) {
    app_.setTransportLatchFromHost(newValue >= 0.5f);
    return true;
  }

  if (parameterID == kParamClockSourceExternal) {
    app_.setClockSourceExternalFromHost(newValue >= 0.5f);
    return true;
  }

  if (parameterID == kParamFollowExternalClock) {
    app_.setFollowExternalClockFromHost(newValue >= 0.5f);
    return true;
  }

  if (parameterID == kParamFollowHostTransport) {
    if (newValue < 0.5f) {
      hostPlaying_ = false;
    }
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
      app_.setInternalBpmFromHost(static_cast<float>(*bpm));
    }

    const bool isPlaying = position->getIsPlaying();
    if (isPlaying != hostPlaying_) {
      hostPlaying_ = isPlaying;
      if (hostPlaying_) {
        app_.onExternalTransportStart();
      } else {
        app_.onExternalTransportStop();
      }
    }
  }
}

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
