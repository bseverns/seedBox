#include "juce/JuceHost.h"

#if SEEDBOX_JUCE

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <utility>

#include "hal/hal_audio.h"

namespace seedbox::juce_bridge {

namespace {
constexpr std::size_t kMinPreparedScratchFrames = 8192;
constexpr int kHostMaintenanceHz = 15;
class JuceMidiBackend : public MidiRouter::Backend {
 public:
  static constexpr std::size_t kQueueCapacity = 256;

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
    readIndex_.store(0, std::memory_order_relaxed);
    writeIndex_.store(0, std::memory_order_relaxed);
  }

  void poll() override {
    while (true) {
      const auto read = readIndex_.load(std::memory_order_relaxed);
      const auto write = writeIndex_.load(std::memory_order_acquire);
      if (read == write) {
        break;
      }
      handle(incoming_[read]);
      readIndex_.store((read + 1u) % kQueueCapacity, std::memory_order_release);
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
    const auto write = writeIndex_.load(std::memory_order_relaxed);
    const auto next = (write + 1u) % kQueueCapacity;
    const auto read = readIndex_.load(std::memory_order_acquire);
    if (next == read) {
      droppedCount_.fetch_add(1u, std::memory_order_relaxed);
      return;
    }
    incoming_[write] = msg;
    writeIndex_.store(next, std::memory_order_release);
  }

  std::uint32_t droppedCount() const { return droppedCount_.load(std::memory_order_relaxed); }

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

  std::array<juce::MidiMessage, kQueueCapacity> incoming_{};
  std::atomic<std::size_t> readIndex_{0};
  std::atomic<std::size_t> writeIndex_{0};
  std::atomic<std::uint32_t> droppedCount_{0};
  std::shared_ptr<juce::MidiOutput> midiOut_{};
};

}  // namespace

class JuceHost::MaintenanceTimer final : private juce::Timer {
 public:
  explicit MaintenanceTimer(JuceHost& host, HostControlThreadAccess& controlThread)
      : host_(host), controlThread_(controlThread) {}

  void start() { startTimerHz(kHostMaintenanceHz); }
  void stop() { stopTimer(); }

 private:
  void timerCallback() override {
    controlThread_.publishHostDiagnostics(host_.hostDiagnostics());
    controlThread_.serviceMaintenance();
  }

  JuceHost& host_;
  HostControlThreadAccess& controlThread_;
};

JuceHost::JuceHost(AppState& app) : app_(app), audioThreadApp_(app), readThreadApp_(app), controlThreadApp_(app) {}

JuceHost::~JuceHost() { stop(); }

std::uint32_t JuceHost::midiDroppedCount() const {
  if (auto* backend = dynamic_cast<JuceMidiBackend*>(midiBackend_)) {
    return backend->droppedCount();
  }
  return 0u;
}

AppState::DiagnosticsSnapshot::HostRuntime JuceHost::hostDiagnostics() const {
  AppState::DiagnosticsSnapshot::HostRuntime host{};
  host.midiDroppedCount = midiDroppedCount();
  host.oversizeBlockDropCount = oversizeBlockDropCount_.load(std::memory_order_relaxed);
  host.lastOversizeBlockFrames =
      static_cast<std::uint32_t>(lastOversizeBlockFrames_.load(std::memory_order_relaxed));
  host.preparedScratchFrames =
      static_cast<std::uint32_t>(preparedScratchFrames_.load(std::memory_order_relaxed));
  return host;
}

void JuceHost::prepareScratchBuffers(int blockSize) {
  const std::size_t preparedFrames =
      static_cast<std::size_t>(std::max(blockSize, static_cast<int>(kMinPreparedScratchFrames)));
  preparedScratchFrames_.store(preparedFrames, std::memory_order_relaxed);
  inputScratchLeft_.assign(preparedFrames, 0.0f);
  inputScratchRight_.assign(preparedFrames, 0.0f);
  scratchLeft_.assign(preparedFrames, 0.0f);
  scratchRight_.assign(preparedFrames, 0.0f);
}

void JuceHost::startMaintenanceTimer() {
  if (!maintenanceTimer_) {
    maintenanceTimer_ = std::make_unique<MaintenanceTimer>(*this, controlThreadApp_);
  }
  maintenanceTimer_->start();
}

void JuceHost::stopMaintenanceTimer() {
  if (maintenanceTimer_) {
    maintenanceTimer_->stop();
  }
}

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
  prepareScratchBuffers(blockSize);
  if (!bootstrapped_) {
    bootstrapped_ = true;
    app_.initJuceHost(static_cast<float>(sampleRate), static_cast<std::size_t>(blockSize));
  } else {
    hal::audio::configureHostStream(static_cast<float>(sampleRate), static_cast<std::size_t>(blockSize));
  }
  startMaintenanceTimer();
}

void JuceHost::audioDeviceAboutToStart(juce::AudioIODevice* device) {
  const double sr = device ? device->getCurrentSampleRate() : 0.0;
  const int block = device ? device->getCurrentBufferSizeSamples() : 0;
  prepareScratchBuffers(block);
  if (!bootstrapped_) {
    bootstrapped_ = true;
    app_.initJuceHost(static_cast<float>(sr), static_cast<std::size_t>(block));
  } else {
    hal::audio::configureHostStream(static_cast<float>(sr), static_cast<std::size_t>(block));
  }
  hal::audio::start();
  startMaintenanceTimer();
}

void JuceHost::audioDeviceStopped() {
  stopMaintenanceTimer();
  hal::audio::stop();
}

void JuceHost::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                int numInputChannels, float* const* outputChannelData,
                                                int numOutputChannels, int numSamples,
                                                const juce::AudioIODeviceCallbackContext&) {
  for (int ch = 0; ch < numOutputChannels; ++ch) {
    juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
  }
  std::size_t frames = static_cast<std::size_t>(std::max(0, numSamples));
  const std::size_t preparedFrames = preparedScratchFrames_.load(std::memory_order_relaxed);
  if (frames > preparedFrames) {
    oversizeBlockDropCount_.fetch_add(1u, std::memory_order_relaxed);
    lastOversizeBlockFrames_.store(frames, std::memory_order_relaxed);
    jassertfalse;
    audioThreadApp_.setDryInput(nullptr, nullptr, 0);
    return;
  }
  float* left = numOutputChannels > 0 ? outputChannelData[0] : nullptr;
  float* right = numOutputChannels > 1 ? outputChannelData[1] : left;
  if (!left) {
    std::fill_n(scratchLeft_.data(), frames, 0.0f);
    left = scratchLeft_.data();
  }
  if (!right) {
    std::fill_n(scratchRight_.data(), frames, 0.0f);
    right = scratchRight_.data();
  }

  const bool hasInput = inputChannelData && numInputChannels > 0 && frames > 0;
  const bool hasRightInput = hasInput && numInputChannels > 1 && inputChannelData[1];
  if (hasInput && inputChannelData[0]) {
    juce::FloatVectorOperations::copy(inputScratchLeft_.data(), inputChannelData[0], numSamples);
    if (hasRightInput) {
      juce::FloatVectorOperations::copy(inputScratchRight_.data(), inputChannelData[1], numSamples);
    }
    audioThreadApp_.setDryInput(inputScratchLeft_.data(), hasRightInput ? inputScratchRight_.data() : nullptr,
                                frames);
  } else {
    audioThreadApp_.setDryInput(nullptr, nullptr, 0);
  }

  hal::audio::renderHostBuffer(left, right, frames);
  app_.midi.poll();
  const bool enginesAudible = hal::audio::bufferHasEngineEnergy(left, right, frames,
                                                                hal::audio::kEnginePassthroughFloor,
                                                                hal::audio::kEngineIdleRmsSlack);
  const bool dryAudible = hasInput &&
                          hal::audio::bufferHasEngineEnergy(inputScratchLeft_.data(),
                                                            hasRightInput ? inputScratchRight_.data() : nullptr,
                                                            frames, hal::audio::kEnginePassthroughFloor,
                                                            hal::audio::kEngineIdleRmsSlack);

  if (dryAudible && !enginesAudible && numOutputChannels > 0) {
    const float* inLeft = inputScratchLeft_.data();
    const float* inRight = hasRightInput ? inputScratchRight_.data() : nullptr;

    juce::FloatVectorOperations::copy(outputChannelData[0], inLeft, numSamples);
    if (numOutputChannels > 1) {
      juce::FloatVectorOperations::copy(outputChannelData[1], inRight ? inRight : inLeft, numSamples);
    }
  }

  audioThreadApp_.tick();
}

void JuceHost::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message) {
  if (auto* backend = dynamic_cast<JuceMidiBackend*>(midiBackend_)) {
    backend->queueIncoming(message);
  }
}

void JuceHost::stop() {
  stopMaintenanceTimer();
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
