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
    std::function<void()> syncSeedStateFromApp;
    std::function<void(std::uint8_t)> persistFocusedSeedEngine;
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
