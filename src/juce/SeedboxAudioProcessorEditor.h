#pragma once

#if SEEDBOX_JUCE

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

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
  void refreshEngineControls();
  void buildAudioSelector();

  SeedboxAudioProcessor& processor_;
  juce::ComboBox modeSelector_;
  juce::Slider masterSeedSlider_;
  juce::ComboBox focusSeedSelector_;
  juce::ComboBox engineSelector_;
  juce::Slider swingSlider_;
  juce::ComboBox quantizeScaleSelector_;
  juce::ComboBox quantizeRootSelector_;
  juce::Slider granularSourceSlider_;
  juce::Label seedParamHeader_;
  juce::Slider seedPitchSlider_;
  juce::Slider seedDensitySlider_;
  juce::Slider seedProbabilitySlider_;
  juce::Slider seedJitterSlider_;
  juce::Slider seedToneSlider_;
  juce::Slider seedSpreadSlider_;
  juce::Slider seedMutateSlider_;
  juce::Label engineControlHeader_;
  juce::TextEditor engineControlList_;
  juce::Label audioSelectorHint_;
  std::unique_ptr<juce::AudioDeviceSelectorComponent> audioSelector_;
  juce::ToggleButton transportLatchButton_;
  juce::ToggleButton externalClockButton_;
  juce::ToggleButton followClockButton_;
  juce::ToggleButton debugMetersButton_;
  juce::Label displayLabel_;
  std::unique_ptr<juce::SliderParameterAttachment> masterSeedAttachment_;
  std::unique_ptr<juce::ComboBoxParameterAttachment> focusSeedAttachment_;
  std::unique_ptr<juce::ComboBoxParameterAttachment> engineAttachment_;
  std::unique_ptr<juce::SliderParameterAttachment> swingAttachment_;
  std::unique_ptr<juce::ComboBoxParameterAttachment> quantizeScaleAttachment_;
  std::unique_ptr<juce::ComboBoxParameterAttachment> quantizeRootAttachment_;
  std::unique_ptr<juce::SliderParameterAttachment> granularAttachment_;
  std::unique_ptr<juce::SliderParameterAttachment> seedPitchAttachment_;
  std::unique_ptr<juce::SliderParameterAttachment> seedDensityAttachment_;
  std::unique_ptr<juce::SliderParameterAttachment> seedProbabilityAttachment_;
  std::unique_ptr<juce::SliderParameterAttachment> seedJitterAttachment_;
  std::unique_ptr<juce::SliderParameterAttachment> seedToneAttachment_;
  std::unique_ptr<juce::SliderParameterAttachment> seedSpreadAttachment_;
  std::unique_ptr<juce::SliderParameterAttachment> seedMutateAttachment_;
  std::unique_ptr<juce::ButtonParameterAttachment> transportLatchAttachment_;
  std::unique_ptr<juce::ButtonParameterAttachment> externalClockAttachment_;
  std::unique_ptr<juce::ButtonParameterAttachment> followClockAttachment_;
  std::unique_ptr<juce::ButtonParameterAttachment> debugMetersAttachment_;
  int lastEngineId_{-1};
};

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
