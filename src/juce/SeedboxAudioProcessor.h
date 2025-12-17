#pragma once

#if SEEDBOX_JUCE

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

#include "app/AppState.h"
#include "app/Preset.h"

namespace seedbox::juce_bridge {

class SeedboxAudioProcessorEditor;

// AudioProcessor wrapper around the existing AppState/hal stack. The goal is
// to let a DAW treat the SeedBox engine like any other plugin while keeping the
// original firmware untouched. MIDI in/out stays routed through MidiRouter;
// audio rendering still happens through hal::audio.
class SeedboxAudioProcessor : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener {
 public:
  SeedboxAudioProcessor();
  ~SeedboxAudioProcessor() override;

  void attachDeviceManager(juce::AudioDeviceManager* manager) { deviceManager_ = manager; }
  juce::AudioDeviceManager* deviceManager() const { return deviceManager_; }

  // juce::AudioProcessor
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
  using juce::AudioProcessor::processBlock;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return "SeedBox"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return true; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return "default"; }
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  AppState& appState() { return app_; }
  juce::AudioProcessorValueTreeState& parameters() { return parameters_; }
  void applySeedEdit(const juce::Identifier& key, double value,
                     const std::function<void(Seed&)>& applyFn);
  juce::var getSeedProp(int idx, const juce::Identifier& key, juce::var defaultValue) const;
  void setPanelQuickPreset(const seedbox::Preset& preset);
  bool applyPanelQuickPreset();
  const std::optional<seedbox::Preset>& panelQuickPreset() const { return panelPreset_; }

 private:
  struct BufferedMidiMessage {
    juce::MidiMessage message;
    int samplePosition{0};
  };

  class ProcessorMidiBackend : public MidiRouter::Backend {
   public:
    ProcessorMidiBackend(MidiRouter& router, MidiRouter::Port port);

    MidiRouter::PortInfo describe() const override;
    void begin() override;
    void poll() override;
    void sendClock() override;
    void sendStart() override;
    void sendStop() override;
    void sendControlChange(std::uint8_t channel, std::uint8_t controller, std::uint8_t value) override;
    void sendNoteOn(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity) override;
    void sendNoteOff(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity) override;
    void sendAllNotesOff(std::uint8_t channel) override;

    void queueIncoming(const BufferedMidiMessage& msg);
    void setOutboundBuffer(juce::MidiBuffer* buffer) { midiOut_ = buffer; }

   private:
    void handle(const BufferedMidiMessage& msg);
    void emit(const juce::MidiMessage& msg, int position);

    std::vector<BufferedMidiMessage> incoming_;
    juce::MidiBuffer* midiOut_{nullptr};
  };

  void parameterChanged(const juce::String& parameterID, float newValue) override;
  static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
  void applyPendingPresetIfAny();
  void syncSeedStateFromApp();
  void applySeedStateToApp();
  juce::ValueTree getOrCreateSeedNode(int idx);
  juce::ValueTree findSeedNode(int idx) const;
  void setSeedProp(int idx, const juce::Identifier& key, const juce::var& value);
  juce::String serializePresetToBase64(const seedbox::Preset& preset) const;

  AppState app_;
  ProcessorMidiBackend* midiBackend_{nullptr};
  juce::AudioProcessorValueTreeState parameters_;
  juce::AudioDeviceManager* deviceManager_{nullptr};  // owned by SeedboxApplication when present
  juce::AudioBuffer<float> renderScratch_{};
  bool prepared_{false};
  juce::AudioBuffer<float> inputScratch_{};
  std::optional<seedbox::Preset> pendingPreset_{};
  std::optional<seedbox::Preset> panelPreset_{};
  juce::String panelPresetBase64_{};
  std::unordered_map<std::string, float> parameterState_{};
  std::uint8_t quantizeScaleParam_{0};
  std::uint8_t quantizeRootParam_{0};
};

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
