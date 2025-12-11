#include "juce/SeedboxAudioProcessor.h"

#if SEEDBOX_JUCE

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>

#include <array>
#include <cstring>
#include <utility>

#include "hal/hal_audio.h"
#include "juce/SeedboxAudioProcessorEditor.h"

namespace seedbox::juce_bridge {

namespace {
constexpr auto kParamMasterSeed = "masterSeed";
constexpr auto kParamFocusSeed = "focusSeed";
constexpr auto kParamSeedEngine = "seedEngine";
constexpr auto kParamSwingPercent = "swingPercent";
constexpr auto kParamQuantizeScale = "quantizeScale";
constexpr auto kParamQuantizeRoot = "quantizeRoot";
constexpr auto kParamTransportLatch = "transportLatch";
constexpr auto kParamClockSourceExternal = "clockSourceExternal";
constexpr auto kParamFollowExternalClock = "followExternalClock";
constexpr auto kParamDebugMeters = "debugMeters";
constexpr auto kParamGranularSourceStep = "granularSourceStep";
constexpr auto kParamPresetSlot = "presetSlot";
constexpr auto kStatePresetData = "presetData";
}

SeedboxAudioProcessor::SeedboxAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Main", juce::AudioChannelSet::stereo(), true)),
      app_(hal::board()),
      parameters_(*this, nullptr, "SeedBoxParameters", createParameterLayout()) {
  auto backend = std::make_unique<ProcessorMidiBackend>(app_.midi, MidiRouter::Port::kUsb);
  midiBackend_ = backend.get();
  app_.midi.installBackend(MidiRouter::Port::kUsb, std::move(backend));
  const std::array<const char*, 11> ids = {kParamMasterSeed,
                                           kParamFocusSeed,
                                           kParamSeedEngine,
                                           kParamSwingPercent,
                                           kParamQuantizeScale,
                                           kParamQuantizeRoot,
                                           kParamTransportLatch,
                                           kParamClockSourceExternal,
                                           kParamFollowExternalClock,
                                           kParamDebugMeters,
                                           kParamGranularSourceStep};
  for (auto* id : ids) {
    parameters_.addParameterListener(id, this);
    parameterState_[id] = parameters_.getRawParameterValue(id)->load();
  }
  quantizeScaleParam_ = static_cast<std::uint8_t>(parameterState_[kParamQuantizeScale]);
  quantizeRootParam_ = static_cast<std::uint8_t>(parameterState_[kParamQuantizeRoot]);
}

SeedboxAudioProcessor::~SeedboxAudioProcessor() {
  const std::array<const char*, 11> ids = {kParamMasterSeed,
                                           kParamFocusSeed,
                                           kParamSeedEngine,
                                           kParamSwingPercent,
                                           kParamQuantizeScale,
                                           kParamQuantizeRoot,
                                           kParamTransportLatch,
                                           kParamClockSourceExternal,
                                           kParamFollowExternalClock,
                                           kParamDebugMeters,
                                           kParamGranularSourceStep};
  for (auto* id : ids) {
    parameters_.removeParameterListener(id, this);
  }
}

void SeedboxAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  if (!prepared_) {
    prepared_ = true;
    app_.initJuceHost(static_cast<float>(sampleRate), static_cast<std::size_t>(samplesPerBlock));
    applyPendingPresetIfAny();
  } else {
    hal::audio::configureHostStream(static_cast<float>(sampleRate), static_cast<std::size_t>(samplesPerBlock));
  }
}

void SeedboxAudioProcessor::releaseResources() { hal::audio::stop(); }

bool SeedboxAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) {
    return false;
  }
  return true;
}

void SeedboxAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  buffer.clear();
  midiBackend_->setOutboundBuffer(&midiMessages);

  std::vector<BufferedMidiMessage> incoming;
  incoming.reserve(static_cast<std::size_t>(midiMessages.getNumEvents()));
  for (const auto metadata : midiMessages) {
    incoming.push_back({metadata.getMessage(), metadata.samplePosition});
  }
  midiMessages.clear();
  for (const auto& msg : incoming) {
    midiBackend_->queueIncoming(msg);
  }

  const int numSamples = buffer.getNumSamples();
  float* left = buffer.getWritePointer(0);
  float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;
  hal::audio::renderHostBuffer(left, right, static_cast<std::size_t>(numSamples));
  app_.midi.poll();
}

juce::AudioProcessorEditor* SeedboxAudioProcessor::createEditor() { return new SeedboxAudioProcessorEditor(*this); }

void SeedboxAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
  auto state = parameters_.copyState();
  state.setProperty(kParamPresetSlot, juce::String(app_.activePresetSlot()), nullptr);

  const seedbox::Preset snapshot = app_.snapshotPresetForHost(app_.activePresetSlot());
  auto serialized = snapshot.serialize();
  juce::MemoryBlock presetBlock(serialized.data(), serialized.size());
  state.setProperty(kStatePresetData, presetBlock.toBase64Encoding(), nullptr);

  juce::MemoryOutputStream stream(destData, true);
  state.writeToStream(stream);
}

void SeedboxAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
  if (data == nullptr || sizeInBytes <= 0) {
    return;
  }
  juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
  auto tree = juce::ValueTree::readFromStream(stream);
  if (!tree.isValid()) {
    return;
  }

  parameters_.replaceState(tree);

  auto presetBase64 = tree.getProperty(kStatePresetData).toString();
  if (presetBase64.isNotEmpty()) {
    juce::MemoryBlock block;
    if (block.fromBase64Encoding(presetBase64)) {
      std::vector<std::uint8_t> payload(block.getSize());
      std::memcpy(payload.data(), block.getData(), block.getSize());
      seedbox::Preset preset;
      if (seedbox::Preset::deserialize(payload, preset)) {
        if (prepared_) {
          app_.applyPresetFromHost(preset, false);
        } else {
          pendingPreset_ = std::move(preset);
        }
      }
    }
  }
}

void SeedboxAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue) {
  if (parameterID == kParamMasterSeed) {
    app_.reseed(static_cast<std::uint32_t>(newValue));
    parameterState_[kParamMasterSeed] = newValue;
    return;
  }

  if (parameterID == kParamFocusSeed) {
    app_.setFocusSeed(static_cast<std::uint8_t>(newValue));
    parameterState_[kParamFocusSeed] = newValue;
    return;
  }

  if (parameterID == kParamSeedEngine) {
    app_.setSeedEngine(app_.focusSeed(), static_cast<std::uint8_t>(newValue));
    parameterState_[kParamSeedEngine] = newValue;
    return;
  }

  if (parameterID == kParamSwingPercent) {
    app_.setSwingPercentFromHost(newValue);
    parameterState_[kParamSwingPercent] = newValue;
    return;
  }

  if (parameterID == kParamQuantizeScale) {
    quantizeScaleParam_ = static_cast<std::uint8_t>(newValue);
    const auto control = static_cast<std::uint8_t>((quantizeScaleParam_ * 32u) + (quantizeRootParam_ % 12u));
    app_.applyQuantizeControlFromHost(control);
    parameterState_[kParamQuantizeScale] = newValue;
    return;
  }

  if (parameterID == kParamQuantizeRoot) {
    quantizeRootParam_ = static_cast<std::uint8_t>(newValue);
    const auto control = static_cast<std::uint8_t>((quantizeScaleParam_ * 32u) + (quantizeRootParam_ % 12u));
    app_.applyQuantizeControlFromHost(control);
    parameterState_[kParamQuantizeRoot] = newValue;
    return;
  }

  if (parameterID == kParamTransportLatch) {
    app_.setTransportLatchFromHost(newValue >= 0.5f);
    parameterState_[kParamTransportLatch] = newValue;
    return;
  }

  if (parameterID == kParamClockSourceExternal) {
    app_.setClockSourceExternalFromHost(newValue >= 0.5f);
    parameterState_[kParamClockSourceExternal] = newValue;
    return;
  }

  if (parameterID == kParamFollowExternalClock) {
    app_.setFollowExternalClockFromHost(newValue >= 0.5f);
    parameterState_[kParamFollowExternalClock] = newValue;
    return;
  }

  if (parameterID == kParamDebugMeters) {
    app_.setDebugMetersEnabledFromHost(newValue >= 0.5f);
    parameterState_[kParamDebugMeters] = newValue;
    return;
  }

  if (parameterID == kParamGranularSourceStep) {
    const float previous = parameterState_[kParamGranularSourceStep];
    const auto delta = static_cast<int>(newValue - previous);
    if (delta != 0) {
      app_.seedPageCycleGranularSource(app_.focusSeed(), delta);
    }
    parameterState_[kParamGranularSourceStep] = newValue;
    return;
  }
}

juce::AudioProcessorValueTreeState::ParameterLayout SeedboxAudioProcessor::createParameterLayout() {
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
  params.push_back(std::make_unique<juce::AudioParameterInt>(kParamMasterSeed, "Master Seed", 0, 9999999, 1));
  params.push_back(std::make_unique<juce::AudioParameterInt>(kParamFocusSeed, "Focus Seed", 0, 3, 0));
  params.push_back(std::make_unique<juce::AudioParameterInt>(kParamSeedEngine, "Seed Engine", 0, 7, 0));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      kParamSwingPercent, "Swing Percent",
      juce::NormalisableRange<float>{0.0f, 0.99f, 0.01f, 1.0f, false}, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterInt>(kParamQuantizeScale, "Quantize Scale", 0, 4, 0));
  params.push_back(std::make_unique<juce::AudioParameterInt>(kParamQuantizeRoot, "Quantize Root", 0, 11, 0));
  params.push_back(std::make_unique<juce::AudioParameterBool>(kParamTransportLatch, "Transport Latch", false));
  params.push_back(
      std::make_unique<juce::AudioParameterBool>(kParamClockSourceExternal, "Clock Source External", false));
  params.push_back(
      std::make_unique<juce::AudioParameterBool>(kParamFollowExternalClock, "Follow External Clock", false));
  params.push_back(std::make_unique<juce::AudioParameterBool>(kParamDebugMeters, "Debug Meters", false));
  params.push_back(std::make_unique<juce::AudioParameterInt>(kParamGranularSourceStep, "Granular Source Step", -8, 8,
                                                            0));
  return {params.begin(), params.end()};
}

void SeedboxAudioProcessor::applyPendingPresetIfAny() {
  if (!pendingPreset_.has_value()) {
    return;
  }
  app_.applyPresetFromHost(*pendingPreset_, false);
  pendingPreset_.reset();
}

SeedboxAudioProcessor::ProcessorMidiBackend::ProcessorMidiBackend(MidiRouter& router, MidiRouter::Port port)
    : MidiRouter::Backend(router, port) {}

MidiRouter::PortInfo SeedboxAudioProcessor::ProcessorMidiBackend::describe() const {
  MidiRouter::PortInfo info{};
  info.label = "JUCE Plugin";
  info.available = true;
  info.clockIn = info.clockOut = true;
  info.transportIn = info.transportOut = true;
  info.controlChangeIn = info.controlChangeOut = true;
  return info;
}

void SeedboxAudioProcessor::ProcessorMidiBackend::begin() { incoming_.clear(); }

void SeedboxAudioProcessor::ProcessorMidiBackend::poll() {
  auto queue = std::move(incoming_);
  incoming_.clear();
  for (const auto& msg : queue) {
    handle(msg);
  }
}

void SeedboxAudioProcessor::ProcessorMidiBackend::sendClock() { emit(juce::MidiMessage::midiClock(), 0); }
void SeedboxAudioProcessor::ProcessorMidiBackend::sendStart() { emit(juce::MidiMessage::midiStart(), 0); }
void SeedboxAudioProcessor::ProcessorMidiBackend::sendStop() { emit(juce::MidiMessage::midiStop(), 0); }

void SeedboxAudioProcessor::ProcessorMidiBackend::sendControlChange(std::uint8_t channel, std::uint8_t controller,
                                                                    std::uint8_t value) {
  emit(juce::MidiMessage::controllerEvent(static_cast<int>(channel) + 1, controller, value), 0);
}

void SeedboxAudioProcessor::ProcessorMidiBackend::sendNoteOn(std::uint8_t channel, std::uint8_t note,
                                                             std::uint8_t velocity) {
  emit(juce::MidiMessage::noteOn(static_cast<int>(channel) + 1, note, static_cast<juce::uint8>(velocity)), 0);
}

void SeedboxAudioProcessor::ProcessorMidiBackend::sendNoteOff(std::uint8_t channel, std::uint8_t note,
                                                              std::uint8_t velocity) {
  emit(juce::MidiMessage::noteOff(static_cast<int>(channel) + 1, note, static_cast<juce::uint8>(velocity)), 0);
}

void SeedboxAudioProcessor::ProcessorMidiBackend::sendAllNotesOff(std::uint8_t channel) {
  emit(juce::MidiMessage::allNotesOff(static_cast<int>(channel) + 1), 0);
}

void SeedboxAudioProcessor::ProcessorMidiBackend::queueIncoming(const BufferedMidiMessage& msg) {
  incoming_.push_back(msg);
}

void SeedboxAudioProcessor::ProcessorMidiBackend::handle(const BufferedMidiMessage& msg) {
  const auto& midi = msg.message;
  if (midi.isMidiClock()) {
    router_.handleClockFrom(port_);
    return;
  }
  if (midi.isMidiStart()) {
    router_.handleStartFrom(port_);
    return;
  }
  if (midi.isMidiStop()) {
    router_.handleStopFrom(port_);
    return;
  }
  if (midi.isController()) {
    router_.handleControlChangeFrom(port_, static_cast<std::uint8_t>(midi.getChannel() - 1),
                                    static_cast<std::uint8_t>(midi.getControllerNumber()),
                                    static_cast<std::uint8_t>(midi.getControllerValue()));
    return;
  }
  if (midi.isSysEx()) {
    router_.handleSysExFrom(port_, midi.getSysExData(), midi.getSysExDataSize());
    return;
  }
}

void SeedboxAudioProcessor::ProcessorMidiBackend::emit(const juce::MidiMessage& msg, int position) {
  if (midiOut_) {
    midiOut_->addEvent(msg, position);
  }
}

}  // namespace seedbox::juce_bridge

// JUCE looks for this factory to wire the plugin binary. Keep it down here so
// it stays out of the way of the engine-heavy bits above.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new seedbox::juce_bridge::SeedboxAudioProcessor();
}

#endif  // SEEDBOX_JUCE
