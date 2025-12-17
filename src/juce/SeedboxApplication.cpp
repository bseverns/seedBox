#include "juce/SeedboxApplication.h"

#if SEEDBOX_JUCE

#include <juce_audio_processors/juce_audio_processors.h>

#include "juce/SeedboxAudioProcessor.h"
#include "juce/SeedboxAudioProcessorEditor.h"

namespace seedbox::juce_bridge {

void SeedboxApplication::initialise(const juce::String&) {
  appProperties_ = std::make_unique<juce::ApplicationProperties>();
  juce::PropertiesFile::Options options;
  options.applicationName = getApplicationName();
  options.filenameSuffix = "settings";
  options.folderName = "SeedBox";
  options.osxLibrarySubFolder = "Application Support";
  appProperties_->setStorageParameters(options);

  processor_ = std::make_unique<SeedboxAudioProcessor>();
  player_ = std::make_unique<juce::AudioProcessorPlayer>();
  deviceManager_ = std::make_unique<juce::AudioDeviceManager>();
  processor_->attachDeviceManager(deviceManager_.get());

  player_->setProcessor(processor_.get());

  deviceManager_->initialise(/*numInputChannels*/ 2, /*numOutputChannels*/ 2, nullptr, true, {}, nullptr);
  deviceManager_->addAudioCallback(player_.get());
  deviceManager_->addMidiInputDeviceCallback({}, player_.get());

  mainWindow_ = std::make_unique<MainWindow>(getApplicationName(), *processor_);
  bool restoredWindow = false;
  if (auto* settings = appProperties_->getUserSettings()) {
    const int lastMode = settings->getIntValue("lastMode", static_cast<int>(AppState::Mode::HOME));
    processor_->appState().setModeFromHost(static_cast<AppState::Mode>(lastMode));

    const auto windowState = settings->getValue("windowState");
    if (!windowState.isEmpty()) {
      mainWindow_->restoreWindowStateFromString(windowState);
      restoredWindow = true;
    }
  }
  if (!restoredWindow) {
    mainWindow_->centreWithSize(mainWindow_->getWidth(), mainWindow_->getHeight());
  }
  mainWindow_->setVisible(true);
}

void SeedboxApplication::shutdown() {
  if (appProperties_) {
    if (auto* settings = appProperties_->getUserSettings()) {
      settings->setValue("lastMode", static_cast<int>(processor_->appState().mode()));
      if (mainWindow_) {
        settings->setValue("windowState", mainWindow_->getWindowStateAsString());
      }
      settings->saveIfNeeded();
    }
  }

  if (deviceManager_) {
    deviceManager_->removeAudioCallback(player_.get());
    deviceManager_->removeMidiInputDeviceCallback({}, player_.get());
  }
  player_.reset();
  deviceManager_.reset();
  mainWindow_.reset();
  processor_.reset();
}

SeedboxApplication::MainWindow::MainWindow(const juce::String& name, SeedboxAudioProcessor& processor)
    : DocumentWindow(name, juce::Colours::black, DocumentWindow::allButtons), processor_(processor) {
  setUsingNativeTitleBar(true);
  setResizable(true, false);
  setContentOwned(processor_.createEditor(), true);
}

void SeedboxApplication::MainWindow::closeButtonPressed() { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

}  // namespace seedbox::juce_bridge

START_JUCE_APPLICATION(seedbox::juce_bridge::SeedboxApplication)

#endif  // SEEDBOX_JUCE
