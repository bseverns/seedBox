#pragma once

#if SEEDBOX_JUCE

#ifndef SEEDBOX_LEGACY_UI
#define SEEDBOX_LEGACY_UI 0
#endif

#include <array>
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "hal/Board.h"
#include "juce/ui/SeedboxPanelView.h"

namespace seedbox::juce_bridge {

class SeedboxAudioProcessor;

class PageComponent : public juce::Component {
 public:
  explicit PageComponent(SeedboxAudioProcessor& processor) : processor_(processor) {}
  ~PageComponent() override = default;
  virtual void refresh() = 0;

 protected:
  SeedboxAudioProcessor& processor_;
};

class HomePageComponent : public PageComponent {
 public:
  explicit HomePageComponent(SeedboxAudioProcessor& processor);
  void refresh() override;
  void resized() override;
  juce::Slider* masterSeedSlider() { return &masterSeedSlider_; }

 private:
  juce::Slider masterSeedSlider_;
  juce::Label bpmLabel_;
  juce::Label clockLabel_;
  juce::Label focusLabel_;
  std::unique_ptr<juce::SliderParameterAttachment> masterSeedAttachment_;
};

class SeedsPageComponent : public PageComponent {
 public:
  explicit SeedsPageComponent(SeedboxAudioProcessor& processor);
  void refresh() override;
  void resized() override;
  juce::Slider* toneSlider() { return &seedToneSlider_; }

 private:
  juce::ComboBox focusSeedSelector_;
  juce::ComboBox gateDivisionSelector_;
  juce::Slider seedToneSlider_;
  juce::Slider seedProbabilitySlider_;
  juce::Slider gateFloorSlider_;
  juce::TextButton randomizeSeedButton_;
  std::unique_ptr<juce::ComboBoxParameterAttachment> focusSeedAttachment_;
  std::unique_ptr<juce::ComboBoxParameterAttachment> gateDivisionAttachment_;
  std::unique_ptr<juce::SliderParameterAttachment> gateFloorAttachment_;
};

class EnginePageComponent : public PageComponent {
 public:
  explicit EnginePageComponent(SeedboxAudioProcessor& processor);
  void refresh() override;
  void resized() override;
  void nudgeFirstKnob(double delta);
  int currentEngineId() const;
  void setEngineId(int id);
  int engineCount() const;

 private:
  struct NamedKnob {
    juce::Label label;
    juce::Slider slider;
  };
  juce::ComboBox engineSelector_;
  juce::Label heading_;
  std::array<NamedKnob, 4> engineKnobs_{};
  int visibleEngineKnobCount_{0};
  int lastEngineId_{-1};
  bool updatingEngineKnobs_{false};
  std::unique_ptr<juce::ComboBoxParameterAttachment> engineAttachment_;
};

class PerfPageComponent : public PageComponent {
 public:
 explicit PerfPageComponent(SeedboxAudioProcessor& processor);
  void refresh() override;
  void resized() override;
  juce::Slider* tempoSlider() { return &tempoSlider_; }
  void setTapHandler(std::function<void()> fn) { tapHandler_ = std::move(fn); }

 private:
  juce::Slider tempoSlider_;
  juce::TextButton tapTempoButton_;
  juce::ToggleButton transportLatchButton_;
  juce::Label bpmLabel_;
  juce::Label clockLabel_;
  std::unique_ptr<juce::ButtonParameterAttachment> transportLatchAttachment_;
  std::function<void()> tapHandler_;
};

class SwingPageComponent : public PageComponent {
 public:
  explicit SwingPageComponent(SeedboxAudioProcessor& processor);
  void refresh() override;
  void resized() override;
  juce::Slider* swingSlider() { return &swingSlider_; }

 private:
  juce::Slider swingSlider_;
  juce::ComboBox quantizeScaleSelector_;
  juce::ComboBox quantizeRootSelector_;
  std::unique_ptr<juce::SliderParameterAttachment> swingAttachment_;
  std::unique_ptr<juce::ComboBoxParameterAttachment> quantizeScaleAttachment_;
  std::unique_ptr<juce::ComboBoxParameterAttachment> quantizeRootAttachment_;
};

class UtilPageComponent : public PageComponent {
 public:
  explicit UtilPageComponent(SeedboxAudioProcessor& processor);
  void refresh() override;
  void resized() override;

 private:
  juce::ToggleButton debugMetersButton_;
  juce::TextButton panicButton_;
  std::unique_ptr<juce::ButtonParameterAttachment> debugMetersAttachment_;
};

class SettingsPageComponent : public PageComponent {
 public:
  explicit SettingsPageComponent(SeedboxAudioProcessor& processor);
  void refresh() override;
  void resized() override;

 private:
  juce::ToggleButton externalClockButton_;
  juce::ToggleButton followClockButton_;
  juce::ToggleButton idlePassthroughButton_;
  juce::Label audioInfo_;
  std::unique_ptr<juce::ButtonParameterAttachment> externalClockAttachment_;
  std::unique_ptr<juce::ButtonParameterAttachment> followClockAttachment_;
  std::unique_ptr<juce::ButtonParameterAttachment> idlePassthroughAttachment_;
};

// Lightweight editor: shows the firmware's OLED snapshot and surfaces real controls
// per page so the desktop mirror feels like the panel.
class SeedboxAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  explicit SeedboxAudioProcessorEditor(SeedboxAudioProcessor& processor);
  ~SeedboxAudioProcessorEditor() override;

  void paint(juce::Graphics& g) override;
  void resized() override;
  bool keyPressed(const juce::KeyPress& key) override;
  bool keyStateChanged(bool isKeyDown) override;
  void setLastDeviceInitError(const juce::String& errorMessage);

 private:
  void timerCallback() override;
  void refreshDisplay();
  void syncKeyboardButtons();
  bool handleButtonKey(int keyCode, bool pressed);
  void updateButtonState(hal::Board::ButtonID id, bool pressed, bool& lastState);
  void handleTapTempo();
  void nudgeVisibleControl(double delta);
  void updateVisiblePage();
  void refreshAllPages();
  void setAdvancedVisible(bool visible);
  bool advancedUiEnabled() const;
  void buildAudioSelector();

  SeedboxAudioProcessor& processor_;
  const bool useLegacyUi_{SEEDBOX_LEGACY_UI != 0};
  bool showAdvanced_{true};
  std::unique_ptr<SeedboxPanelView> panelView_;
  juce::TextButton advancedToggle_{};
  juce::GroupComponent advancedFrame_{};
  juce::ComboBox modeSelector_;
  juce::Label displayLabel_;
  juce::Label shortcutsLabel_;
  juce::Label advancedHint_;
  juce::Label audioSelectorHint_;
  juce::Label audioInitWarning_;
  std::unique_ptr<juce::AudioDeviceSelectorComponent> audioSelector_;
  std::unique_ptr<HomePageComponent> homePage_;
  std::unique_ptr<SeedsPageComponent> seedsPage_;
  std::unique_ptr<EnginePageComponent> enginePage_;
  std::unique_ptr<PerfPageComponent> perfPage_;
  std::unique_ptr<SwingPageComponent> swingPage_;
  std::unique_ptr<UtilPageComponent> utilPage_;
  std::unique_ptr<SettingsPageComponent> settingsPage_;
  const int toneKeyCode_{'o'};
  const int shiftKeyCode_{'s'};
  const int altKeyCode_{'a'};
  bool toneKeyDown_{false};
  bool shiftKeyDown_{false};
  bool altKeyDown_{false};
  double lastTapMs_{0.0};
  juce::String lastDeviceInitError_;
};

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
