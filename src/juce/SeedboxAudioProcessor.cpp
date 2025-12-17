#include "juce/SeedboxAudioProcessor.h"

#if SEEDBOX_JUCE

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <utility>

#include "hal/hal_audio.h"
#include "juce/SeedboxAudioProcessorEditor.h"
#include "Seed.h"

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
constexpr auto kStatePanelPreset = "panelPreset";
constexpr auto kStateRoot = "seedboxState";
constexpr auto kSeedNodePrefix = "seed";

const juce::Identifier kPropEngineId{"engineId"};
const juce::Identifier kPropTone{"tone"};
const juce::Identifier kPropDensity{"density"};
const juce::Identifier kPropProbability{"probability"};
const juce::Identifier kPropSpread{"spread"};
const juce::Identifier kPropJitterMs{"jitterMs"};
const juce::Identifier kPropEnvRelease{"envRelease"};
const juce::Identifier kPropGrainSizeMs{"grainSizeMs"};
const juce::Identifier kPropGrainSprayMs{"grainSprayMs"};
const juce::Identifier kPropGranularTranspose{"granularTranspose"};
const juce::Identifier kPropGranularWindowSkew{"granularWindowSkew"};
const juce::Identifier kPropResonatorMode{"resonatorMode"};
const juce::Identifier kPropResonatorBank{"resonatorBank"};
const juce::Identifier kPropResonatorFeedback{"resonatorFeedback"};
const juce::Identifier kPropResonatorDamping{"resonatorDamping"};

constexpr std::array<const char*, 11> kParameterIds = {kParamMasterSeed,      kParamFocusSeed,
                                                        kParamSeedEngine,     kParamSwingPercent,
                                                        kParamQuantizeScale,  kParamQuantizeRoot,
                                                        kParamTransportLatch, kParamClockSourceExternal,
                                                        kParamFollowExternalClock, kParamDebugMeters,
                                                        kParamGranularSourceStep};
}

SeedboxAudioProcessor::SeedboxAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Main", juce::AudioChannelSet::stereo(), true)),
      app_(hal::board()),
      parameters_(*this, nullptr, "SeedBoxParameters", createParameterLayout()) {
  auto backend = std::make_unique<ProcessorMidiBackend>(app_.midi, MidiRouter::Port::kUsb);
  midiBackend_ = backend.get();
  app_.midi.installBackend(MidiRouter::Port::kUsb, std::move(backend));
  for (auto* id : kParameterIds) {
    parameters_.addParameterListener(id, this);
    parameterState_[id] = parameters_.getRawParameterValue(id)->load();
  }
  quantizeScaleParam_ = static_cast<std::uint8_t>(parameterState_[kParamQuantizeScale]);
  quantizeRootParam_ = static_cast<std::uint8_t>(parameterState_[kParamQuantizeRoot]);
}

SeedboxAudioProcessor::~SeedboxAudioProcessor() {
  for (auto* id : kParameterIds) {
    parameters_.removeParameterListener(id, this);
  }
}

void SeedboxAudioProcessor::requestShutdown() {
#if JucePlugin_Build_Standalone
  if (auto* app = juce::JUCEApplicationBase::getInstance()) {
    app->systemRequestedQuit();
  }
#endif
}

void SeedboxAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  if (!prepared_) {
    prepared_ = true;
    app_.initJuceHost(static_cast<float>(sampleRate), static_cast<std::size_t>(samplesPerBlock));
    applyPendingPresetIfAny();
    applySeedStateToApp();
    syncSeedStateFromApp();
  } else {
    hal::audio::configureHostStream(static_cast<float>(sampleRate), static_cast<std::size_t>(samplesPerBlock));
  }
}

void SeedboxAudioProcessor::releaseResources() { hal::audio::stop(); }

bool SeedboxAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) {
    return false;
  }

  const auto input = layouts.getMainInputChannelSet();
  if (input.isDisabled() || input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo()) {
    return true;
  }
  return false;
}

void SeedboxAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  const int numSamples = buffer.getNumSamples();
  const int numInputs = getTotalNumInputChannels();
  const int numOutputs = getTotalNumOutputChannels();

  auto inputBus = getBusBuffer(buffer, true, 0);
  auto outputBus = getBusBuffer(buffer, false, 0);

  if (numInputs > 0) {
    inputScratch_.setSize(numInputs, numSamples, false, false, true);
    for (int ch = 0; ch < numInputs; ++ch) {
      juce::FloatVectorOperations::copy(inputScratch_.getWritePointer(ch), inputBus.getReadPointer(ch), numSamples);
    }
  } else {
    inputScratch_.setSize(0, 0, false, false, true);
  }

  renderScratch_.setSize(std::max(2, numOutputs), numSamples, false, false, true);
  renderScratch_.clear();
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

  float* renderLeft = renderScratch_.getWritePointer(0);
  float* renderRight = renderScratch_.getNumChannels() > 1 ? renderScratch_.getWritePointer(1) : renderLeft;
  hal::audio::renderHostBuffer(renderLeft, renderRight, static_cast<std::size_t>(numSamples));
  app_.midi.poll();
  const bool engineHasOutput = renderScratch_.getRMSLevel(0, 0, numSamples) > 0.0f ||
                               (renderScratch_.getNumChannels() > 1 &&
                                renderScratch_.getRMSLevel(1, 0, numSamples) > 0.0f);

  float* outLeft = outputBus.getWritePointer(0);
  float* outRight = outputBus.getNumChannels() > 1 ? outputBus.getWritePointer(1) : outLeft;

  if (engineHasOutput) {
    juce::FloatVectorOperations::copy(outLeft, renderScratch_.getReadPointer(0), numSamples);
    if (outputBus.getNumChannels() > 1) {
      juce::FloatVectorOperations::copy(outRight, renderScratch_.getReadPointer(1), numSamples);
    } else {
      juce::FloatVectorOperations::copy(outLeft, renderScratch_.getReadPointer(0), numSamples);
    }
  } else if (numInputs > 0 && inputScratch_.getNumSamples() == numSamples) {
    const float* inLeft = inputScratch_.getReadPointer(0);
    const bool hasRight = inputScratch_.getNumChannels() > 1;
    const float* inRight = hasRight ? inputScratch_.getReadPointer(1) : inLeft;

    juce::FloatVectorOperations::copy(outLeft, inLeft, numSamples);
    if (outputBus.getNumChannels() > 1) {
      juce::FloatVectorOperations::copy(outRight, inRight, numSamples);
    } else {
      // duplicate mono/left into single bus when hosts give us one channel
      juce::FloatVectorOperations::copy(outLeft, inLeft, numSamples);
    }
  } else {
    outputBus.clear();
  }

  app_.tick();
}

juce::AudioProcessorEditor* SeedboxAudioProcessor::createEditor() { return new SeedboxAudioProcessorEditor(*this); }

void SeedboxAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
  syncSeedStateFromApp();
  auto state = parameters_.copyState();
  state.setProperty(kParamPresetSlot, juce::String(app_.activePresetSlot()), nullptr);

  const seedbox::Preset snapshot = app_.snapshotPresetForHost(app_.activePresetSlot());
  auto serialized = snapshot.serialize();
  juce::MemoryBlock presetBlock(serialized.data(), serialized.size());
  state.setProperty(kStatePresetData, presetBlock.toBase64Encoding(), nullptr);
  state.setProperty(kStatePanelPreset, panelPresetBase64_, nullptr);

  juce::ValueTree root(kStateRoot);
  root.addChild(state, 0, nullptr);
  juce::MemoryOutputStream stream(destData, true);
  root.writeToStream(stream);
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

  juce::ValueTree parameterTree = tree;
  if (tree.hasType(kStateRoot)) {
    parameterTree = tree.getChildWithName(parameters_.state.getType());
    if (!parameterTree.isValid() && tree.getNumChildren() > 0) {
      parameterTree = tree.getChild(0);
    }
  }

  if (parameterTree.isValid()) {
    parameters_.replaceState(parameterTree);
    applySeedStateToApp();
  }

  auto presetBase64 = tree.getProperty(kStatePresetData).toString();
  if (presetBase64.isEmpty() && parameterTree.isValid()) {
    presetBase64 = parameterTree.getProperty(kStatePresetData).toString();
  }
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

  auto panelPresetBase64 = tree.getProperty(kStatePanelPreset).toString();
  if (panelPresetBase64.isEmpty() && parameterTree.isValid()) {
    panelPresetBase64 = parameterTree.getProperty(kStatePanelPreset).toString();
  }
  if (panelPresetBase64.isNotEmpty()) {
    juce::MemoryBlock block;
    if (block.fromBase64Encoding(panelPresetBase64)) {
      std::vector<std::uint8_t> payload(block.getSize());
      std::memcpy(payload.data(), block.getData(), block.getSize());
      seedbox::Preset preset;
      if (seedbox::Preset::deserialize(payload, preset)) {
        panelPreset_ = preset;
        panelPresetBase64_ = panelPresetBase64;
      }
    }
  }
}

juce::String SeedboxAudioProcessor::serializePresetToBase64(const seedbox::Preset& preset) const {
  auto serialized = preset.serialize();
  juce::MemoryBlock presetBlock(serialized.data(), serialized.size());
  return presetBlock.toBase64Encoding();
}

void SeedboxAudioProcessor::setPanelQuickPreset(const seedbox::Preset& preset) {
  panelPreset_ = preset;
  panelPresetBase64_ = serializePresetToBase64(preset);
}

bool SeedboxAudioProcessor::applyPanelQuickPreset() {
  if (!panelPreset_.has_value()) {
    return false;
  }
  if (prepared_) {
    app_.applyPresetFromHost(*panelPreset_, false);
  } else {
    pendingPreset_ = *panelPreset_;
  }
  return true;
}

void SeedboxAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue) {
  if (parameterID == kParamMasterSeed) {
    app_.reseed(static_cast<std::uint32_t>(newValue));
    syncSeedStateFromApp();
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
    setSeedProp(static_cast<int>(app_.focusSeed()), kPropEngineId,
                static_cast<int>(static_cast<std::uint8_t>(newValue)));
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
  const Seed defaults{};
  params.push_back(std::make_unique<juce::AudioParameterInt>(kParamMasterSeed, "Master Seed", 0, 9999999, 1));

  // Choice labels must stay in lockstep with the combo boxes built in
  // SeedboxAudioProcessorEditor so that automation/preset values map correctly.
  juce::StringArray focusSeedChoices{"Seed 1", "Seed 2", "Seed 3", "Seed 4"};
  params.push_back(std::make_unique<juce::AudioParameterChoice>(kParamFocusSeed, "Focus Seed", focusSeedChoices, 0));

  juce::StringArray engineChoices{"Default", "Grain", "Chord", "Drum", "FM", "Additive", "Resonator", "Noise"};
  params.push_back(std::make_unique<juce::AudioParameterChoice>(kParamSeedEngine, "Seed Engine", engineChoices, 0));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      kParamSwingPercent, "Swing Percent",
      juce::NormalisableRange<float>{0.0f, 0.99f, 0.01f, 1.0f, false}, 0.0f));

  juce::StringArray quantizeScaleChoices{"Chromatic", "Major", "Minor", "Dorian", "Lydian"};
  params.push_back(
      std::make_unique<juce::AudioParameterChoice>(kParamQuantizeScale, "Quantize Scale", quantizeScaleChoices, 0));

  juce::StringArray quantizeRootChoices{"C",  "C#", "D",  "D#", "E",  "F",
                                        "F#", "G",  "G#", "A",  "A#", "B"};
  params.push_back(
      std::make_unique<juce::AudioParameterChoice>(kParamQuantizeRoot, "Quantize Root", quantizeRootChoices, 0));
  params.push_back(std::make_unique<juce::AudioParameterBool>(kParamTransportLatch, "Transport Latch", false));
  params.push_back(
      std::make_unique<juce::AudioParameterBool>(kParamClockSourceExternal, "Clock Source External", false));
  params.push_back(
      std::make_unique<juce::AudioParameterBool>(kParamFollowExternalClock, "Follow External Clock", false));
  params.push_back(std::make_unique<juce::AudioParameterBool>(kParamDebugMeters, "Debug Meters", false));

  // Discrete source index delta sent to app_.seedPageCycleGranularSource() when the UI nudge changes.
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

juce::ValueTree SeedboxAudioProcessor::findSeedNode(int idx) const {
  const juce::Identifier name(juce::String(kSeedNodePrefix) + juce::String(idx));
  return parameters_.state.getChildWithName(name);
}

juce::ValueTree SeedboxAudioProcessor::getOrCreateSeedNode(int idx) {
  auto node = findSeedNode(idx);
  if (!node.isValid()) {
    node = juce::ValueTree(juce::Identifier(juce::String(kSeedNodePrefix) + juce::String(idx)));
    parameters_.state.addChild(node, static_cast<int>(parameters_.state.getNumChildren()), nullptr);
  }
  return node;
}

void SeedboxAudioProcessor::setSeedProp(int idx, const juce::Identifier& key, const juce::var& value) {
  auto node = getOrCreateSeedNode(idx);
  node.setProperty(key, value, nullptr);
}

juce::var SeedboxAudioProcessor::getSeedProp(int idx, const juce::Identifier& key, juce::var defaultValue) const {
  auto node = findSeedNode(idx);
  if (!node.isValid()) {
    return defaultValue;
  }
  auto prop = node.getProperty(key, defaultValue);
  if (prop.isVoid()) {
    return defaultValue;
  }
  return prop;
}

void SeedboxAudioProcessor::applySeedEdit(const juce::Identifier& key, double value,
                                          const std::function<void(Seed&)>& applyFn) {
  const std::uint8_t focus = app_.focusSeed();
  app_.applySeedEditFromHost(focus, applyFn);
  setSeedProp(static_cast<int>(focus), key, value);
}

void SeedboxAudioProcessor::syncSeedStateFromApp() {
  const auto& seeds = app_.seeds();
  for (std::size_t i = 0; i < seeds.size(); ++i) {
    const Seed& seed = seeds[i];
    const int idx = static_cast<int>(i);
    setSeedProp(idx, kPropEngineId, static_cast<int>(seed.engine));
    setSeedProp(idx, kPropTone, seed.tone);
    setSeedProp(idx, kPropDensity, seed.density);
    setSeedProp(idx, kPropProbability, seed.probability);
    setSeedProp(idx, kPropSpread, seed.spread);
    setSeedProp(idx, kPropJitterMs, seed.jitterMs);
    setSeedProp(idx, kPropEnvRelease, seed.envR);
    setSeedProp(idx, kPropGrainSizeMs, seed.granular.grainSizeMs);
    setSeedProp(idx, kPropGrainSprayMs, seed.granular.sprayMs);
    setSeedProp(idx, kPropGranularTranspose, seed.granular.transpose);
    setSeedProp(idx, kPropGranularWindowSkew, seed.granular.windowSkew);
    setSeedProp(idx, kPropResonatorMode, static_cast<int>(seed.resonator.mode));
    setSeedProp(idx, kPropResonatorBank, static_cast<int>(seed.resonator.bank));
    setSeedProp(idx, kPropResonatorFeedback, seed.resonator.feedback);
    setSeedProp(idx, kPropResonatorDamping, seed.resonator.damping);
  }
}

void SeedboxAudioProcessor::applySeedStateToApp() {
  const auto& seeds = app_.seeds();
  for (std::size_t i = 0; i < seeds.size(); ++i) {
    const Seed& seed = seeds[i];
    const std::uint8_t index = static_cast<std::uint8_t>(i);
    const auto engineId = static_cast<std::uint8_t>(static_cast<int>(getSeedProp(static_cast<int>(i), kPropEngineId, seed.engine)));
    if (engineId != seed.engine) {
      app_.setSeedEngine(index, engineId);
    }

    auto applyFloat = [&](const juce::Identifier& key, float current, float min, float max,
                         const std::function<void(Seed&, float)>& setter) {
      const float target = static_cast<float>(getSeedProp(static_cast<int>(i), key, current));
      const float clamped = juce::jlimit(min, max, target);
      app_.applySeedEditFromHost(index, [&](Seed& s) { setter(s, clamped); });
    };

    auto applyInt = [&](const juce::Identifier& key, int current, int min, int max,
                        const std::function<void(Seed&, int)>& setter) {
      const int target = static_cast<int>(getSeedProp(static_cast<int>(i), key, current));
      const int clamped = juce::jlimit(min, max, target);
      app_.applySeedEditFromHost(index, [&](Seed& s) { setter(s, clamped); });
    };

    applyFloat(kPropTone, seed.tone, 0.0f, 1.0f, [](Seed& s, float v) { s.tone = v; });
    applyFloat(kPropDensity, seed.density, 0.0f, 8.0f, [](Seed& s, float v) { s.density = v; });
    applyFloat(kPropProbability, seed.probability, 0.0f, 1.0f, [](Seed& s, float v) { s.probability = v; });
    applyFloat(kPropSpread, seed.spread, 0.0f, 1.0f, [](Seed& s, float v) { s.spread = v; });
    applyFloat(kPropJitterMs, seed.jitterMs, 0.0f, 50.0f, [](Seed& s, float v) { s.jitterMs = v; });
    applyFloat(kPropEnvRelease, seed.envR, 0.0f, 1.5f, [](Seed& s, float v) { s.envR = v; });
    applyFloat(kPropGrainSizeMs, seed.granular.grainSizeMs, 10.0f, 400.0f,
               [](Seed& s, float v) { s.granular.grainSizeMs = v; });
    applyFloat(kPropGrainSprayMs, seed.granular.sprayMs, 0.0f, 120.0f,
               [](Seed& s, float v) { s.granular.sprayMs = v; });
    applyFloat(kPropGranularTranspose, seed.granular.transpose, -24.0f, 24.0f,
               [](Seed& s, float v) { s.granular.transpose = v; });
    applyFloat(kPropGranularWindowSkew, seed.granular.windowSkew, -1.0f, 1.0f,
               [](Seed& s, float v) { s.granular.windowSkew = v; });
    applyInt(kPropResonatorMode, seed.resonator.mode, 0, 3,
             [](Seed& s, int v) { s.resonator.mode = static_cast<std::uint8_t>(v); });
    applyInt(kPropResonatorBank, seed.resonator.bank, 0, 7,
             [](Seed& s, int v) { s.resonator.bank = static_cast<std::uint8_t>(v); });
    applyFloat(kPropResonatorFeedback, seed.resonator.feedback, 0.0f, 1.0f,
               [](Seed& s, float v) { s.resonator.feedback = v; });
    applyFloat(kPropResonatorDamping, seed.resonator.damping, 0.0f, 1.0f,
               [](Seed& s, float v) { s.resonator.damping = v; });
  }
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
