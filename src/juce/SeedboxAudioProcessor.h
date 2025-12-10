#pragma once

#if SEEDBOX_JUCE

#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>
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

  AppState app_;
  ProcessorMidiBackend* midiBackend_{nullptr};
  juce::AudioProcessorValueTreeState parameters_;
  bool prepared_{false};
  std::optional<seedbox::Preset> pendingPreset_{};
};

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
