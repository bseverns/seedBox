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
namespace {
struct EngineKnobSpec {
  juce::String label;
  std::function<double(const Seed&)> value;
  std::function<void(Seed&, double)> apply;
  juce::Identifier property;
  double minValue{0.0};
  double maxValue{1.0};
  double step{0.01};
  int decimals{2};
  juce::String suffix;
};

const EngineKnobSpec kToneKnob{"Tone",
                                [](const Seed& seed) { return seed.tone; },
                                [](Seed& seed, double v) { seed.tone = juce::jlimit(0.0f, 1.0f, static_cast<float>(v)); },
                                juce::Identifier{"tone"}, 0.0, 1.0, 0.01, 2, {}};
const EngineKnobSpec kDensityKnob{"Density",
                                  [](const Seed& seed) { return seed.density; },
                                  [](Seed& seed, double v) { seed.density = juce::jlimit(0.0f, 8.0f, static_cast<float>(v)); },
                                  juce::Identifier{"density"}, 0.0, 8.0, 0.01, 2, {" hits"}};
const EngineKnobSpec kProbabilityKnob{"Probability",
                                      [](const Seed& seed) { return seed.probability; },
                                      [](Seed& seed, double v) { seed.probability = juce::jlimit(0.0f, 1.0f, static_cast<float>(v)); },
                                      juce::Identifier{"probability"}, 0.0, 1.0, 0.01, 2, {}};
const EngineKnobSpec kSpreadKnob{"Spread",
                                 [](const Seed& seed) { return seed.spread; },
                                 [](Seed& seed, double v) { seed.spread = juce::jlimit(0.0f, 1.0f, static_cast<float>(v)); },
                                 juce::Identifier{"spread"}, 0.0, 1.0, 0.01, 2, {}};
const EngineKnobSpec kJitterKnob{"Jitter (ms)",
                                 [](const Seed& seed) { return seed.jitterMs; },
                                 [](Seed& seed, double v) { seed.jitterMs = juce::jlimit(0.0f, 50.0f, static_cast<float>(v)); },
                                 juce::Identifier{"jitterMs"}, 0.0, 50.0, 0.1, 1, {" ms"}};
const EngineKnobSpec kReleaseKnob{"Env R (s)",
                                  [](const Seed& seed) { return seed.envR; },
                                  [](Seed& seed, double v) { seed.envR = juce::jlimit(0.0f, 1.5f, static_cast<float>(v)); },
                                  juce::Identifier{"envRelease"}, 0.0, 1.5, 0.01, 2, {" s"}};
const EngineKnobSpec kGrainSizeKnob{"Grain (ms)",
                                    [](const Seed& seed) { return seed.granular.grainSizeMs; },
                                    [](Seed& seed, double v) {
                                      seed.granular.grainSizeMs = juce::jlimit(10.0f, 400.0f, static_cast<float>(v));
                                    },
                                    juce::Identifier{"grainSizeMs"}, 10.0, 400.0, 1.0, 0, {" ms"}};
const EngineKnobSpec kGrainSprayKnob{"Spray (ms)",
                                     [](const Seed& seed) { return seed.granular.sprayMs; },
                                     [](Seed& seed, double v) {
                                       seed.granular.sprayMs = juce::jlimit(0.0f, 120.0f, static_cast<float>(v));
                                     },
                                     juce::Identifier{"grainSprayMs"}, 0.0, 120.0, 1.0, 0, {" ms"}};
const EngineKnobSpec kGrainTransposeKnob{
    "Transpose",
    [](const Seed& seed) { return seed.granular.transpose; },
    [](Seed& seed, double v) { seed.granular.transpose = juce::jlimit(-24.0f, 24.0f, static_cast<float>(v)); },
    juce::Identifier{"granularTranspose"}, -24.0, 24.0, 1.0, 0, {" st"}};
const EngineKnobSpec kGrainWindowSkewKnob{
    "Window Skew",
    [](const Seed& seed) { return seed.granular.windowSkew; },
    [](Seed& seed, double v) { seed.granular.windowSkew = juce::jlimit(-1.0f, 1.0f, static_cast<float>(v)); },
    juce::Identifier{"granularWindowSkew"}, -1.0, 1.0, 0.01, 2, {}};
const EngineKnobSpec kResonatorFeedbackKnob{
    "Feedback",
    [](const Seed& seed) { return seed.resonator.feedback; },
    [](Seed& seed, double v) { seed.resonator.feedback = juce::jlimit(0.0f, 1.0f, static_cast<float>(v)); },
    juce::Identifier{"resonatorFeedback"}, 0.0, 1.0, 0.01, 2, {}};
const EngineKnobSpec kResonatorDampingKnob{
    "Damping",
    [](const Seed& seed) { return seed.resonator.damping; },
    [](Seed& seed, double v) { seed.resonator.damping = juce::jlimit(0.0f, 1.0f, static_cast<float>(v)); },
    juce::Identifier{"resonatorDamping"}, 0.0, 1.0, 0.01, 2, {}};

struct EngineControlTemplate {
  juce::String heading;
  std::vector<EngineKnobSpec> knobs;
};

const EngineControlTemplate kDefaultEngine{
    "Default Engine", {kToneKnob, kDensityKnob, kProbabilityKnob, kSpreadKnob}};
const EngineControlTemplate kGrainEngine{"Granular",
                                         {kToneKnob, kDensityKnob, kProbabilityKnob, kSpreadKnob, kJitterKnob,
                                          kReleaseKnob, kGrainSizeKnob, kGrainSprayKnob, kGrainTransposeKnob,
                                          kGrainWindowSkewKnob}};
const EngineControlTemplate kChordEngine{"Chord", {kToneKnob, kDensityKnob, kProbabilityKnob, kSpreadKnob}};
const EngineControlTemplate kDrumEngine{"Drum", {kToneKnob, kDensityKnob, kProbabilityKnob}};
const EngineControlTemplate kFmEngine{"FM", {kToneKnob, kDensityKnob, kProbabilityKnob, kSpreadKnob, kJitterKnob}};
const EngineControlTemplate kAdditiveEngine{"Additive",
                                           {kToneKnob, kDensityKnob, kProbabilityKnob, kSpreadKnob, kJitterKnob,
                                            kReleaseKnob}};
const EngineControlTemplate kResonatorEngine{
    "Resonator",
    {kToneKnob, kDensityKnob, kProbabilityKnob, kSpreadKnob, kResonatorFeedbackKnob, kResonatorDampingKnob}};
const EngineControlTemplate kNoiseEngine{"Noise", {kToneKnob, kDensityKnob, kProbabilityKnob, kSpreadKnob}};

const EngineControlTemplate& engineTemplateFor(int engineId) {
  switch (engineId) {
    case 1:
      return kGrainEngine;
    case 2:
      return kChordEngine;
    case 3:
      return kDrumEngine;
    case 4:
      return kFmEngine;
    case 5:
      return kAdditiveEngine;
    case 6:
      return kResonatorEngine;
    case 7:
      return kNoiseEngine;
    default:
      return kDefaultEngine;
  }
}

void nudgeSlider(juce::Slider& slider, double delta) {
  double step = slider.getInterval();
  if (step <= 0.0) {
    step = 0.1;
  }
  slider.setValue(slider.getValue() + (step * delta), juce::sendNotificationSync);
}
}  // namespace

HomePageComponent::HomePageComponent(SeedboxAudioProcessor& processor) : PageComponent(processor) {
  masterSeedSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  masterSeedSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
  const juce::String tooltip = juce::String::fromUTF8(BinaryData::juce_motd_txt, BinaryData::juce_motd_txtSize);
  masterSeedSlider_.setTooltip(tooltip);
  addAndMakeVisible(masterSeedSlider_);
  masterSeedAttachment_ = std::make_unique<juce::SliderParameterAttachment>(
      *processor_.parameters().getParameter("masterSeed"), masterSeedSlider_, nullptr);

  bpmLabel_.setJustificationType(juce::Justification::centredLeft);
  bpmLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
  clockLabel_.setJustificationType(juce::Justification::centredLeft);
  clockLabel_.setColour(juce::Label::textColourId, juce::Colours::orange);
  focusLabel_.setJustificationType(juce::Justification::centredLeft);

  addAndMakeVisible(bpmLabel_);
  addAndMakeVisible(clockLabel_);
  addAndMakeVisible(focusLabel_);
}

void HomePageComponent::refresh() {
  const auto& ui = processor_.appState().uiStateCache();
  bpmLabel_.setText("BPM: " + juce::String(ui.bpm, 1), juce::dontSendNotification);
  const char* clockName = ui.clock == UiState::ClockSource::kExternal ? "Clock: External" : "Clock: Internal";
  clockLabel_.setText(clockName, juce::dontSendNotification);
  const auto focus = processor_.appState().focusSeed() + 1;
  focusLabel_.setText("Focus: Seed " + juce::String(focus), juce::dontSendNotification);
}

void HomePageComponent::resized() {
  auto area = getLocalBounds();
  masterSeedSlider_.setBounds(area.removeFromTop(180));
  auto labels = area.removeFromTop(60);
  bpmLabel_.setBounds(labels.removeFromLeft(160));
  clockLabel_.setBounds(labels.removeFromLeft(160));
  focusLabel_.setBounds(labels);
}

SeedsPageComponent::SeedsPageComponent(SeedboxAudioProcessor& processor) : PageComponent(processor) {
  focusSeedSelector_.addItem("Seed 1", 1);
  focusSeedSelector_.addItem("Seed 2", 2);
  focusSeedSelector_.addItem("Seed 3", 3);
  focusSeedSelector_.addItem("Seed 4", 4);
  focusSeedSelector_.setJustificationType(juce::Justification::centred);
  focusSeedSelector_.setTooltip("Choose which seed to poke (1-4 also works).");
  addAndMakeVisible(focusSeedSelector_);
  focusSeedAttachment_ = std::make_unique<juce::ComboBoxParameterAttachment>(
      *processor_.parameters().getParameter("focusSeed"), focusSeedSelector_);

  gateDivisionSelector_.addItem("1/1", 1);
  gateDivisionSelector_.addItem("1/2", 2);
  gateDivisionSelector_.addItem("1/4", 3);
  gateDivisionSelector_.addItem("Bar", 4);
  gateDivisionSelector_.setJustificationType(juce::Justification::centred);
  gateDivisionSelector_.setTooltip("Quantize the live-input reseed gate to beats/partials/bars.");
  addAndMakeVisible(gateDivisionSelector_);
  gateDivisionAttachment_ = std::make_unique<juce::ComboBoxParameterAttachment>(
      *processor_.parameters().getParameter("gateDivision"), gateDivisionSelector_);

  seedToneSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  seedToneSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
  seedToneSlider_.setRange(0.0, 1.0, 0.01);
  seedToneSlider_.setTooltip("Per-seed tone skew; lives in the state tree so it saves/restores.");
  seedToneSlider_.onValueChange = [this]() {
    processor_.applySeedEdit(juce::Identifier{"tone"}, seedToneSlider_.getValue(),
                             [v = seedToneSlider_.getValue()](Seed& seed) {
                               seed.tone = juce::jlimit(0.0f, 1.0f, static_cast<float>(v));
                             });
  };
  addAndMakeVisible(seedToneSlider_);

  seedProbabilitySlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  seedProbabilitySlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
  seedProbabilitySlider_.setRange(0.0, 1.0, 0.01);
  seedProbabilitySlider_.setTooltip("Seed probability (focused seed only). Saves via ValueTree.");
  seedProbabilitySlider_.onValueChange = [this]() {
    processor_.applySeedEdit(juce::Identifier{"probability"}, seedProbabilitySlider_.getValue(),
                             [v = seedProbabilitySlider_.getValue()](Seed& seed) {
                               seed.probability = juce::jlimit(0.0f, 1.0f, static_cast<float>(v));
                             });
  };
  addAndMakeVisible(seedProbabilitySlider_);

  gateFloorSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
  gateFloorSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 18);
  gateFloorSlider_.setRange(1e-6, 0.25, 0.000001);
  gateFloorSlider_.setSkewFactor(0.35, false);
  gateFloorSlider_.setTooltip("Energy floor for live-input reseeds. Lower = more sensitive.");
  addAndMakeVisible(gateFloorSlider_);
  gateFloorAttachment_ = std::make_unique<juce::SliderParameterAttachment>(
      *processor_.parameters().getParameter("gateFloor"), gateFloorSlider_);

  randomizeSeedButton_.setButtonText("Randomize Focused Seed");
  randomizeSeedButton_.onClick = [this]() {
    processor_.appState().seedPageReseed(processor_.appState().masterSeed(), AppState::SeedPrimeMode::kLfsr);
  };
  addAndMakeVisible(randomizeSeedButton_);
}

void SeedsPageComponent::refresh() {
  const auto focus = processor_.appState().focusSeed();
  focusSeedSelector_.setSelectedId(static_cast<int>(focus) + 1, juce::dontSendNotification);
  const auto& seeds = processor_.appState().seeds();
  if (seeds.empty()) {
    seedToneSlider_.setEnabled(false);
    seedProbabilitySlider_.setEnabled(false);
    gateDivisionSelector_.setEnabled(false);
    gateFloorSlider_.setEnabled(false);
    return;
  }
  const auto idx = std::min<std::size_t>(seeds.size() - 1, static_cast<std::size_t>(focus));
  const Seed& seed = seeds[idx];
  seedToneSlider_.setEnabled(true);
  seedProbabilitySlider_.setEnabled(true);
  gateDivisionSelector_.setEnabled(true);
  gateFloorSlider_.setEnabled(true);
  seedToneSlider_.setValue(seed.tone, juce::dontSendNotification);
  seedProbabilitySlider_.setValue(seed.probability, juce::dontSendNotification);
}

void SeedsPageComponent::resized() {
  auto area = getLocalBounds().reduced(12);
  focusSeedSelector_.setBounds(area.removeFromTop(48));
  auto gateRow = area.removeFromTop(36);
  gateDivisionSelector_.setBounds(gateRow.removeFromLeft(gateRow.getWidth() / 3));
  gateFloorSlider_.setBounds(gateRow);
  auto knobRow = area.removeFromTop(180);
  seedToneSlider_.setBounds(knobRow.removeFromLeft(knobRow.getWidth() / 2));
  seedProbabilitySlider_.setBounds(knobRow);
  randomizeSeedButton_.setBounds(area.removeFromTop(44));
}

EnginePageComponent::EnginePageComponent(SeedboxAudioProcessor& processor) : PageComponent(processor) {
  engineSelector_.addItem("Default", 1);
  engineSelector_.addItem("Grain", 2);
  engineSelector_.addItem("Chord", 3);
  engineSelector_.addItem("Drum", 4);
  engineSelector_.addItem("FM", 5);
  engineSelector_.addItem("Additive", 6);
  engineSelector_.addItem("Resonator", 7);
  engineSelector_.addItem("Noise", 8);
  engineSelector_.setJustificationType(juce::Justification::centred);
  engineSelector_.setTooltip("Engine select for focused seed (E cycles too).");
  engineSelector_.onChange = [this]() { refresh(); };
  addAndMakeVisible(engineSelector_);
  engineAttachment_ = std::make_unique<juce::ComboBoxParameterAttachment>(
      *processor_.parameters().getParameter("seedEngine"), engineSelector_);

  heading_.setJustificationType(juce::Justification::centredLeft);
  heading_.setFont(juce::Font(16.0f, juce::Font::bold));
  addAndMakeVisible(heading_);

  for (auto& knob : engineKnobs_) {
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    knob.slider.setEnabled(false);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(knob.slider);
    addAndMakeVisible(knob.label);
  }
}

void EnginePageComponent::refresh() {
  const int engineId = engineSelector_.getSelectedId();
  const EngineControlTemplate& tpl = engineTemplateFor(engineId - 1);
  heading_.setText(tpl.heading, juce::dontSendNotification);
  const auto& seeds = processor_.appState().seeds();
  const std::size_t focus = std::min<std::size_t>(processor_.appState().focusSeed(),
                                                 seeds.empty() ? 0 : seeds.size() - 1);
  const Seed* focusSeed = seeds.empty() ? nullptr : &seeds[focus];

  const std::size_t knobCount = std::min<std::size_t>(tpl.knobs.size(), engineKnobs_.size());
  visibleEngineKnobCount_ = static_cast<int>(focusSeed ? knobCount : 0);

  updatingEngineKnobs_ = true;
  for (std::size_t i = 0; i < engineKnobs_.size(); ++i) {
    const bool show = focusSeed != nullptr && i < knobCount;
    auto& knob = engineKnobs_[i];
    knob.slider.setVisible(show);
    knob.label.setVisible(show);
    knob.slider.setEnabled(show);
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
    knob.slider.onValueChange = [this, spec, slider = &knob.slider]() {
      if (updatingEngineKnobs_) {
        return;
      }
      const double next = slider->getValue();
      processor_.applySeedEdit(spec.property, next, [spec, next](Seed& seed) { spec.apply(seed, next); });
    };
    knob.label.setText(spec.label, juce::dontSendNotification);
  }
  updatingEngineKnobs_ = false;
  lastEngineId_ = engineId;
  resized();
}

void EnginePageComponent::nudgeFirstKnob(double delta) {
  if (visibleEngineKnobCount_ == 0) {
    return;
  }
  nudgeSlider(engineKnobs_[0].slider, delta);
}

int EnginePageComponent::currentEngineId() const { return engineSelector_.getSelectedId(); }

void EnginePageComponent::setEngineId(int id) { engineSelector_.setSelectedId(id, juce::sendNotificationSync); }

int EnginePageComponent::engineCount() const { return engineSelector_.getNumItems(); }

void EnginePageComponent::resized() {
  auto area = getLocalBounds().reduced(12);
  engineSelector_.setBounds(area.removeFromTop(48));
  heading_.setBounds(area.removeFromTop(24));
  auto knobArea = area.removeFromTop(200);
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
}

PerfPageComponent::PerfPageComponent(SeedboxAudioProcessor& processor) : PageComponent(processor) {
  tempoSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  tempoSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
  tempoSlider_.setRange(40.0, 240.0, 0.1);
  tempoSlider_.setNumDecimalPlacesToDisplay(1);
  tempoSlider_.setTextValueSuffix(" BPM");
  tempoSlider_.setTooltip("Internal tempo: live editable on PERF/HOME.");
  tempoSlider_.onValueChange = [this]() {
    processor_.appState().setInternalBpmFromHost(static_cast<float>(tempoSlider_.getValue()));
  };
  addAndMakeVisible(tempoSlider_);

  tapTempoButton_.setButtonText("Tap Tempo");
  tapTempoButton_.setTooltip("Tap your BPM like the hardware stomp.");
  tapTempoButton_.onClick = [this]() {
    if (tapHandler_) {
      tapHandler_();
    }
  };
  addAndMakeVisible(tapTempoButton_);

  transportLatchButton_.setButtonText("Transport Latch");
  transportLatchButton_.setTooltip("Space toggles; Tap also latches.");
  addAndMakeVisible(transportLatchButton_);

  bpmLabel_.setJustificationType(juce::Justification::centredLeft);
  bpmLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
  clockLabel_.setJustificationType(juce::Justification::centredLeft);
  clockLabel_.setColour(juce::Label::textColourId, juce::Colours::orange);
  addAndMakeVisible(bpmLabel_);
  addAndMakeVisible(clockLabel_);

  transportLatchAttachment_ = std::make_unique<juce::ButtonParameterAttachment>(
      *processor_.parameters().getParameter("transportLatch"), transportLatchButton_);
}

void PerfPageComponent::refresh() {
  const auto& ui = processor_.appState().uiStateCache();
  bpmLabel_.setText("BPM: " + juce::String(ui.bpm, 1), juce::dontSendNotification);
  const char* clockName = ui.clock == UiState::ClockSource::kExternal ? "Clock: External" : "Clock: Internal";
  clockLabel_.setText(clockName, juce::dontSendNotification);
  tempoSlider_.setValue(ui.bpm, juce::dontSendNotification);
}

void PerfPageComponent::resized() {
  auto area = getLocalBounds().reduced(12);
  auto topRow = area.removeFromTop(120);
  tempoSlider_.setBounds(topRow.removeFromLeft(160));
  tapTempoButton_.setBounds(topRow.removeFromLeft(140));
  transportLatchButton_.setBounds(topRow.removeFromLeft(160));
  auto info = area.removeFromTop(40);
  bpmLabel_.setBounds(info.removeFromLeft(140));
  clockLabel_.setBounds(info);
}

SwingPageComponent::SwingPageComponent(SeedboxAudioProcessor& processor) : PageComponent(processor) {
  swingSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  swingSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
  swingSlider_.setTooltip("Swing depth (0-99%).");
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

  swingAttachment_ = std::make_unique<juce::SliderParameterAttachment>(
      *processor_.parameters().getParameter("swingPercent"), swingSlider_, nullptr);
  quantizeScaleAttachment_ = std::make_unique<juce::ComboBoxParameterAttachment>(
      *processor_.parameters().getParameter("quantizeScale"), quantizeScaleSelector_);
  quantizeRootAttachment_ = std::make_unique<juce::ComboBoxParameterAttachment>(
      *processor_.parameters().getParameter("quantizeRoot"), quantizeRootSelector_);
}

void SwingPageComponent::refresh() {
  const auto& ui = processor_.appState().uiStateCache();
  swingSlider_.setValue(ui.swing, juce::dontSendNotification);
  quantizeScaleSelector_.setSelectedItemIndex(static_cast<int>(processor_.appState().quantizeScaleIndex()),
                                              juce::dontSendNotification);
  quantizeRootSelector_.setSelectedItemIndex(static_cast<int>(processor_.appState().quantizeRoot()),
                                             juce::dontSendNotification);
}

void SwingPageComponent::resized() {
  auto area = getLocalBounds().reduced(12);
  swingSlider_.setBounds(area.removeFromTop(160));
  auto quantRow = area.removeFromTop(44);
  quantizeScaleSelector_.setBounds(quantRow.removeFromLeft(180));
  quantizeRootSelector_.setBounds(quantRow.removeFromLeft(120));
}

UtilPageComponent::UtilPageComponent(SeedboxAudioProcessor& processor) : PageComponent(processor) {
  debugMetersButton_.setButtonText("Debug Meters");
  debugMetersButton_.setTooltip("Shift + FxMutate toggles the debug waterfall.");
  addAndMakeVisible(debugMetersButton_);

  panicButton_.setButtonText("Panic / Reset");
  panicButton_.setTooltip("All notes off, unlatch transport.");
  panicButton_.onClick = [this]() {
    processor_.appState().midi.panic();
    processor_.appState().setTransportLatchFromHost(false);
  };
  addAndMakeVisible(panicButton_);

  debugMetersAttachment_ = std::make_unique<juce::ButtonParameterAttachment>(
      *processor_.parameters().getParameter("debugMeters"), debugMetersButton_);
}

void UtilPageComponent::refresh() {}

void UtilPageComponent::resized() {
  auto area = getLocalBounds().reduced(12);
  debugMetersButton_.setBounds(area.removeFromTop(36));
  panicButton_.setBounds(area.removeFromTop(36));
}

SettingsPageComponent::SettingsPageComponent(SeedboxAudioProcessor& processor) : PageComponent(processor) {
  externalClockButton_.setButtonText("Clock Source: External");
  externalClockButton_.setTooltip("Shift + Tap = chase external clock.");
  addAndMakeVisible(externalClockButton_);

  followClockButton_.setButtonText("Follow External Clock");
  followClockButton_.setTooltip("Alt + Tap = obey incoming transport.");
  addAndMakeVisible(followClockButton_);

  audioInfo_.setJustificationType(juce::Justification::centredLeft);
  audioInfo_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
  addAndMakeVisible(audioInfo_);

  externalClockAttachment_ = std::make_unique<juce::ButtonParameterAttachment>(
      *processor_.parameters().getParameter("clockSourceExternal"), externalClockButton_);
  followClockAttachment_ = std::make_unique<juce::ButtonParameterAttachment>(
      *processor_.parameters().getParameter("followExternalClock"), followClockButton_);
}

void SettingsPageComponent::refresh() {
  if (auto* dm = processor_.deviceManager()) {
    auto* setup = dm->getCurrentAudioDevice();
    if (setup != nullptr) {
      const auto inputs = setup->getActiveInputChannels().countNumberOfSetBits();
      const auto outputs = setup->getActiveOutputChannels().countNumberOfSetBits();
      audioInfo_.setText("Device: " + setup->getName() + " | Inputs: " + juce::String(inputs) +
                             " | Outputs: " + juce::String(outputs),
                         juce::dontSendNotification);
    } else {
      audioInfo_.setText("No audio device", juce::dontSendNotification);
    }
  }
}

void SettingsPageComponent::resized() {
  auto area = getLocalBounds().reduced(12);
  externalClockButton_.setBounds(area.removeFromTop(36));
  followClockButton_.setBounds(area.removeFromTop(36));
  audioInfo_.setBounds(area.removeFromTop(24));
}

SeedboxAudioProcessorEditor::SeedboxAudioProcessorEditor(SeedboxAudioProcessor& processor)
    : juce::AudioProcessorEditor(&processor), processor_(processor) {
  setWantsKeyboardFocus(true);
  grabKeyboardFocus();

  showAdvanced_ = true;

  if (!useLegacyUi_) {
    setSize(960, 720);
    setResizable(true, true);
    if (auto* constrainer = getConstrainer()) {
      constrainer->setFixedAspectRatio(720.0 / 520.0);
    }
    panelView_ = std::make_unique<SeedboxPanelView>(processor_, processor_.deviceManager());
    addAndMakeVisible(panelView_.get());

    advancedToggle_.setButtonText("Advanced / Pages");
    advancedToggle_.setTooltip(
        "Show the legacy HOME/SEEDS/ENGINE/PERF/SWING/SETTINGS/UTIL stack without rebuilding with SEEDBOX_LEGACY_UI.");
    advancedToggle_.onClick = [this]() { setAdvancedVisible(!showAdvanced_); };
    addAndMakeVisible(advancedToggle_);

    advancedHint_.setText("Advanced pages are live by default—toggle to tuck them away.", juce::dontSendNotification);
    advancedHint_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    advancedHint_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(advancedHint_);
  } else {
    setSize(820, 760);
  }

  advancedFrame_.setText("Advanced Pages");
  advancedFrame_.setTextLabelPosition(juce::Justification::centredLeft);
  advancedFrame_.setColour(juce::GroupComponent::outlineColourId, juce::Colours::dimgrey);
  advancedFrame_.setColour(juce::GroupComponent::textColourId, juce::Colours::lightgrey);
  advancedFrame_.setInterceptsMouseClicks(false, false);
  addAndMakeVisible(advancedFrame_);

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
    updateVisiblePage();
  };
  modeSelector_.setSelectedId(1);
  addAndMakeVisible(modeSelector_);

  displayLabel_.setJustificationType(juce::Justification::centred);
  displayLabel_.setColour(juce::Label::backgroundColourId, juce::Colours::black);
  displayLabel_.setColour(juce::Label::textColourId, juce::Colours::lime);
  displayLabel_.setFont(juce::Font(16.0f, juce::Font::plain));
  displayLabel_.setBorderSize(juce::BorderSize<int>(4));
    displayLabel_.setTooltip("Desktop control hint: press O for Tone, S for Shift, A for Alt.");
  addAndMakeVisible(displayLabel_);

  shortcutsLabel_.setJustificationType(juce::Justification::centred);
  shortcutsLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
  shortcutsLabel_.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.3f));
  shortcutsLabel_.setText("Shortcuts: Space=Latch, T=Tap, O=Tone/Tilt, 1-4=Focus seed, E=Cycle engine, Arrows=Nudge",
                          juce::dontSendNotification);
  shortcutsLabel_.setFont(juce::Font(13.0f));
  addAndMakeVisible(shortcutsLabel_);

  audioSelectorHint_.setJustificationType(juce::Justification::centredLeft);
  audioSelectorHint_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

  audioInitWarning_.setJustificationType(juce::Justification::centredLeft);
  audioInitWarning_.setColour(juce::Label::textColourId, juce::Colours::orange);
  audioInitWarning_.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.25f));
  audioInitWarning_.setVisible(false);
  addAndMakeVisible(audioInitWarning_);

  homePage_ = std::make_unique<HomePageComponent>(processor_);
  seedsPage_ = std::make_unique<SeedsPageComponent>(processor_);
  enginePage_ = std::make_unique<EnginePageComponent>(processor_);
  perfPage_ = std::make_unique<PerfPageComponent>(processor_);
  swingPage_ = std::make_unique<SwingPageComponent>(processor_);
  utilPage_ = std::make_unique<UtilPageComponent>(processor_);
  settingsPage_ = std::make_unique<SettingsPageComponent>(processor_);
  perfPage_->setTapHandler([this]() { handleTapTempo(); });

  addAndMakeVisible(homePage_.get());
  addChildComponent(seedsPage_.get());
  addChildComponent(enginePage_.get());
  addChildComponent(perfPage_.get());
  addChildComponent(swingPage_.get());
  addChildComponent(utilPage_.get());
  addChildComponent(settingsPage_.get());

  buildAudioSelector();
  setAdvancedVisible(showAdvanced_);
  refreshDisplay();
  startTimerHz(15);
}

SeedboxAudioProcessorEditor::~SeedboxAudioProcessorEditor() { stopTimer(); }

void SeedboxAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colours::darkslategrey); }

void SeedboxAudioProcessorEditor::resized() {
  auto layoutAdvanced = [this](juce::Rectangle<int> area) {
    advancedFrame_.setBounds(area);
    auto inner = area.reduced(8);
    auto header = inner.removeFromTop(44);
    modeSelector_.setBounds(header.removeFromLeft(std::min(260, header.getWidth())));

    if (audioInitWarning_.isVisible()) {
      auto warningArea = inner.removeFromTop(52);
      audioInitWarning_.setBounds(warningArea);
    }

    auto body = inner.removeFromBottom(48);
    shortcutsLabel_.setBounds(body.removeFromLeft(std::min(520, body.getWidth())));
    displayLabel_.setBounds(body);

    const bool hasDeviceManager = processor_.deviceManager() != nullptr;
    const bool selectorVisible = audioSelector_ && audioSelector_->isVisible();
    const bool selectorSpaceNeeded =
        audioSelectorHint_.isVisible() || (hasDeviceManager && processor_.appState().mode() == AppState::Mode::SETTINGS);

    if (selectorSpaceNeeded) {
      const auto hintArea = inner.removeFromBottom(32);
      if (audioSelectorHint_.isVisible()) {
        audioSelectorHint_.setBounds(hintArea);
      }

      auto selectorBounds = inner.removeFromBottom(220);
      if (selectorVisible) {
        audioSelector_->setBounds(selectorBounds);
      }
    }

    if (homePage_) homePage_->setBounds(inner);
    if (seedsPage_) seedsPage_->setBounds(inner);
    if (enginePage_) enginePage_->setBounds(inner);
    if (perfPage_) perfPage_->setBounds(inner);
    if (swingPage_) swingPage_->setBounds(inner);
    if (utilPage_) utilPage_->setBounds(inner);
    if (settingsPage_) settingsPage_->setBounds(inner);
  };

  if (useLegacyUi_) {
    layoutAdvanced(getLocalBounds().reduced(12));
    return;
  }

  auto bounds = getLocalBounds().reduced(8);
  auto header = bounds.removeFromTop(36);
  advancedToggle_.setBounds(header.removeFromRight(180));
  advancedHint_.setBounds(header);

  if (advancedUiEnabled()) {
    const int advancedWidth = std::min(420, bounds.getWidth() / 2 + 120);
    auto advancedArea = bounds.removeFromRight(advancedWidth);
    layoutAdvanced(advancedArea);
  } else {
    advancedFrame_.setBounds({});
  }

  if (panelView_) {
    panelView_->setBounds(bounds);
  }
}

void SeedboxAudioProcessorEditor::timerCallback() {
  if (advancedUiEnabled()) {
    const int appModeId = static_cast<int>(processor_.appState().mode()) + 1;
    if (modeSelector_.getSelectedId() != appModeId) {
      modeSelector_.setSelectedId(appModeId, juce::dontSendNotification);
      updateVisiblePage();
    }
    refreshAllPages();
    refreshDisplay();
  }
  if (panelView_) {
    panelView_->refresh();
  }
  syncKeyboardButtons();
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
  if (panelView_) {
    panelView_->syncKeyboardModifiers(toneDown, shiftDown, altDown);
  }
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
  const int code = key.getKeyCode();
  const int lower = std::tolower(code);

  if (code == juce::KeyPress::spaceKey) {
    const bool next = !processor_.appState().transportLatchEnabled();
    processor_.appState().setTransportLatchFromHost(next);
    if (advancedUiEnabled() && perfPage_) {
      perfPage_->refresh();
    }
    return true;
  }

  if (lower == 't') {
    handleTapTempo();
    return true;
  }

  if (lower >= '1' && lower <= '4') {
    const int index = lower - '1';
    processor_.appState().setFocusSeed(static_cast<std::uint8_t>(index));
    if (panelView_) panelView_->refresh();
    if (advancedUiEnabled()) refreshAllPages();
    return true;
  }

  if (lower == 'e') {
    if (advancedUiEnabled() && enginePage_) {
      const int current = enginePage_->currentEngineId();
      const int total = std::max(1, enginePage_->engineCount());
      const int next = ((current - 1 + total) % total) + 1;
      enginePage_->setEngineId(next);
      return true;
    }
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(processor_.parameters().getParameter("seedEngine"))) {
      const int total = std::max(1, choice->choices.size());
      const int next = (choice->getIndex() + 1) % total;
      const float normalized = static_cast<float>(next) / static_cast<float>(std::max(1, total - 1));
      choice->setValueNotifyingHost(normalized);
      processor_.appState().setSeedEngine(processor_.appState().focusSeed(), static_cast<std::uint8_t>(next));
      if (panelView_) panelView_->refresh();
      return true;
    }
  }

  if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey) {
    const double delta = (code == juce::KeyPress::leftKey) ? -1.0 : 1.0;
    nudgeVisibleControl(delta);
    return true;
  }

  return handleButtonKey(code, true);
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

void SeedboxAudioProcessorEditor::handleTapTempo() {
  const double now = juce::Time::getMillisecondCounterHiRes();
  if (lastTapMs_ > 0.0) {
    const double delta = now - lastTapMs_;
    processor_.appState().recordTapTempoInterval(static_cast<uint32_t>(delta));
  }
  lastTapMs_ = now;
}

void SeedboxAudioProcessorEditor::nudgeVisibleControl(double delta) {
  if (!advancedUiEnabled()) {
    if (panelView_) panelView_->nudgeActiveControl(delta);
    return;
  }
  const auto mode = processor_.appState().mode();
  switch (mode) {
    case AppState::Mode::ENGINE:
      if (enginePage_) enginePage_->nudgeFirstKnob(delta);
      break;
    case AppState::Mode::PERF:
      if (perfPage_ && perfPage_->tempoSlider()) nudgeSlider(*perfPage_->tempoSlider(), delta);
      break;
    case AppState::Mode::SWING:
      if (swingPage_ && swingPage_->swingSlider()) nudgeSlider(*swingPage_->swingSlider(), delta);
      break;
    case AppState::Mode::SEEDS:
      if (seedsPage_ && seedsPage_->toneSlider()) nudgeSlider(*seedsPage_->toneSlider(), delta);
      break;
    default:
      if (homePage_ && homePage_->masterSeedSlider()) nudgeSlider(*homePage_->masterSeedSlider(), delta);
      break;
  }
}

bool SeedboxAudioProcessorEditor::advancedUiEnabled() const { return useLegacyUi_ || showAdvanced_; }

void SeedboxAudioProcessorEditor::setAdvancedVisible(bool visible) {
  showAdvanced_ = useLegacyUi_ || visible;
  const int appModeId = static_cast<int>(processor_.appState().mode()) + 1;
  modeSelector_.setSelectedId(appModeId, juce::dontSendNotification);
  updateVisiblePage();
  refreshAllPages();
  resized();
  if (advancedUiEnabled()) {
    advancedFrame_.toFront(false);
    modeSelector_.toFront(false);
    displayLabel_.toFront(false);
    shortcutsLabel_.toFront(false);
    audioInitWarning_.toFront(false);
    audioSelectorHint_.toFront(false);
    if (audioSelector_) audioSelector_->toFront(false);
    if (homePage_) homePage_->toFront(false);
    if (seedsPage_) seedsPage_->toFront(false);
    if (enginePage_) enginePage_->toFront(false);
    if (perfPage_) perfPage_->toFront(false);
    if (swingPage_) swingPage_->toFront(false);
    if (settingsPage_) settingsPage_->toFront(false);
    if (utilPage_) utilPage_->toFront(false);
  }
}

void SeedboxAudioProcessorEditor::updateVisiblePage() {
  const bool advancedVisible = advancedUiEnabled();
  advancedFrame_.setVisible(advancedVisible);
  modeSelector_.setVisible(advancedVisible);
  shortcutsLabel_.setVisible(advancedVisible);
  displayLabel_.setVisible(advancedVisible);
  audioInitWarning_.setVisible(advancedVisible && lastDeviceInitError_.isNotEmpty());
  audioSelectorHint_.setVisible(false);
  if (!advancedVisible) {
    if (homePage_) homePage_->setVisible(false);
    if (seedsPage_) seedsPage_->setVisible(false);
    if (enginePage_) enginePage_->setVisible(false);
    if (perfPage_) perfPage_->setVisible(false);
    if (settingsPage_) settingsPage_->setVisible(false);
    if (utilPage_) utilPage_->setVisible(false);
    if (swingPage_) swingPage_->setVisible(false);
    if (audioSelector_) audioSelector_->setVisible(false);
    return;
  }
  const auto mode = processor_.appState().mode();
  if (homePage_) homePage_->setVisible(mode == AppState::Mode::HOME);
  if (seedsPage_) seedsPage_->setVisible(mode == AppState::Mode::SEEDS);
  if (enginePage_) enginePage_->setVisible(mode == AppState::Mode::ENGINE);
  if (perfPage_) perfPage_->setVisible(mode == AppState::Mode::PERF);
  if (settingsPage_) settingsPage_->setVisible(mode == AppState::Mode::SETTINGS);
  if (utilPage_) utilPage_->setVisible(mode == AppState::Mode::UTIL);
  if (swingPage_) swingPage_->setVisible(mode == AppState::Mode::SWING);
  if (audioSelector_) audioSelector_->setVisible(mode == AppState::Mode::SETTINGS);
  audioSelectorHint_.setVisible(mode == AppState::Mode::SETTINGS && audioSelector_ == nullptr);
  resized();
}

void SeedboxAudioProcessorEditor::refreshAllPages() {
  if (!advancedUiEnabled()) {
    return;
  }
  if (homePage_ && homePage_->isVisible()) homePage_->refresh();
  if (seedsPage_ && seedsPage_->isVisible()) seedsPage_->refresh();
  if (enginePage_ && enginePage_->isVisible()) enginePage_->refresh();
  if (perfPage_ && perfPage_->isVisible()) perfPage_->refresh();
  if (swingPage_ && swingPage_->isVisible()) swingPage_->refresh();
  if (utilPage_ && utilPage_->isVisible()) utilPage_->refresh();
  if (settingsPage_ && settingsPage_->isVisible()) settingsPage_->refresh();
}

void SeedboxAudioProcessorEditor::buildAudioSelector() {
  if (processor_.deviceManager() == nullptr) {
    audioSelectorHint_.setText("Host plugin builds let the DAW pick I/O. Standalone gets a full selector.",
                               juce::dontSendNotification);
    addAndMakeVisible(audioSelectorHint_);
    return;
  }

  const int numInputs = processor_.getTotalNumInputChannels();
  const int numOutputs = processor_.getTotalNumOutputChannels();
  audioSelector_ = std::make_unique<juce::AudioDeviceSelectorComponent>(
      *processor_.deviceManager(), numInputs > 0 ? 1 : 0, std::max(1, numInputs), numOutputs > 0 ? 1 : 0,
      std::max(1, numOutputs), false, false, true, false);
  addAndMakeVisible(audioSelector_.get());
  audioSelector_->setVisible(advancedUiEnabled() && processor_.appState().mode() == AppState::Mode::SETTINGS);
}

void SeedboxAudioProcessorEditor::setLastDeviceInitError(const juce::String& errorMessage) {
  lastDeviceInitError_ = errorMessage;
  if (lastDeviceInitError_.isNotEmpty()) {
    audioInitWarning_.setText("Audio init hiccup: " + lastDeviceInitError_ + " | Open SETTINGS to pick a device.",
                              juce::dontSendNotification);
  }
  updateVisiblePage();
}

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
