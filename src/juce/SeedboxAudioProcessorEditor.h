#pragma once

#if SEEDBOX_JUCE

#include <juce_audio_processors/juce_audio_processors.h>

namespace seedbox::juce_bridge {

class SeedboxAudioProcessor;

// Lightweight editor: shows the firmware's OLED snapshot and surfaces the master
// seed as an automatable control. The vibe is intentionally lo-fi so hosts get a
// responsive UI without burying students in chrome.
class SeedboxAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  explicit SeedboxAudioProcessorEditor(SeedboxAudioProcessor& processor);
  ~SeedboxAudioProcessorEditor() override;

  void paint(juce::Graphics& g) override;
  void resized() override;

 private:
  void timerCallback() override;
  void refreshDisplay();

  SeedboxAudioProcessor& processor_;
  juce::Slider masterSeedSlider_;
  juce::Label displayLabel_;
  std::unique_ptr<juce::SliderParameterAttachment> masterSeedAttachment_;
};

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
