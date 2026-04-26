#pragma once

#if SEEDBOX_JUCE

#include <juce_audio_processors/juce_audio_processors.h>

#include "app/AppState.h"

namespace seedbox::juce_bridge {

class HostControlBridge {
 public:
  explicit HostControlBridge(AppState& app) : app_(app) {}

  bool handleParameterChange(const juce::String& parameterID, float newValue);
  void syncHostTransport(const juce::AudioPlayHead* playHead, bool followHostTransport);

  bool hostPlaying() const { return hostPlaying_; }

 private:
  AppState& app_;
  bool hostPlaying_{false};
};

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
