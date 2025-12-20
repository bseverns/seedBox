#include "juce/JuceHost.h"

#if SEEDBOX_JUCE

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <mutex>
#include <utility>

#include "hal/hal_audio.h"

namespace seedbox::juce_bridge {

namespace {
class JuceMidiBackend : public MidiRouter::Backend {
 public:
  JuceMidiBackend(MidiRouter& router, MidiRouter::Port port, std::shared_ptr<juce::MidiOutput> output)
      : MidiRouter::Backend(router, port), midiOut_(std::move(output)) {}

  MidiRouter::PortInfo describe() const override {
    MidiRouter::PortInfo info{};
    info.label = "JUCE";
    info.available = true;
    info.clockIn = info.clockOut = true;
    info.transportIn = info.transportOut = true;
    info.controlChangeIn = info.controlChangeOut = true;
    return info;
  }

  void begin() override {
    std::scoped_lock lock(mutex_);
    incoming_.clear();
  }

  void poll() override {
    std::vector<juce::MidiMessage> queue;
    {
      std::scoped_lock lock(mutex_);
      queue.swap(incoming_);
    }
    for (const auto& msg : queue) {
      handle(msg);
    }
  }

  void sendClock() override { emit(juce::MidiMessage::midiClock()); }
  void sendStart() override { emit(juce::MidiMessage::midiStart()); }
  void sendStop() override { emit(juce::MidiMessage::midiStop()); }

  void sendControlChange(std::uint8_t channel, std::uint8_t controller, std::uint8_t value) override {
    emit(juce::MidiMessage::controllerEvent(static_cast<int>(channel) + 1, controller, value));
  }

  void sendNoteOn(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity) override {
    emit(juce::MidiMessage::noteOn(static_cast<int>(channel) + 1, note, (juce::uint8)velocity));
  }

  void sendNoteOff(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity) override {
    emit(juce::MidiMessage::noteOff(static_cast<int>(channel) + 1, note, (juce::uint8)velocity));
  }

  void sendAllNotesOff(std::uint8_t channel) override {
    emit(juce::MidiMessage::allNotesOff(static_cast<int>(channel) + 1));
  }

  void queueIncoming(const juce::MidiMessage& msg) {
    std::scoped_lock lock(mutex_);
    incoming_.push_back(msg);
  }

  void setOutput(std::shared_ptr<juce::MidiOutput> out) { midiOut_ = std::move(out); }

 private:
  void handle(const juce::MidiMessage& msg) {
    if (msg.isMidiClock()) {
      router_.handleClockFrom(port_);
      return;
    }
    if (msg.isMidiStart()) {
      router_.handleStartFrom(port_);
      return;
    }
    if (msg.isMidiStop()) {
      router_.handleStopFrom(port_);
      return;
    }
    if (msg.isController()) {
      router_.handleControlChangeFrom(port_, static_cast<std::uint8_t>(msg.getChannel() - 1),
                                      static_cast<std::uint8_t>(msg.getControllerNumber()),
                                      static_cast<std::uint8_t>(msg.getControllerValue()));
      return;
    }
    if (msg.isSysEx()) {
      router_.handleSysExFrom(port_, msg.getSysExData(), msg.getSysExDataSize());
      return;
    }
  }

  void emit(const juce::MidiMessage& msg) {
    if (midiOut_) {
      midiOut_->sendMessageNow(msg);
    }
  }

  std::mutex mutex_;
  std::vector<juce::MidiMessage> incoming_;
  std::shared_ptr<juce::MidiOutput> midiOut_{};
};

std::vector<float>& scratch(std::vector<float>& buffer, std::size_t frames) {
  if (buffer.size() != frames) {
    buffer.assign(frames, 0.0f);
  } else {
    std::fill(buffer.begin(), buffer.end(), 0.0f);
  }
  return buffer;
}

}  // namespace

JuceHost::JuceHost(AppState& app) : app_(app) {}

JuceHost::~JuceHost() { stop(); }

void JuceHost::initialiseWithDefaults() {
  ensureMidiOutput();
  auto backend = std::make_unique<JuceMidiBackend>(app_.midi, MidiRouter::Port::kUsb, midiOutput_);
  midiBackend_ = backend.get();
  app_.midi.installBackend(MidiRouter::Port::kUsb, std::move(backend));

  deviceManager_.initialiseWithDefaultDevices(/*numInputChannels*/ 2, /*numOutputChannels*/ 2);
  deviceManager_.addAudioCallback(this);
  deviceManager_.addMidiInputDeviceCallback({}, this);
}

void JuceHost::configureForTests(double sampleRate, int blockSize) {
  if (!bootstrapped_) {
    bootstrapped_ = true;
    app_.initJuceHost(static_cast<float>(sampleRate), static_cast<std::size_t>(blockSize));
  } else {
    hal::audio::configureHostStream(static_cast<float>(sampleRate), static_cast<std::size_t>(blockSize));
  }
}

void JuceHost::audioDeviceAboutToStart(juce::AudioIODevice* device) {
  const double sr = device ? device->getCurrentSampleRate() : 0.0;
  const int block = device ? device->getCurrentBufferSizeSamples() : 0;
  if (!bootstrapped_) {
    bootstrapped_ = true;
    app_.initJuceHost(static_cast<float>(sr), static_cast<std::size_t>(block));
  } else {
    hal::audio::configureHostStream(static_cast<float>(sr), static_cast<std::size_t>(block));
  }
  hal::audio::start();
}

void JuceHost::audioDeviceStopped() { hal::audio::stop(); }

void JuceHost::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                int numInputChannels, float* const* outputChannelData,
                                                int numOutputChannels, int numSamples,
                                                const juce::AudioIODeviceCallbackContext&) {
  for (int ch = 0; ch < numOutputChannels; ++ch) {
    juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
  }
  float* left = numOutputChannels > 0 ? outputChannelData[0] : nullptr;
  float* right = numOutputChannels > 1 ? outputChannelData[1] : left;
  std::size_t frames = static_cast<std::size_t>(std::max(0, numSamples));
  left = left ? left : scratch(scratchLeft_, frames).data();
  right = right ? right : scratch(scratchRight_, frames).data();

  const bool hasInput = inputChannelData && numInputChannels > 0 && frames > 0;
  if (hasInput && inputChannelData[0]) {
    auto& inLeftScratch = scratch(inputScratchLeft_, frames);
    juce::FloatVectorOperations::copy(inLeftScratch.data(), inputChannelData[0], numSamples);

    const bool hasRight = numInputChannels > 1 && inputChannelData[1];
    const float* rightInput = nullptr;
    if (hasRight) {
      auto& inRightScratch = scratch(inputScratchRight_, frames);
      juce::FloatVectorOperations::copy(inRightScratch.data(), inputChannelData[1], numSamples);
      rightInput = inRightScratch.data();
    } else {
      inputScratchRight_.clear();
    }

    app_.setDryInputFromHost(inLeftScratch.data(), rightInput, frames);
  } else {
    inputScratchLeft_.clear();
    inputScratchRight_.clear();
    app_.setDryInputFromHost(nullptr, nullptr, 0);
  }

  hal::audio::renderHostBuffer(left, right, frames);
  app_.midi.poll();
  const bool enginesAudible = hal::audio::bufferHasEngineEnergy(left, right, frames,
                                                                hal::audio::kEnginePassthroughFloor,
                                                                hal::audio::kEngineIdleRmsSlack);
  const bool dryAudible = hasInput && !inputScratchLeft_.empty() &&
                          hal::audio::bufferHasEngineEnergy(inputScratchLeft_.data(),
                                                            inputScratchRight_.empty()
                                                                ? nullptr
                                                                : inputScratchRight_.data(),
                                                            frames, hal::audio::kEnginePassthroughFloor,
                                                            hal::audio::kEngineIdleRmsSlack);

  if (dryAudible && !enginesAudible && numOutputChannels > 0) {
    const float* inLeft = inputScratchLeft_.data();
    const float* inRight = inputScratchRight_.empty() ? nullptr : inputScratchRight_.data();

    juce::FloatVectorOperations::copy(outputChannelData[0], inLeft, numSamples);
    if (numOutputChannels > 1) {
      juce::FloatVectorOperations::copy(outputChannelData[1], inRight ? inRight : inLeft, numSamples);
    }
  }

  app_.tick();
}

void JuceHost::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message) {
  if (auto* backend = dynamic_cast<JuceMidiBackend*>(midiBackend_)) {
    backend->queueIncoming(message);
  }
}

void JuceHost::stop() {
  deviceManager_.removeAudioCallback(this);
  deviceManager_.removeMidiInputDeviceCallback({}, this);
  hal::audio::stop();
}

void JuceHost::ensureMidiOutput() {
  if (!midiOutput_) {
    if (auto out = deviceManager_.getDefaultMidiOutput()) {
      midiOutput_ = std::shared_ptr<juce::MidiOutput>(std::move(out));
    }
  }
  if (auto* backend = dynamic_cast<JuceMidiBackend*>(midiBackend_)) {
    backend->setOutput(midiOutput_);
  }
}

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
