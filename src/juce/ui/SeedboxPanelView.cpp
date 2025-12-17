#include "juce/ui/SeedboxPanelView.h"

#if SEEDBOX_JUCE

#include <cmath>

#include "hal/Board.h"

namespace seedbox::juce_bridge {
namespace {
constexpr float kViewWidth = 720.0f;
constexpr float kViewHeight = 520.0f;
constexpr float kPanelX = 40.0f;
constexpr float kPanelY = 40.0f;
constexpr float kPanelWidth = 640.0f;
constexpr float kPanelHeight = 360.0f;

juce::String engineName(int index) {
  switch (index) {
    case 0:
      return "Default";
    case 1:
      return "Grain";
    case 2:
      return "Chord";
    case 3:
      return "Drum";
    case 4:
      return "FM";
    case 5:
      return "Additive";
    case 6:
      return "Resonator";
    case 7:
      return "Noise";
    default:
      return "Engine";
  }
}

juce::Colour accentColour() { return juce::Colour::fromRGB(0x66, 0xCC, 0xFF); }
}  // namespace

SeedboxPanelView::PanelLookAndFeel::PanelLookAndFeel() {
  setColour(juce::Slider::thumbColourId, accentColour());
  setColour(juce::TextButton::buttonColourId, juce::Colours::darkslategrey);
  setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
  setColour(juce::TextButton::textColourOnId, accentColour());
}

void SeedboxPanelView::PanelLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                                          double sliderPos, double rotaryStartAngle,
                                                          double rotaryEndAngle, juce::Slider& slider) {
  const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height))
                          .reduced(4.0f);
  const float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f;
  const juce::Point<float> centre = bounds.getCentre();
  const float lineWidth = 3.0f;
  const float tickRadius = radius - 6.0f;

  g.setColour(juce::Colours::darkgrey.withBrightness(0.3f));
  g.fillEllipse(bounds);
  g.setColour(juce::Colours::black.withAlpha(0.7f));
  g.fillEllipse(bounds.reduced(4.0f));

  const float angle = static_cast<float>(rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle));
  const auto thumb = centre.getPointOnCircumference(tickRadius, angle);

  g.setColour(accentColour());
  g.drawLine(centre.x, centre.y, thumb.x, thumb.y, lineWidth);
  g.drawEllipse(bounds, 2.0f);

  g.setColour(juce::Colours::whitesmoke.withAlpha(0.9f));
  g.fillEllipse(thumb.x - 5.0f, thumb.y - 5.0f, 10.0f, 10.0f);
}

void SeedboxPanelView::PanelLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                              const juce::Colour& backgroundColour,
                                                              bool shouldDrawButtonAsHighlighted,
                                                              bool shouldDrawButtonAsDown) {
  auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
  const float corner = 8.0f;
  juce::Colour base = backgroundColour.darker(0.2f);
  if (button.getToggleState()) {
    base = accentColour();
  }
  if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown) {
    base = base.brighter(0.2f);
  }
  g.setColour(base);
  g.fillRoundedRectangle(bounds, corner);
  g.setColour(base.brighter(0.4f));
  g.drawRoundedRectangle(bounds, corner, 1.6f);
}

SeedboxPanelView::PanelKnob::PanelKnob() {
  setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
}

void SeedboxPanelView::PanelKnob::mouseUp(const juce::MouseEvent& event) {
  juce::Slider::mouseUp(event);
  if (!event.mouseWasDraggedSinceMouseDown() && onPress) {
    onPress();
  }
}

SeedboxPanelView::JackIcon::JackIcon(juce::String name, std::function<void()> onClick)
    : label_(std::move(name)), onClick_(std::move(onClick)) {
  setInterceptsMouseClicks(true, false);
}

void SeedboxPanelView::JackIcon::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat();
  const float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f;
  juce::Point<float> centre = bounds.getCentre();
  g.setColour(juce::Colours::darkslategrey.darker(0.4f));
  g.fillEllipse(bounds);
  g.setColour(accentColour());
  g.drawEllipse(bounds, 2.0f);
  g.setColour(juce::Colours::whitesmoke);
  g.fillEllipse(centre.x - radius * 0.35f, centre.y - radius * 0.35f, radius * 0.7f, radius * 0.7f);
  g.setColour(juce::Colours::black.withAlpha(0.7f));
  g.fillEllipse(centre.x - radius * 0.15f, centre.y - radius * 0.15f, radius * 0.3f, radius * 0.3f);

  g.setColour(juce::Colours::whitesmoke);
  g.setFont(juce::Font(12.0f, juce::Font::plain));
  g.drawFittedText(label_, getLocalBounds().translated(0, static_cast<int>(radius) + 6), juce::Justification::centredTop,
                   1);
}

void SeedboxPanelView::JackIcon::mouseUp(const juce::MouseEvent& event) {
  if (!event.mouseWasDraggedSinceMouseDown() && onClick_) {
    onClick_();
  }
}

SeedboxPanelView::SeedboxPanelView(SeedboxAudioProcessor& processor) : processor_(processor) {
  setLookAndFeel(&lookAndFeel_);

  auto setupKnob = [&](LabeledKnob& target, const juce::String& name, const juce::String& helper) {
    target.knob.setRange(0.0, 1.0, 0.01);
    target.label.setText(name, juce::dontSendNotification);
    target.label.setJustificationType(juce::Justification::centred);
    target.label.setColour(juce::Label::textColourId, juce::Colours::whitesmoke);
    target.helper.setText(helper, juce::dontSendNotification);
    target.helper.setJustificationType(juce::Justification::centred);
    target.helper.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(target.knob);
    addAndMakeVisible(target.label);
    addAndMakeVisible(target.helper);
  };

  setupKnob(seedKnob_, "Seed Bank", "Focus");
  seedKnob_.knob.setRange(0.0, 3.0, 1.0);
  seedKnob_.knob.onValueChange = [this]() {
    const auto value = static_cast<int>(std::round(seedKnob_.knob.getValue()));
    processor_.appState().setFocusSeed(static_cast<std::uint8_t>(value));
    if (auto* param = processor_.parameters().getParameter("focusSeed")) {
      param->setValueNotifyingHost(value / 3.0f);
    }
    updateEngineLabel();
    updateLockIndicator();
    lastActive_ = &seedKnob_.knob;
  };
  seedKnob_.knob.onPress = [this]() {
    const int next = (static_cast<int>(processor_.appState().focusSeed()) + 1) % 4;
    seedKnob_.knob.setValue(next, juce::sendNotificationSync);
  };

  setupKnob(densityKnob_, "Density", "Hits");
  densityKnob_.knob.setRange(0.0, 8.0, 0.05);
  densityKnob_.knob.onValueChange = [this]() {
    processor_.applySeedEdit(juce::Identifier{"density"}, densityKnob_.knob.getValue(),
                             [v = densityKnob_.knob.getValue()](Seed& seed) {
                               seed.density = juce::jlimit(0.0f, 8.0f, static_cast<float>(v));
                             });
    lastActive_ = &densityKnob_.knob;
  };

  setupKnob(toneKnob_, "Tone/Tilt", "Bright/Soft");
  toneKnob_.knob.setRange(0.0, 1.0, 0.01);
  toneKnob_.knob.onValueChange = [this]() {
    processor_.applySeedEdit(juce::Identifier{"tone"}, toneKnob_.knob.getValue(),
                             [v = toneKnob_.knob.getValue()](Seed& seed) {
                               seed.tone = juce::jlimit(0.0f, 1.0f, static_cast<float>(v));
                             });
    lastActive_ = &toneKnob_.knob;
  };

  setupKnob(fxKnob_, "FX/Mutate", "Spread");
  fxKnob_.knob.setRange(0.0, 1.0, 0.01);
  fxKnob_.knob.onValueChange = [this]() {
    processor_.applySeedEdit(juce::Identifier{"spread"}, fxKnob_.knob.getValue(),
                             [v = fxKnob_.knob.getValue()](Seed& seed) {
                               seed.spread = juce::jlimit(0.0f, 1.0f, static_cast<float>(v));
                             });
    lastActive_ = &fxKnob_.knob;
  };
  fxKnob_.knob.onPress = [this]() {
    auto* choice = dynamic_cast<juce::AudioParameterChoice*>(processor_.parameters().getParameter("seedEngine"));
    if (choice == nullptr) {
      return;
    }
    const int current = choice->getIndex();
    const int next = (current + 1) % choice->choices.size();
    const float normalized = static_cast<float>(next) / static_cast<float>(std::max(1, choice->choices.size() - 1));
    choice->setValueNotifyingHost(normalized);
    processor_.appState().setSeedEngine(processor_.appState().focusSeed(), static_cast<std::uint8_t>(next));
    updateEngineLabel();
  };

  auto setupButton = [&](juce::TextButton& btn, const juce::String& label) {
    btn.setButtonText(label);
    btn.setColour(juce::TextButton::buttonColourId, juce::Colours::dimgrey);
    addAndMakeVisible(btn);
  };

  setupButton(tapButton_, "Tap Tempo");
  tapButton_.onMouseDown = [this](const juce::MouseEvent&) { lastTapMs_ = juce::Time::getMillisecondCounterHiRes(); };
  tapButton_.onMouseUp = [this](const juce::MouseEvent& evt) {
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double delta = now - lastTapMs_;
    handleTap(delta > 600.0);
    tapButton_.setToggleState(false, juce::dontSendNotification);
    juce::ignoreUnused(evt);
  };

  setupButton(shiftButton_, "Shift");
  shiftButton_.setClickingTogglesState(false);
  shiftButton_.onMouseDown = [this](const juce::MouseEvent&) { setShiftHeld(true, false); };
  shiftButton_.onMouseUp = [this](const juce::MouseEvent&) { setShiftHeld(false, false); };

  setupButton(altButton_, "Alt / Storage");
  altButton_.setClickingTogglesState(false);
  altButton_.onMouseDown = [this](const juce::MouseEvent&) { setAltHeld(true, false); };
  altButton_.onMouseUp = [this](const juce::MouseEvent&) { setAltHeld(false, false); };

  setupButton(reseedButton_, "Reseed");
  reseedButton_.onClick = [this]() { handleReseed(); };

  setupButton(lockButton_, "Lock");
  lockButton_.setClickingTogglesState(true);
  lockButton_.onClick = [this]() { handleLock(); };

  oledLabel_.setJustificationType(juce::Justification::centred);
  oledLabel_.setColour(juce::Label::backgroundColourId, juce::Colours::black);
  oledLabel_.setColour(juce::Label::textColourId, juce::Colours::aqua);
  oledLabel_.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
  addAndMakeVisible(oledLabel_);

  engineNameLabel_.setJustificationType(juce::Justification::centred);
  engineNameLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
  addAndMakeVisible(engineNameLabel_);

  jackIcons_.add(new JackIcon("MIDI In", [this]() {
    processor_.appState().setClockSourceExternalFromHost(true);
  }));
  jackIcons_.add(new JackIcon("MIDI Out", [this]() {
    processor_.appState().setFollowExternalClockFromHost(true);
  }));
  jackIcons_.add(new JackIcon("Headphone", [this]() {
    if (auto* dm = processor_.deviceManager()) {
      dm->restartLastAudioDevice();
    }
  }));
  jackIcons_.add(new JackIcon("USB", nullptr));
  jackIcons_.add(new JackIcon("DC", nullptr));
  for (auto* jack : jackIcons_) {
    addAndMakeVisible(jack);
  }

  applySensitivity();
  refresh();
}

SeedboxPanelView::~SeedboxPanelView() { setLookAndFeel(nullptr); }

void SeedboxPanelView::paint(juce::Graphics& g) {
  g.fillAll(juce::Colours::darkslategrey.darker(0.6f));
  paintPanel(g);
}

void SeedboxPanelView::paintPanel(juce::Graphics& g) {
  g.setColour(juce::Colours::darkslategrey.darker(0.2f));
  g.fillRoundedRectangle(panelBounds_, 18.0f);
  g.setColour(accentColour());
  g.drawRoundedRectangle(panelBounds_, 18.0f, 2.0f);
}

void SeedboxPanelView::resized() { layoutControls(); }

void SeedboxPanelView::layoutControls() {
  const auto bounds = getLocalBounds().toFloat();
  const float scale = std::min(bounds.getWidth() / kViewWidth, bounds.getHeight() / kViewHeight);
  const juce::Point<float> viewOrigin(bounds.getCentreX() - (kViewWidth * scale) / 2.0f,
                                      bounds.getCentreY() - (kViewHeight * scale) / 2.0f);
  viewBounds_ = {viewOrigin.x, viewOrigin.y, kViewWidth * scale, kViewHeight * scale};
  panelBounds_ = {viewOrigin.x + kPanelX * scale, viewOrigin.y + kPanelY * scale, kPanelWidth * scale,
                  kPanelHeight * scale};

  auto placeKnob = [&](LabeledKnob& target, float baseX, float baseY) {
    const juce::Point<float> centre(viewOrigin.x + baseX * scale, viewOrigin.y + baseY * scale);
    const float r = 32.0f * scale;
    juce::Rectangle<float> knobArea(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    target.knob.setBounds(knobArea.toNearestInt());
    auto labelArea = knobArea.withY(knobArea.getBottom() + 6.0f * scale).withHeight(22.0f * scale);
    target.label.setBounds(labelArea.toNearestInt());
    target.helper.setBounds(labelArea.translated(0, static_cast<float>(labelArea.getHeight())).toNearestInt());
  };

  placeKnob(seedKnob_, 150.0f, 170.0f);
  placeKnob(densityKnob_, 280.0f, 170.0f);
  placeKnob(toneKnob_, 440.0f, 170.0f);
  placeKnob(fxKnob_, 570.0f, 170.0f);

  auto placeButton = [&](juce::TextButton& btn, float baseX, float baseY) {
    const juce::Rectangle<float> area(viewOrigin.x + baseX * scale, viewOrigin.y + baseY * scale, 60.0f * scale,
                                      40.0f * scale);
    btn.setBounds(area.toNearestInt());
  };

  placeButton(tapButton_, 120.0f, 250.0f);
  placeButton(shiftButton_, 220.0f, 250.0f);
  placeButton(altButton_, 320.0f, 250.0f);
  placeButton(reseedButton_, 420.0f, 250.0f);
  placeButton(lockButton_, 520.0f, 250.0f);

  auto placeJack = [&](JackIcon* jack, float baseX, float baseY) {
    const juce::Point<float> centre(viewOrigin.x + baseX * scale, viewOrigin.y + baseY * scale);
    const float r = 20.0f * scale;
    jack->setBounds(juce::Rectangle<float>(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f).toNearestInt());
  };

  if (jackIcons_.size() >= 5) {
    placeJack(jackIcons_[0], 180.0f, 360.0f);
    placeJack(jackIcons_[1], 280.0f, 360.0f);
    placeJack(jackIcons_[2], 380.0f, 360.0f);
    placeJack(jackIcons_[3], 480.0f, 360.0f);
    placeJack(jackIcons_[4], 580.0f, 360.0f);
  }

  const juce::Rectangle<float> oledArea(viewOrigin.x + 290.0f * scale, viewOrigin.y + 80.0f * scale,
                                        160.0f * scale, 70.0f * scale);
  oledLabel_.setBounds(oledArea.toNearestInt());
  engineNameLabel_.setBounds(fxKnob_.label.getBounds().withHeight(18).translated(0, -fxKnob_.label.getHeight()));
}

void SeedboxPanelView::refresh() {
  AppState::DisplaySnapshot snapshot{};
  processor_.appState().captureDisplaySnapshot(snapshot);
  juce::String display;
  display << snapshot.title << "\n" << snapshot.status << "\n" << snapshot.metrics << "\n" << snapshot.nuance;
  oledLabel_.setText(display, juce::dontSendNotification);

  const auto& seeds = processor_.appState().seeds();
  const std::size_t focus = seeds.empty() ? 0 : std::min<std::size_t>(processor_.appState().focusSeed(), seeds.size() - 1);
  const Seed* focusSeed = seeds.empty() ? nullptr : &seeds[focus];

  seedKnob_.knob.setValue(static_cast<double>(processor_.appState().focusSeed()), juce::dontSendNotification);
  if (focusSeed != nullptr) {
    densityKnob_.knob.setValue(focusSeed->density, juce::dontSendNotification);
    toneKnob_.knob.setValue(focusSeed->tone, juce::dontSendNotification);
    fxKnob_.knob.setValue(focusSeed->spread, juce::dontSendNotification);
    seedKnob_.helper.setText("Focus: " + juce::String(static_cast<int>(focus) + 1), juce::dontSendNotification);
  } else {
    seedKnob_.helper.setText("No seeds", juce::dontSendNotification);
  }
  updateEngineLabel();
  updateLockIndicator();
  applySensitivity();
}

void SeedboxPanelView::setModifierStates(bool toneHeld, bool shiftHeld, bool altHeld) {
  setToneHeld(toneHeld, true);
  setShiftHeld(shiftHeld, true);
  setAltHeld(altHeld, true);
}

void SeedboxPanelView::setShiftHeld(bool held, bool keyboard) {
  shiftHeldByKeyboard_ = keyboard ? held : shiftHeldByKeyboard_;
  shiftHeldByButton_ = keyboard ? shiftHeldByButton_ : held;
  shiftButton_.setToggleState(shiftActive(), juce::dontSendNotification);
  hal::nativeBoardSetButton(hal::Board::ButtonID::Shift, shiftActive());
  applySensitivity();
}

void SeedboxPanelView::setAltHeld(bool held, bool keyboard) {
  altHeldByKeyboard_ = keyboard ? held : altHeldByKeyboard_;
  altHeldByButton_ = keyboard ? altHeldByButton_ : held;
  altButton_.setToggleState(altActive(), juce::dontSendNotification);
  hal::nativeBoardSetButton(hal::Board::ButtonID::AltSeed, altActive());
  applySensitivity();
}

void SeedboxPanelView::setToneHeld(bool held, bool keyboard) {
  toneHeldByKeyboard_ = keyboard ? held : toneHeldByKeyboard_;
  toneHeldByButton_ = keyboard ? toneHeldByButton_ : held;
  hal::nativeBoardSetButton(hal::Board::ButtonID::EncoderToneTilt, toneActive());
}

void SeedboxPanelView::applySensitivity() {
  const double fine = shiftActive() ? 0.01 : 0.05;
  densityKnob_.knob.setInterval(fine);
  toneKnob_.knob.setInterval(shiftActive() ? 0.005 : 0.01);
  fxKnob_.knob.setInterval(shiftActive() ? 0.005 : 0.01);
}

void SeedboxPanelView::handleTap(bool longPress) {
  if (longPress) {
    const bool next = !processor_.appState().transportLatchEnabled();
    processor_.appState().setTransportLatchFromHost(next);
    lastTapMs_ = 0.0;
    return;
  }
  const double now = juce::Time::getMillisecondCounterHiRes();
  if (lastTapMs_ > 0.0) {
    processor_.appState().recordTapTempoInterval(static_cast<uint32_t>(now - lastTapMs_));
  }
  lastTapMs_ = now;
}

void SeedboxPanelView::handleReseed() {
  if (altActive()) {
    saveQuickPreset();
    return;
  }
  processor_.appState().seedPageReseed(processor_.appState().masterSeed(), AppState::SeedPrimeMode::kLfsr);
}

void SeedboxPanelView::handleLock() {
  if (altActive()) {
    recallQuickPreset();
    return;
  }
  processor_.appState().seedPageToggleLock(processor_.appState().focusSeed());
  updateLockIndicator();
}

void SeedboxPanelView::saveQuickPreset() {
  const auto preset = processor_.appState().snapshotPresetForHost("panel");
  processor_.setPanelQuickPreset(preset);
}

void SeedboxPanelView::recallQuickPreset() {
  processor_.applyPanelQuickPreset();
  refresh();
}

void SeedboxPanelView::updateLockIndicator() {
  const bool locked = processor_.appState().isSeedLocked(processor_.appState().focusSeed());
  lockButton_.setToggleState(locked, juce::dontSendNotification);
}

void SeedboxPanelView::updateEngineLabel() {
  const int engineId = static_cast<int>(processor_.appState().seeds().empty() ? 0
                                                                              : processor_.appState().seeds()
                                                                                    [processor_.appState().focusSeed()]
                                                                                        .engine);
  engineLabel_ = engineName(engineId);
  engineNameLabel_.setText("Engine: " + engineLabel_, juce::dontSendNotification);
}

void SeedboxPanelView::nudgeActiveControl(double delta) {
  juce::Slider* target = lastActive_;
  if (target == nullptr) {
    target = &seedKnob_.knob;
  }
  const double step = target->getInterval() > 0.0 ? target->getInterval() : 0.1;
  target->setValue(target->getValue() + (delta * step), juce::sendNotificationSync);
}

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
