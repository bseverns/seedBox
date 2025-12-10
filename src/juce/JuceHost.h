#pragma once

#if SEEDBOX_JUCE

#include <juce_audio_devices/juce_audio_devices.h>
#include <memory>
#include <vector>

#include "app/AppState.h"
#include "io/MidiRouter.h"

namespace seedbox::juce_bridge {

// Desktop shim that wires JUCE's AudioDeviceManager into the existing HAL
// callback surface. The goal is to keep the SeedBox engines blissfully unaware
// of the host while still letting plugin/standalone targets honour whatever
// buffer size and sample rate the DAW asks for.
class JuceHost final : public juce::AudioIODeviceCallback, public juce::MidiInputCallback {
 public:
  explicit JuceHost(AppState& app);
  ~JuceHost() override;

  // Convenience bootstrap: open the default audio device (stereo out), connect
  // the default MIDI input/output pair, and start streaming buffers into the
  // existing hal::audio callback.
  void initialiseWithDefaults();

  // Let tests pretend the host handed us a device without actually opening
  // CoreAudio/ASIO/JACK. This mirrors audioDeviceAboutToStart but stays
  // headless so CI can chew on it.
  void configureForTests(double sampleRate, int blockSize);

  void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
  void audioDeviceStopped() override;
  void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                        float* const* outputChannelData, int numOutputChannels,
                                        int numSamples,
                                        const juce::AudioIODeviceCallbackContext& context) override;

  void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

  // Gracefully unplug from the device manager.
  void stop();

 private:
  void ensureMidiOutput();

  AppState& app_;
  juce::AudioDeviceManager deviceManager_;
  std::shared_ptr<juce::MidiOutput> midiOutput_{};
  MidiRouter::Backend* midiBackend_{nullptr};
  bool bootstrapped_{false};
  std::vector<float> scratchLeft_{};
  std::vector<float> scratchRight_{};
};

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
