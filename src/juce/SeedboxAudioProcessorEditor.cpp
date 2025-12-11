#include "juce/SeedboxAudioProcessorEditor.h"

#if SEEDBOX_JUCE

#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <map>
#include <utility>

#include "BinaryData.h"
#include <sstream>

#include "app/AppState.h"
#include "juce/SeedboxAudioProcessor.h"

namespace seedbox::juce_bridge {

SeedboxAudioProcessorEditor::SeedboxAudioProcessorEditor(SeedboxAudioProcessor& processor)
    : juce::AudioProcessorEditor(&processor), processor_(processor) {
  setSize(880, 620);

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

  displayLabel_.setJustificationType(juce::Justification::centred);
  displayLabel_.setColour(juce::Label::backgroundColourId, juce::Colours::black);
  displayLabel_.setColour(juce::Label::textColourId, juce::Colours::lime);
  displayLabel_.setFont(juce::Font(16.0f, juce::Font::plain));
  displayLabel_.setBorderSize(juce::BorderSize<int>(4));
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
  refreshEngineControls();
  
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

  auto engineArea = area.removeFromTop(110);
  engineControlHeader_.setBounds(engineArea.removeFromTop(24));
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

void SeedboxAudioProcessorEditor::timerCallback() { refreshDisplay(); }

void SeedboxAudioProcessorEditor::refreshDisplay() {
  AppState::DisplaySnapshot snapshot{};
  processor_.appState().captureDisplaySnapshot(snapshot);
  std::ostringstream stream;
  stream << snapshot.title << "\n" << snapshot.status << "\n" << snapshot.metrics << "\n" << snapshot.nuance;
  displayLabel_.setText(stream.str(), juce::dontSendNotification);
}

namespace {
struct EngineControlTemplate {
  juce::String heading;
  juce::StringArray controls;
};

const std::map<int, EngineControlTemplate> kEngineControls = {
    {1, {"Sampler lane", {"Start + length macros live on Density", "Tone tilt is your quick EQ", "Spread widens the stereo field", "Env ADSR rides per-seed"}}},
    {2, {"Grain lane", {"Grain size + spray set texture", "Transpose macro follows seed pitch", "Window skew sculpts envelopes", "Source select flips between live + SD"}}},
    {3, {"Chord lane", {"Voice stack = Density", "Macro tilt brightens/darkens", "Spread and jitter keep voicings human"}}},
    {4, {"Drum lane", {"Probability acts like a gate", "Tone -> transient tilt", "Spread fans the kit in stereo"}}},
    {5, {"FM playground", {"Carrier/Mod ratio rides on Tone", "Mod index sits on Density", "Feedback macro is in Spread", "Jitter sprinkles subtle detune"}}},
    {6, {"Additive palette", {"Harmonic mix -> Tone", "Density drives partial triggers", "Spread pans the bank"}}},
    {7, {"Resonator", {"Mode/bank live on Tone", "Feedback macro lives on Spread", "Excite time and damping follow Density/Jitter"}}},
    {8, {"Noise lab", {"Filter tilt on Tone", "Density = burst rate", "Spread = width"}}},
};
}  // namespace

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
