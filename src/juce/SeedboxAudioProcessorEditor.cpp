#include "juce/SeedboxAudioProcessorEditor.h"

#if SEEDBOX_JUCE

#include <juce_graphics/juce_graphics.h>
#include <cctype>
#include <algorithm>
#include <functional>
#include <map>
#include <utility>
#include <vector>

#include "BinaryData.h"
#include <sstream>

#include "app/AppState.h"
#include "juce/SeedboxAudioProcessor.h"

namespace seedbox::juce_bridge {

SeedboxAudioProcessorEditor::SeedboxAudioProcessorEditor(SeedboxAudioProcessor& processor)
    : juce::AudioProcessorEditor(&processor), processor_(processor) {
  setSize(760, 720);
  setWantsKeyboardFocus(true);

  modeSelector_.addItem("HOME", 1);
  modeSelector_.addItem("SEEDS", 2);
  modeSelector_.addItem("ENGINE", 3);
  modeSelector_.addItem("PERF", 4);
  modeSelector_.addItem("SETTINGS", 5);
  modeSelector_.addItem("UTIL", 6);
  modeSelector_.addItem("SWING", 7);
  modeSelector_.setJustificationType(juce::Justification::centred);
  modeSelector_.setTooltip("Mode jumper: same as mashing the panel combos without wearing out your fingers.");
  modeSelector_.onChange = [this]() {
    const auto id = modeSelector_.getSelectedId();
    if (id <= 0) {
      return;
    }
    const auto mode = static_cast<AppState::Mode>(id - 1);
    processor_.appState().setModeFromHost(mode);
    refreshModeControls();
  };
  modeSelector_.setSelectedId(1);
  addAndMakeVisible(modeSelector_);

  masterSeedSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  masterSeedSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
  const juce::String tooltip = juce::String::fromUTF8(BinaryData::juce_motd_txt, BinaryData::juce_motd_txtSize);
  masterSeedSlider_.setTooltip(tooltip);
  addAndMakeVisible(masterSeedSlider_);

  focusSeedSelector_.addItem("Seed 1", 1);
  focusSeedSelector_.addItem("Seed 2", 2);
  focusSeedSelector_.addItem("Seed 3", 3);
  focusSeedSelector_.addItem("Seed 4", 4);
  focusSeedSelector_.setJustificationType(juce::Justification::centred);
  focusSeedSelector_.setTooltip("Encoder A (SeedBank): Shift = coarse jumps, Alt = focus swap.");
  addAndMakeVisible(focusSeedSelector_);

  engineSelector_.addItem("Default", 1);
  engineSelector_.addItem("Grain", 2);
  engineSelector_.addItem("Chord", 3);
  engineSelector_.addItem("Drum", 4);
  engineSelector_.addItem("FM", 5);
  engineSelector_.addItem("Additive", 6);
  engineSelector_.addItem("Resonator", 7);
  engineSelector_.addItem("Noise", 8);
  engineSelector_.setJustificationType(juce::Justification::centred);
  engineSelector_.setTooltip("Encoder B (ToneTilt): Shift = alt engine, Alt = mod routing.");
  engineSelector_.onChange = [this]() { refreshEngineControls(); };
  addAndMakeVisible(engineSelector_);

  swingSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  swingSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
  swingSlider_.setTooltip("Encoder C (Density): Shift = micro swing; Alt = quantize toggles.");
  addAndMakeVisible(swingSlider_);

  quantizeScaleSelector_.addItem("Chromatic", 1);
  quantizeScaleSelector_.addItem("Major", 2);
  quantizeScaleSelector_.addItem("Minor", 3);
  quantizeScaleSelector_.addItem("Dorian", 4);
  quantizeScaleSelector_.addItem("Lydian", 5);
  quantizeScaleSelector_.setTooltip("Quantize scale (Alt + Density on hardware).");
  addAndMakeVisible(quantizeScaleSelector_);

  juce::StringArray roots{"C",  "C#", "D",  "D#", "E",  "F",
                          "F#", "G",  "G#", "A",  "A#", "B"};
  quantizeRootSelector_.addItemList(roots, 1);
  quantizeRootSelector_.setTooltip("Quantize root (Shift + Density).");
  addAndMakeVisible(quantizeRootSelector_);

  granularSourceSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  granularSourceSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
  granularSourceSlider_.setRange(-8.0, 8.0, 1.0);
  granularSourceSlider_.setNumDecimalPlacesToDisplay(0);
  granularSourceSlider_.setTooltip("Encoder D (FxMutate): Shift = source select, Alt = mutate grains.");
  addAndMakeVisible(granularSourceSlider_);

  engineControlHeader_.setText("Engine Controls", juce::dontSendNotification);
  engineControlHeader_.setJustificationType(juce::Justification::centredLeft);
  engineControlHeader_.setFont(juce::Font(16.0f, juce::Font::bold));
  addAndMakeVisible(engineControlHeader_);

  engineControlList_.setMultiLine(true);
  engineControlList_.setReadOnly(true);
  engineControlList_.setScrollbarsShown(true);
  engineControlList_.setCaretVisible(false);
  engineControlList_.setPopupMenuEnabled(false);
  engineControlList_.setColour(juce::TextEditor::backgroundColourId, juce::Colours::darkslategrey.darker(0.4f));
  engineControlList_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
  addAndMakeVisible(engineControlList_);

  audioSelectorHint_.setJustificationType(juce::Justification::centredLeft);
  audioSelectorHint_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
  addAndMakeVisible(audioSelectorHint_);

  transportLatchButton_.setButtonText("Transport Latch");
  transportLatchButton_.setTooltip("Tap Tempo; Shift holds transport; Alt punches pre-roll.");
  addAndMakeVisible(transportLatchButton_);

  externalClockButton_.setButtonText("Clock Source: External");
  externalClockButton_.setTooltip("Shift + Tap = chase external clock.");
  addAndMakeVisible(externalClockButton_);

  followClockButton_.setButtonText("Follow External Clock");
  followClockButton_.setTooltip("Alt + Tap = obey incoming transport.");
  addAndMakeVisible(followClockButton_);

  debugMetersButton_.setButtonText("Debug Meters");
  debugMetersButton_.setTooltip("Shift + FxMutate toggles the debug waterfall.");
  addAndMakeVisible(debugMetersButton_);

  for (auto& knob : engineKnobs_) {
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    knob.slider.setEnabled(false);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(knob.slider);
    addAndMakeVisible(knob.label);
  }

  displayLabel_.setJustificationType(juce::Justification::centred);
  displayLabel_.setColour(juce::Label::backgroundColourId, juce::Colours::black);
  displayLabel_.setColour(juce::Label::textColourId, juce::Colours::lime);
  displayLabel_.setFont(juce::Font(16.0f, juce::Font::plain));
  displayLabel_.setBorderSize(juce::BorderSize<int>(4));
  displayLabel_.setTooltip("Desktop control hint: press T for Tone, S for Shift, A for Alt.");
  addAndMakeVisible(displayLabel_);

  masterSeedAttachment_ = std::make_unique<juce::SliderParameterAttachment>(
      *processor_.parameters().getParameter("masterSeed"), masterSeedSlider_, nullptr);
  focusSeedAttachment_ = std::make_unique<juce::ComboBoxParameterAttachment>(
      *processor_.parameters().getParameter("focusSeed"), focusSeedSelector_);
  engineAttachment_ = std::make_unique<juce::ComboBoxParameterAttachment>(
      *processor_.parameters().getParameter("seedEngine"), engineSelector_);
  swingAttachment_ = std::make_unique<juce::SliderParameterAttachment>(
      *processor_.parameters().getParameter("swingPercent"), swingSlider_, nullptr);
  quantizeScaleAttachment_ = std::make_unique<juce::ComboBoxParameterAttachment>(
      *processor_.parameters().getParameter("quantizeScale"), quantizeScaleSelector_);
  quantizeRootAttachment_ = std::make_unique<juce::ComboBoxParameterAttachment>(
      *processor_.parameters().getParameter("quantizeRoot"), quantizeRootSelector_);
  granularAttachment_ = std::make_unique<juce::SliderParameterAttachment>(
      *processor_.parameters().getParameter("granularSourceStep"), granularSourceSlider_, nullptr);
  transportLatchAttachment_ = std::make_unique<juce::ButtonParameterAttachment>(
      *processor_.parameters().getParameter("transportLatch"), transportLatchButton_);
  externalClockAttachment_ = std::make_unique<juce::ButtonParameterAttachment>(
      *processor_.parameters().getParameter("clockSourceExternal"), externalClockButton_);
  followClockAttachment_ = std::make_unique<juce::ButtonParameterAttachment>(
      *processor_.parameters().getParameter("followExternalClock"), followClockButton_);
  debugMetersAttachment_ = std::make_unique<juce::ButtonParameterAttachment>(
      *processor_.parameters().getParameter("debugMeters"), debugMetersButton_);

  buildAudioSelector();
  refreshModeControls();

  refreshDisplay();
  startTimerHz(15);
}

SeedboxAudioProcessorEditor::~SeedboxAudioProcessorEditor() { stopTimer(); }

void SeedboxAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colours::darkslategrey); }

void SeedboxAudioProcessorEditor::resized() {
  auto area = getLocalBounds().reduced(16);
  auto header = area.removeFromTop(44);
  modeSelector_.setBounds(header.removeFromLeft(180));
  header.removeFromLeft(12);
  masterSeedSlider_.setBounds(header.removeFromLeft(200));
  header.removeFromLeft(12);
  quantizeScaleSelector_.setBounds(header.removeFromLeft(200));
  quantizeRootSelector_.setBounds(header);

  auto controlArea = area.removeFromTop(180);
  juce::Grid grid;
  grid.templateRows = {juce::Grid::TrackInfo(juce::Grid::Fr(1)), juce::Grid::TrackInfo(juce::Grid::Fr(1))};
  grid.templateColumns = {juce::Grid::TrackInfo(juce::Grid::Fr(1)), juce::Grid::TrackInfo(juce::Grid::Fr(1)),
                          juce::Grid::TrackInfo(juce::Grid::Fr(1))};
  grid.items.addArray({juce::GridItem(focusSeedSelector_), juce::GridItem(engineSelector_), juce::GridItem(swingSlider_),
                       juce::GridItem(granularSourceSlider_), juce::GridItem(transportLatchButton_),
                       juce::GridItem(externalClockButton_)});
  grid.performLayout(controlArea);

  auto engineArea = area.removeFromTop(260);
  engineControlHeader_.setBounds(engineArea.removeFromTop(24));
  auto knobArea = engineArea.removeFromTop(160);
  if (visibleEngineKnobCount_ > 0) {
    juce::Grid knobGrid;
    knobGrid.rowGap = juce::Grid::Px(6);
    knobGrid.columnGap = juce::Grid::Px(12);
    knobGrid.templateRows.add(juce::Grid::TrackInfo(juce::Grid::Fr(3)));
    knobGrid.templateRows.add(juce::Grid::TrackInfo(juce::Grid::Fr(1)));
    for (int i = 0; i < visibleEngineKnobCount_; ++i) {
      knobGrid.templateColumns.add(juce::Grid::TrackInfo(juce::Grid::Fr(1)));
    }

    juce::Array<juce::GridItem> knobItems;
    for (int i = 0; i < visibleEngineKnobCount_; ++i) {
      knobItems.add(juce::GridItem(engineKnobs_[i].slider));
    }
    for (int i = 0; i < visibleEngineKnobCount_; ++i) {
      knobItems.add(juce::GridItem(engineKnobs_[i].label));
    }
    knobGrid.items = knobItems;
    knobGrid.performLayout(knobArea);
  }
  engineControlList_.setBounds(engineArea);

  auto buttonArea = area.removeFromTop(48);
  followClockButton_.setBounds(buttonArea.removeFromLeft(220));
  buttonArea.removeFromLeft(8);
  debugMetersButton_.setBounds(buttonArea.removeFromLeft(220));

  auto audioArea = area.removeFromTop(120);
  if (audioSelector_) {
    audioSelector_->setBounds(audioArea);
  } else {
    audioSelectorHint_.setBounds(audioArea);
  }

  displayLabel_.setBounds(area.reduced(4));
}

void SeedboxAudioProcessorEditor::timerCallback() {
  int selectedModeId = modeSelector_.getSelectedId();
  const int appModeId = static_cast<int>(processor_.appState().mode()) + 1;
  if (selectedModeId != appModeId) {
    selectedModeId = appModeId;
    modeSelector_.setSelectedId(appModeId, juce::dontSendNotification);
  }

  const int engineId = engineSelector_.getSelectedId();
  const int focusId = focusSeedSelector_.getSelectedId();
  // ComboBoxParameterAttachment updates bypass onChange callbacks, so poll for
  // changes here to keep the helper text in sync with host automation and
  // preset recalls.
  if (engineId != lastEngineId_ || focusId != lastFocusSeed_ || selectedModeId != lastModeId_) {
    refreshModeControls();
  }
  refreshDisplay();
}

void SeedboxAudioProcessorEditor::refreshDisplay() {
  AppState::DisplaySnapshot snapshot{};
  processor_.appState().captureDisplaySnapshot(snapshot);
  std::ostringstream stream;
  stream << snapshot.title << "\n" << snapshot.status << "\n" << snapshot.metrics << "\n" << snapshot.nuance;
  displayLabel_.setText(stream.str(), juce::dontSendNotification);
}

void SeedboxAudioProcessorEditor::syncKeyboardButtons() {
#if !SEEDBOX_HW
  const bool toneDown = juce::KeyPress::isKeyCurrentlyDown(toneKeyCode_);
  const bool shiftDown = juce::KeyPress::isKeyCurrentlyDown(shiftKeyCode_);
  const bool altDown = juce::KeyPress::isKeyCurrentlyDown(altKeyCode_);
  updateButtonState(hal::Board::ButtonID::EncoderToneTilt, toneDown, toneKeyDown_);
  updateButtonState(hal::Board::ButtonID::Shift, shiftDown, shiftKeyDown_);
  updateButtonState(hal::Board::ButtonID::AltSeed, altDown, altKeyDown_);
#endif
}

bool SeedboxAudioProcessorEditor::handleButtonKey(int keyCode, bool pressed) {
#if !SEEDBOX_HW
  const int lower = std::tolower(keyCode);
  if (lower == toneKeyCode_) {
    updateButtonState(hal::Board::ButtonID::EncoderToneTilt, pressed, toneKeyDown_);
    return true;
  }
  if (lower == shiftKeyCode_) {
    updateButtonState(hal::Board::ButtonID::Shift, pressed, shiftKeyDown_);
    return true;
  }
  if (lower == altKeyCode_) {
    updateButtonState(hal::Board::ButtonID::AltSeed, pressed, altKeyDown_);
    return true;
  }
#endif
  juce::ignoreUnused(keyCode, pressed);
  return false;
}

void SeedboxAudioProcessorEditor::updateButtonState(hal::Board::ButtonID id, bool pressed, bool& lastState) {
#if !SEEDBOX_HW
  if (pressed == lastState) {
    return;
  }
  lastState = pressed;
  hal::nativeBoardSetButton(id, pressed);
#else
  juce::ignoreUnused(id, pressed, lastState);
#endif
}

bool SeedboxAudioProcessorEditor::keyPressed(const juce::KeyPress& key) {
#if !SEEDBOX_HW
  return handleButtonKey(key.getKeyCode(), true);
#else
  juce::ignoreUnused(key);
  return false;
#endif
}

bool SeedboxAudioProcessorEditor::keyStateChanged(bool isKeyDown) {
#if !SEEDBOX_HW
  juce::ignoreUnused(isKeyDown);
  syncKeyboardButtons();
  return false;
#else
  juce::ignoreUnused(isKeyDown);
  return false;
#endif
}

namespace {
struct EngineKnobSpec {
  juce::String label;
  std::function<double(const Seed&)> value;
  double minValue{0.0};
  double maxValue{1.0};
  double step{0.01};
  int decimals{2};
  juce::String suffix;
};

struct EngineControlTemplate {
  juce::String heading;
  juce::StringArray controls;
  std::vector<EngineKnobSpec> knobs;
};

struct ModeControlTemplate {
  juce::String heading;
  juce::StringArray controls;
};

const EngineKnobSpec kToneKnob{"Tone", [](const Seed& seed) { return seed.tone; }, 0.0, 1.0, 0.01, 2, {}};
const EngineKnobSpec kDensityKnob{"Density", [](const Seed& seed) { return seed.density; }, 0.0, 8.0, 0.01, 2, {" hits"}};
const EngineKnobSpec kProbabilityKnob{"Probability", [](const Seed& seed) { return seed.probability; }, 0.0, 1.0, 0.01, 2, {}};
const EngineKnobSpec kSpreadKnob{"Spread", [](const Seed& seed) { return seed.spread; }, 0.0, 1.0, 0.01, 2, {}};
const EngineKnobSpec kJitterKnob{"Jitter (ms)", [](const Seed& seed) { return seed.jitterMs; }, 0.0, 50.0, 0.1, 1, {" ms"}};
const EngineKnobSpec kReleaseKnob{"Env R (s)", [](const Seed& seed) { return seed.envR; }, 0.0, 1.5, 0.01, 2, {" s"}};
const EngineKnobSpec kGrainSizeKnob{"Grain (ms)", [](const Seed& seed) { return seed.granular.grainSizeMs; }, 10.0, 400.0, 1.0, 0, " ms"};
const EngineKnobSpec kSprayKnob{"Spray (ms)", [](const Seed& seed) { return seed.granular.sprayMs; }, 0.0, 120.0, 1.0, 0, " ms"};
const EngineKnobSpec kTransposeKnob{"Transpose", [](const Seed& seed) { return seed.granular.transpose; }, -24.0, 24.0, 0.1, 1, " st"};
const EngineKnobSpec kWindowSkewKnob{"Window skew", [](const Seed& seed) { return seed.granular.windowSkew; }, -1.0, 1.0, 0.01, 2, {}};
const EngineKnobSpec kResonatorModeKnob{"Mode", [](const Seed& seed) { return static_cast<double>(seed.resonator.mode); }, 0.0, 3.0, 1.0, 0, {}};
const EngineKnobSpec kResonatorBankKnob{"Bank", [](const Seed& seed) { return static_cast<double>(seed.resonator.bank); }, 0.0, 7.0, 1.0, 0, {}};
const EngineKnobSpec kResonatorFeedbackKnob{"Feedback", [](const Seed& seed) { return seed.resonator.feedback; }, 0.0, 1.0, 0.01, 2, {}};
const EngineKnobSpec kResonatorDampingKnob{"Damping", [](const Seed& seed) { return seed.resonator.damping; }, 0.0, 1.0, 0.01, 2, {}};

const std::map<int, EngineControlTemplate> kEngineControls = {
    {1,
     {"Sampler lane",
      {"Start + length macros live on Density", "Tone tilt is your quick EQ", "Spread widens the stereo field", "Env ADSR rides per-seed"},
      {kDensityKnob, kToneKnob, kSpreadKnob, kReleaseKnob}}},
    {2,
     {"Grain lane",
      {"Grain size + spray set texture", "Transpose macro follows seed pitch", "Window skew sculpts envelopes", "Source select flips between live + SD"},
      {kGrainSizeKnob, kSprayKnob, kTransposeKnob, kWindowSkewKnob}}},
    {3,
     {"Chord lane",
      {"Voice stack = Density", "Macro tilt brightens/darkens", "Spread and jitter keep voicings human"},
      {kDensityKnob, kToneKnob, kSpreadKnob, kJitterKnob}}},
    {4,
     {"Drum lane",
      {"Probability acts like a gate", "Tone -> transient tilt", "Spread fans the kit in stereo"},
      {kProbabilityKnob, kToneKnob, kSpreadKnob, kDensityKnob}}},
    {5,
     {"FM playground",
      {"Carrier/Mod ratio rides on Tone", "Mod index sits on Density", "Feedback macro is in Spread", "Jitter sprinkles subtle detune"},
      {kToneKnob, kDensityKnob, kSpreadKnob, kJitterKnob}}},
    {6,
     {"Additive palette",
      {"Harmonic mix -> Tone", "Density drives partial triggers", "Spread pans the bank"},
      {kToneKnob, kDensityKnob, kSpreadKnob}}},
    {7,
     {"Resonator",
      {"Mode/bank live on Tone", "Feedback macro lives on Spread", "Excite time and damping follow Density/Jitter"},
      {kResonatorModeKnob, kResonatorBankKnob, kResonatorFeedbackKnob, kResonatorDampingKnob}}},
    {8,
     {"Noise lab",
      {"Filter tilt on Tone", "Density = burst rate", "Spread = width"},
      {kToneKnob, kDensityKnob, kSpreadKnob, kJitterKnob}}},
};

const std::map<AppState::Mode, ModeControlTemplate> kModeControls = {
    {AppState::Mode::HOME,
     {"Home base",
      {"Shift+Seed slides focus", "Tap for tempo, latch to loop", "Hop to SEEDS or ENGINE to sculpt"}}},
    {AppState::Mode::SEEDS,
     {"Seeds page",
      {"Seed encoder swaps focus", "Hold Tone+Shift: swap granular src", "Hold Tone+Alt: density micro nudges"}}},
    {AppState::Mode::PERF,
     {"Performance",
      {"Tap toggles transport latch", "Use swing + quantize to tighten", "Locks keep accident-proof sets"}}},
    {AppState::Mode::SETTINGS,
     {"Settings",
      {"Tap flips external clock follow", "Clock + transport live here", "Alt/Shift mirrors hardware combos"}}},
    {AppState::Mode::UTIL,
     {"Utility bench",
      {"FX knob with motion = debug meters", "Great for lab checks", "Leave OFF when tracking"}}},
    {AppState::Mode::SWING,
     {"Swing edit",
      {"Seed encoder: 5% chunks", "Density encoder: fine shuffles", "Tap exits back to your last mode"}}},
};
}  // namespace

void SeedboxAudioProcessorEditor::refreshModeControls() {
  const int id = modeSelector_.getSelectedId();
  const auto mode = id > 0 ? static_cast<AppState::Mode>(id - 1) : AppState::Mode::HOME;

  if (mode == AppState::Mode::ENGINE) {
    refreshEngineControls();
    return;
  }

  const auto it = kModeControls.find(mode);
  const auto& tpl = (it != kModeControls.end())
                        ? it->second
                        : ModeControlTemplate{"Pick a mode",
                                              {"Choose a page to see its live controls."}};

  juce::String text;
  for (const auto& line : tpl.controls) {
    text << juce::String("• ") << line << "\n";
  }

  engineControlHeader_.setText(tpl.heading, juce::dontSendNotification);
  engineControlList_.setText(text, juce::dontSendNotification);

  visibleEngineKnobCount_ = 0;
  for (auto& knob : engineKnobs_) {
    knob.slider.setVisible(false);
    knob.label.setVisible(false);
  }

  lastModeId_ = id;
  lastEngineId_ = engineSelector_.getSelectedId();
  lastFocusSeed_ = focusSeedSelector_.getSelectedId();
  resized();
}

void SeedboxAudioProcessorEditor::refreshEngineControls() {
  const int engineId = engineSelector_.getSelectedId();
  const auto it = kEngineControls.find(engineId);
  const auto& tpl = (it != kEngineControls.end())
                        ? it->second
                        : EngineControlTemplate{"Pick an engine", {"Select an engine to see its live controls."}};

  juce::String text;
  for (const auto& line : tpl.controls) {
    text << juce::String("• ") << line << "\n";
  }

  engineControlHeader_.setText(tpl.heading, juce::dontSendNotification);
  engineControlList_.setText(text, juce::dontSendNotification);
  const Seed* focusSeed = [&]() -> const Seed* {
    const auto& seeds = processor_.appState().seeds();
    if (seeds.empty()) {
      return nullptr;
    }
    const std::size_t focus = std::min<std::size_t>(processor_.appState().focusSeed(), seeds.size() - 1);
    return &seeds[focus];
  }();

  const std::size_t knobCount = std::min<std::size_t>(tpl.knobs.size(), engineKnobs_.size());
  visibleEngineKnobCount_ = static_cast<int>(focusSeed ? knobCount : 0);

  for (std::size_t i = 0; i < engineKnobs_.size(); ++i) {
    const bool show = focusSeed != nullptr && i < knobCount;
    auto& knob = engineKnobs_[i];
    knob.slider.setVisible(show);
    knob.label.setVisible(show);
    if (!show) {
      continue;
    }

    const auto& spec = tpl.knobs[i];
    const double value = spec.value(*focusSeed);
    const double minRange = std::min(spec.minValue, value);
    const double maxRange = std::max(spec.maxValue, value);
    knob.slider.setRange(minRange, maxRange, spec.step);
    knob.slider.setNumDecimalPlacesToDisplay(spec.decimals);
    knob.slider.setTextValueSuffix(spec.suffix);
    knob.slider.setValue(value, juce::dontSendNotification);
    knob.label.setText(spec.label, juce::dontSendNotification);
  }

  lastModeId_ = modeSelector_.getSelectedId();
  lastEngineId_ = engineId;
  lastFocusSeed_ = focusSeedSelector_.getSelectedId();
  resized();
}

void SeedboxAudioProcessorEditor::buildAudioSelector() {
  if (processor_.deviceManager() == nullptr) {
    audioSelectorHint_.setText("Host plugin builds let the DAW pick I/O. Standalone gets a full selector.",
                               juce::dontSendNotification);
    return;
  }

  const int numInputs = processor_.getTotalNumInputChannels();
  const int numOutputs = processor_.getTotalNumOutputChannels();
  audioSelector_ = std::make_unique<juce::AudioDeviceSelectorComponent>(
      *processor_.deviceManager(), numInputs > 0 ? 1 : 0, std::max(1, numInputs), numOutputs > 0 ? 1 : 0,
      std::max(1, numOutputs), false, false, true, false);
  addAndMakeVisible(audioSelector_.get());
  audioSelectorHint_.setVisible(false);
}

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
