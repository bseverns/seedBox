#pragma once

#if SEEDBOX_JUCE

#include <juce_gui_extra/juce_gui_extra.h>

namespace seedbox::juce_bridge {

class SeedboxAudioProcessor;

// Standalone JUCEApplication that mirrors the Arduino setup/loop lifecycle.
// Audio and MIDI flow through JUCE devices into the existing hal + AppState
// stack, giving the firmware a native desktop skin.
class SeedboxApplication : public juce::JUCEApplication {
 public:
  SeedboxApplication() = default;
  const juce::String getApplicationName() override { return "SeedBox"; }
  const juce::String getApplicationVersion() override { return "0.0.0"; }
  bool moreThanOneInstanceAllowed() const override { return true; }

  void initialise(const juce::String& commandLine) override;
  void shutdown() override;
  void systemRequestedQuit() override { quit(); }
  void anotherInstanceStarted(const juce::String&) override {}

 private:
  class MainWindow : public juce::DocumentWindow {
   public:
    MainWindow(const juce::String& name, SeedboxAudioProcessor& processor);
    void closeButtonPressed() override;

   private:
    SeedboxAudioProcessor& processor_;
  };

  std::unique_ptr<SeedboxAudioProcessor> processor_;
  std::unique_ptr<juce::AudioProcessorPlayer> player_;
  std::unique_ptr<juce::AudioDeviceManager> deviceManager_;
  std::unique_ptr<MainWindow> mainWindow_;
};

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
