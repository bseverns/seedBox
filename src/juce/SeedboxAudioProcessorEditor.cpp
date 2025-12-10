#include "juce/SeedboxAudioProcessorEditor.h"

#if SEEDBOX_JUCE

#include <juce_graphics/juce_graphics.h>

#include <sstream>

#include "app/AppState.h"
#include "juce/SeedboxAudioProcessor.h"

namespace seedbox::juce_bridge {

SeedboxAudioProcessorEditor::SeedboxAudioProcessorEditor(SeedboxAudioProcessor& processor)
    : juce::AudioProcessorEditor(&processor), processor_(processor) {
  setSize(420, 260);

  masterSeedSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  masterSeedSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
  masterSeedSlider_.setTooltip("Master Seed drives the procedural voices; automate it to morph the groove.");
  addAndMakeVisible(masterSeedSlider_);

  displayLabel_.setJustificationType(juce::Justification::centred);
  displayLabel_.setColour(juce::Label::backgroundColourId, juce::Colours::black);
  displayLabel_.setColour(juce::Label::textColourId, juce::Colours::lime);
  displayLabel_.setFont(juce::Font(16.0f, juce::Font::plain));
  displayLabel_.setBorderSize(juce::BorderSize<int>(4));
  addAndMakeVisible(displayLabel_);

  masterSeedAttachment_ = std::make_unique<juce::SliderParameterAttachment>(
      *processor_.parameters().getParameter("masterSeed"), masterSeedSlider_, nullptr);

  refreshDisplay();
  startTimerHz(15);
}

SeedboxAudioProcessorEditor::~SeedboxAudioProcessorEditor() { stopTimer(); }

void SeedboxAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colours::darkslategrey); }

void SeedboxAudioProcessorEditor::resized() {
  auto area = getLocalBounds().reduced(16);
  auto top = area.removeFromTop(140);
  masterSeedSlider_.setBounds(top.withSizeKeepingCentre(180, 140));
  displayLabel_.setBounds(area);
}

void SeedboxAudioProcessorEditor::timerCallback() { refreshDisplay(); }

void SeedboxAudioProcessorEditor::refreshDisplay() {
  AppState::DisplaySnapshot snapshot{};
  processor_.appState().captureDisplaySnapshot(snapshot);
  std::ostringstream stream;
  stream << snapshot.title << "\n" << snapshot.status << "\n" << snapshot.metrics << "\n" << snapshot.nuance;
  displayLabel_.setText(stream.str(), juce::dontSendNotification);
}

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
