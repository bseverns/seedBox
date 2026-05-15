#pragma once

#if SEEDBOX_JUCE

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_map>

#include "juce/AppStateThreadViews.h"

namespace seedbox::juce_bridge {

class HostControlBridge {
 public:
  // Host parameter changes arrive off the audio callback and may legally touch
  // preset/seed/UI-facing state. Keep that mutation policy here so the
  // processor shell does not own another long thread-boundary switch.
  struct ParameterContext {
    std::unordered_map<std::string, float>& parameterState;
    std::uint8_t& quantizeScaleParam;
    std::uint8_t& quantizeRootParam;
    // Last-wins deferred commands. Rapid changes overwrite the pending target
    // and the maintenance timer applies only the newest command.
    std::function<void(std::uint32_t)> requestMasterSeedReseed;
    std::function<void(std::uint8_t, std::uint8_t)> requestSeedEngineApply;
    std::function<void(std::uint8_t, std::uint8_t)> requestQuantizeApply;
    // Accumulating deferred command. Granular source is a relative gesture, so
    // bursts must preserve net motion per seed slot until flush.
    std::function<void(std::uint8_t, std::int16_t)> requestGranularSourceStepApply;
    // Last-wins deferred gate policy commands.
    std::function<void(std::uint8_t)> requestGateDivisionApply;
    std::function<void(float)> requestGateFloorApply;
  };

  HostControlBridge(HostAudioThreadAccess& audioThread, HostControlThreadAccess& controlThread)
      : audioThread_(audioThread), controlThread_(controlThread) {}

  bool handleParameterChange(const juce::String& parameterID, float newValue, ParameterContext& context);
  void syncHostTransport(const juce::AudioPlayHead* playHead, bool followHostTransport);

  bool hostPlaying() const { return hostPlaying_; }

 private:
  HostAudioThreadAccess& audioThread_;
  HostControlThreadAccess& controlThread_;
  bool hostPlaying_{false};
};

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
