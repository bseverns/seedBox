#include "hal/PanelControls.h"
#include "juce/ui/SeedboxPanelView.h"

#if SEEDBOX_JUCE

#include <cmath>

#include <algorithm>

#include <juce_audio_utils/juce_audio_utils.h>

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
      return "Sampler";
    case 1:
      return "Granular";
    case 2:
      return "Resonator";
    case 3:
      return "Euclid";
    case 4:
      return "Burst";
    case 5:
      return "Toy";
    default:
      return "FX";
  }
}

juce::String engineHint(int index) {
  switch (index) {
    case 0:
      return "Playback / drive";
    case 1:
      return "Smear / texture";
    case 2:
      return "Ring / resonance";
    case 3:
      return "Rhythmic gate";
    case 4:
      return "Burst / echo";
    case 5:
      return "Toy color";
    default:
      return "Shape the input";
  }
}

juce::Colour accentColour() { return juce::Colour::fromRGB(0x66, 0xCC, 0xFF); }
}  // namespace

class AudioSelectorHost : public juce::Component {
 public:
  AudioSelectorHost(juce::AudioDeviceManager& manager, int inputChannels, int outputChannels)
      : manager_(manager),
        selector_(manager_, inputChannels > 0 ? 1 : 0, std::max(1, inputChannels), outputChannels > 0 ? 1 : 0,
                  std::max(1, outputChannels), false, false, true, false) {
    addAndMakeVisible(selector_);
    setSize(420, 320);
  }

  ~AudioSelectorHost() override { manager_.restartLastAudioDevice(); }

  void resized() override { selector_.setBounds(getLocalBounds()); }

 private:
  juce::AudioDeviceManager& manager_;
  juce::AudioDeviceSelectorComponent selector_;
};

class AudioDeviceInfoMessage : public juce::Component {
 public:
  explicit AudioDeviceInfoMessage(juce::String text) {
    copyButton_.setButtonText("Copy");
    copyButton_.setTooltip("Copy this device summary to your clipboard.");
    copyButton_.onClick = [msg = text, this]() {
      juce::SystemClipboard::copyTextToClipboard(msg);
      copyButton_.setButtonText("Copied");
    };

    message_.setJustificationType(juce::Justification::centred);
    message_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    message_.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.4f));
    message_.setText(std::move(text), juce::dontSendNotification);

    addAndMakeVisible(message_);
    addAndMakeVisible(copyButton_);
    setSize(420, 140);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(12);
    copyButton_.setBounds(area.removeFromBottom(32).removeFromRight(80));
    message_.setBounds(area);
  }

 private:
  juce::Label message_;
  juce::TextButton copyButton_;
};

SeedboxPanelView::PanelLookAndFeel::PanelLookAndFeel() {
  setColour(juce::Slider::thumbColourId, accentColour());
  setColour(juce::TextButton::buttonColourId, juce::Colours::darkslategrey);
  setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
  setColour(juce::TextButton::textColourOnId, accentColour());
}

void SeedboxPanelView::PanelLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                                          float sliderPos, float rotaryStartAngle,
                                                          float rotaryEndAngle, juce::Slider& slider) {
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

  const float angle = static_cast<float>(slider.getValue()) * juce::MathConstants<float>::twoPi / 24.0f;
  juce::ignoreUnused(sliderPos, rotaryStartAngle, rotaryEndAngle);
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

void SeedboxPanelView::PanelKnob::mouseDown(const juce::MouseEvent& event) {
  if (event.mods.isRightButtonDown()) {
    switchDown_ = true;
    if (onSwitch) onSwitch(true);
    return;
  }
  juce::Slider::mouseDown(event);
}

void SeedboxPanelView::PanelKnob::mouseUp(const juce::MouseEvent& event) {
  if (switchDown_) {
    switchDown_ = false;
    if (onSwitch) onSwitch(false);
    return;
  }
  juce::Slider::mouseUp(event);
}

void SeedboxPanelView::PanelButton::mouseDown(const juce::MouseEvent& event) {
  juce::TextButton::mouseDown(event);
  if (onDown) {
    onDown(event);
  }
}

void SeedboxPanelView::PanelButton::mouseUp(const juce::MouseEvent& event) {
  juce::TextButton::mouseUp(event);
  if (onUp) {
    onUp(event);
  }
}

SeedboxPanelView::JackIcon::JackIcon(juce::String name, MenuBuilder menuBuilder, std::function<void()> onClick)
    : label_(std::move(name)), menuBuilder_(std::move(menuBuilder)), onClick_(std::move(onClick)) {
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
  g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
  g.drawFittedText(label_, getLocalBounds().translated(0, static_cast<int>(radius) + 6), juce::Justification::centredTop,
                   1);
}

void SeedboxPanelView::JackIcon::mouseUp(const juce::MouseEvent& event) {
  if (event.mouseWasDraggedSinceMouseDown()) {
    return;
  }

  if (menuBuilder_) {
    auto menu = menuBuilder_();
    if (menu.getNumItems() > 0) {
      auto options = juce::PopupMenu::Options().withTargetComponent(this);
      if (auto* top = getTopLevelComponent()) {
        options = options.withParentComponent(top);
      }
      menu.showMenuAsync(options);
      return;
    }
  }

  if (onClick_) {
    onClick_();
  }
}

SeedboxPanelView::SeedboxPanelView(SeedboxAudioProcessor& processor, juce::AudioDeviceManager* audioManager)
    : processor_(processor), audioManager_(audioManager != nullptr ? audioManager : processor.deviceManager()) {
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

  const std::array<LabeledKnob*, 4> knobs{{&seedKnob_, &densityKnob_, &toneKnob_, &fxKnob_}};
  for (std::size_t i = 0; i < knobs.size(); ++i) {
    auto& target = *knobs[i];
    const auto& control = hal::panel::encoders[i];
    setupKnob(target, control.label, "Right-click: switch");
    target.knob.setRange(-10000.0, 10000.0, 1.0);
    target.knob.setValue(0.0, juce::dontSendNotification);
    target.knob.setMouseDragSensitivity(200000);
    target.knob.setTooltip("Drag to turn. Right-click and hold to press the encoder switch.");
    target.knob.onValueChange = [this, knob = &target.knob, token = control.token, previous = 0]() mutable {
      const auto value = static_cast<int>(std::round(knob->getValue()));
      const int delta = value - previous;
      previous = value;
      if (delta != 0) {
        hal::nativeBoardFeed(std::string("enc ") + token + " " + std::to_string(delta));
        processor_.controlThreadApp().serviceMaintenance();
      }
      lastActive_ = knob;
    };
    target.knob.onSwitch = [this, id = control.button_id](bool pressed) {
      if (id == hal::Board::ButtonID::EncoderToneTilt) setToneHeld(pressed, false);
      else setPanelButton(id, pressed);
    };
  }

  auto setupButton = [&](PanelButton& btn, const hal::panel::Button& control) {
    btn.setButtonText(control.label);
    btn.setColour(juce::TextButton::buttonColourId, juce::Colours::dimgrey);
    btn.onDown = [this, id = control.id](const juce::MouseEvent&) { setPanelButton(id, true); };
    btn.onUp = [this, id = control.id](const juce::MouseEvent&) { setPanelButton(id, false); };
    addAndMakeVisible(btn);
  };
  setupButton(tapButton_, hal::panel::buttons[0]);
  setupButton(shiftButton_, hal::panel::buttons[1]);
  shiftButton_.onDown = [this](const juce::MouseEvent&) { setShiftHeld(true, false); };
  shiftButton_.onUp = [this](const juce::MouseEvent&) { setShiftHeld(false, false); };
  setupButton(altButton_, hal::panel::buttons[2]);
  altButton_.onDown = [this](const juce::MouseEvent&) { setAltHeld(true, false); };
  altButton_.onUp = [this](const juce::MouseEvent&) { setAltHeld(false, false); };
  setupButton(captureButton_, hal::panel::buttons[3]);
  captureButton_.setTooltip("Press: Live Capture. Hold: panic after capture.");

  oledLabel_.setJustificationType(juce::Justification::centred);
  oledLabel_.setColour(juce::Label::backgroundColourId, juce::Colours::black);
  oledLabel_.setColour(juce::Label::textColourId, juce::Colours::aqua);
  oledLabel_.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
  addAndMakeVisible(oledLabel_);

  clockStatusLabel_.setJustificationType(juce::Justification::centred);
  clockStatusLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
  clockStatusLabel_.setFont(juce::FontOptions(11.0f, juce::Font::plain));
  addAndMakeVisible(clockStatusLabel_);

  engineNameLabel_.setJustificationType(juce::Justification::centred);
  engineNameLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
  addAndMakeVisible(engineNameLabel_);

  engineHintLabel_.setJustificationType(juce::Justification::centred);
  engineHintLabel_.setColour(juce::Label::textColourId, juce::Colours::silver);
  engineHintLabel_.setFont(juce::FontOptions(11.0f));
  addAndMakeVisible(engineHintLabel_);

  jackIcons_.add(new JackIcon("MIDI In", [this]() {
    juce::PopupMenu menu;
    const bool follow = processor_.readThreadApp().followExternalClockEnabled();

    menu.addItem("Use MIDI In as clock source", true, follow,
                 [this]() { processor_.controlThreadApp().setClockSourceExternal(true); });
    menu.addItem("Use internal clock", true, !follow,
                 [this]() { processor_.controlThreadApp().setClockSourceExternal(false); });
    menu.addSeparator();
    menu.addItem("Follow external clock", true, follow,
                 [this]() { processor_.controlThreadApp().setFollowExternalClock(true); });
    menu.addItem("Ignore external clock", true, !follow,
                 [this]() { processor_.controlThreadApp().setFollowExternalClock(false); });
    return menu;
  }));
  jackIcons_.add(new JackIcon("MIDI Out", [this]() {
    juce::PopupMenu menu;
    const bool follow = processor_.readThreadApp().followExternalClockEnabled();

    menu.addItem("Send MIDI clock from host", true, follow,
                 [this]() { processor_.controlThreadApp().setClockSourceExternal(true); });
    menu.addItem("Keep SeedBox internal clock", true, !follow,
                 [this]() { processor_.controlThreadApp().setClockSourceExternal(false); });
    menu.addSeparator();
    menu.addItem("Follow external transport/clock", true, follow,
                 [this]() { processor_.controlThreadApp().setFollowExternalClock(true); });
    menu.addItem("Stop following host clock", true, !follow,
                 [this]() { processor_.controlThreadApp().setFollowExternalClock(false); });
    return menu;
  }));
  jackIcons_.add(new JackIcon("Headphone", [this]() {
    juce::PopupMenu menu;
    auto* dm = audioManager_ != nullptr ? audioManager_ : processor_.deviceManager();
    auto* jack = jackIcons_[2];
    const auto target = jack != nullptr ? jack->getScreenBounds() : getScreenBounds().toNearestInt();
    juce::Component* anchor = jack != nullptr ? static_cast<juce::Component*>(jack) : static_cast<juce::Component*>(this);
    auto* top = anchor != nullptr ? anchor->getTopLevelComponent() : nullptr;
    const auto localTarget = top != nullptr ? top->getLocalArea(nullptr, target) : target;
    juce::Component* calloutParent = top != nullptr ? top : anchor;
    if (dm != nullptr) {
      menu.addItem("Audio I/O...", true, false, [this, dm, localTarget, calloutParent]() {
        const int numInputs = processor_.getTotalNumInputChannels();
        const int numOutputs = processor_.getTotalNumOutputChannels();
        auto selector = std::make_unique<AudioSelectorHost>(*dm, numInputs, numOutputs);
        juce::CallOutBox::launchAsynchronously(std::move(selector), localTarget, calloutParent);
      });
      if (auto* device = dm->getCurrentAudioDevice()) {
        const auto inputs = device->getActiveInputChannels().countNumberOfSetBits();
        const auto outputs = device->getActiveOutputChannels().countNumberOfSetBits();
        const juce::String summary = "Device: " + device->getName() + "\nInputs: " + juce::String(inputs) +
                                    " | Outputs: " + juce::String(outputs) +
                                    "\nSample Rate: " + juce::String(device->getCurrentSampleRate(), 1) + " Hz | Block Size: " +
                                    juce::String(device->getCurrentBufferSizeSamples()) + " samples";
        menu.addItem("Device summary", true, false, [calloutParent, localTarget, summary]() mutable {
          auto info = std::make_unique<AudioDeviceInfoMessage>(summary);
          juce::CallOutBox::launchAsynchronously(std::move(info), localTarget, calloutParent);
        });
      }
      menu.addSeparator();
      menu.addItem("Restart audio engine", true, false, [dm]() { dm->restartLastAudioDevice(); });
    } else {
      menu.addItem("Why can't I pick audio here?", true, false, [calloutParent, localTarget]() {
        auto info = std::make_unique<AudioDeviceInfoMessage>(
            "Host plugin builds let the DAW pick I/O.\n"
            "Pop open your DAW's audio/device prefs to switch drivers or ports.\n"
            "Standalone SeedBox exposes the full selector here.");
        juce::CallOutBox::launchAsynchronously(std::move(info), localTarget, calloutParent);
      });
    }
    return menu;
  }));
  jackIcons_.add(new JackIcon("USB", [this]() {
    juce::PopupMenu menu;
#if !SEEDBOX_HW
    const auto controllers = hal::nativeEnumerateControllers();
#else
    const std::vector<std::string> controllers;
#endif
    if (controllers.empty()) {
      menu.addItem("No controller connected", false, true, nullptr);
    } else {
      for (const auto& name : controllers) {
        menu.addItem(name, true, false, []() {});
      }
    }
    return menu;
  }));
  jackIcons_.add(new JackIcon("DC", [this]() {
    juce::PopupMenu menu;
#if JucePlugin_Build_Standalone
    menu.addItem("Power off / Exit", true, false, [this]() { processor_.requestShutdown(); });
#else
    menu.addItem("Power off / Exit", false, false, nullptr);
#endif
    return menu;
  }));
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
    auto labelArea = knobArea.expanded(28.0f * scale, 0).withY(knobArea.getBottom() + 6.0f * scale).withHeight(22.0f * scale);
    target.label.setBounds(labelArea.toNearestInt());
    target.helper.setBounds(labelArea.translated(0, static_cast<float>(labelArea.getHeight())).toNearestInt());
  };

  placeKnob(seedKnob_, 150.0f, 170.0f);
  placeKnob(densityKnob_, 280.0f, 170.0f);
  placeKnob(toneKnob_, 440.0f, 170.0f);
  placeKnob(fxKnob_, 570.0f, 170.0f);

  auto placeButton = [&](juce::TextButton& btn, float baseX, float baseY) {
    const juce::Rectangle<float> area(viewOrigin.x + baseX * scale, viewOrigin.y + baseY * scale, 100.0f * scale,
                                      40.0f * scale);
    btn.setBounds(area.toNearestInt());
  };

  placeButton(tapButton_, 100.0f, 250.0f);
  placeButton(shiftButton_, 240.0f, 250.0f);
  placeButton(altButton_, 380.0f, 250.0f);
  placeButton(captureButton_, 520.0f, 250.0f);

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

  const juce::Rectangle<float> oledArea(viewOrigin.x + 290.0f * scale, viewOrigin.y + 65.0f * scale,
                                        160.0f * scale, 65.0f * scale);
  oledLabel_.setBounds(oledArea.toNearestInt());
  clockStatusLabel_.setBounds(juce::Rectangle<float>(
      viewOrigin.x + 80.0f * scale, viewOrigin.y + 315.0f * scale,
      560.0f * scale, 18.0f * scale).toNearestInt());
  const juce::Rectangle<int> engineLine(
      static_cast<int>(viewOrigin.x + 490.0f * scale), static_cast<int>(viewOrigin.y + 80.0f * scale),
      static_cast<int>(160.0f * scale), static_cast<int>(18.0f * scale));
  engineNameLabel_.setBounds(engineLine);
  engineHintLabel_.setBounds(engineLine.translated(0, 18));
}

void SeedboxPanelView::refresh(bool displayDirty) {
  const auto& app = processor_.readThreadApp();
  if (displayDirty || cachedOledText_.isEmpty()) {
    const auto& snapshot = app.displayCache();
    juce::String display;
    display << snapshot.title << "\n" << snapshot.status << "\n" << snapshot.metrics << "\n" << snapshot.nuance;
    cachedOledText_ = display;
    oledLabel_.setText(cachedOledText_, juce::dontSendNotification);
  }

  AppState::LearnFrame learn{};
  app.captureLearnFrame(learn);
  const bool waiting = app.waitingForExternalClock();
  juce::String clockMode = "INTERNAL";
  if (waiting) {
    clockMode = "WAITING";
  } else if (processor_.followHostTransportEnabled()) {
    clockMode = "HOST";
  } else if (app.followExternalClockEnabled()) {
    clockMode = "MIDI";
  }

  auto toDb = [](float value) {
    constexpr float kFloor = 1e-6f;
    return 20.0f * std::log10(std::max(value, kFloor));
  };
  const float rmsDb = toDb(learn.audio.combinedRms);
  const float peakDb = toDb(learn.audio.combinedPeak);
  juce::String meter = "OUT " + juce::String(rmsDb, 1) + "dB/" + juce::String(peakDb, 1) + "dB";
  const auto diagnostics = app.diagnosticsSnapshot();
  const auto& host = diagnostics.host;
  juce::String clockStatus = "CLK " + clockMode + " | " + meter;
  if (host.midiDroppedCount > 0u || host.oversizeBlockDropCount > 0u) {
    clockStatus << " | WARN " << juce::String(static_cast<int>(host.midiDroppedCount)) << "/"
                << juce::String(static_cast<int>(host.oversizeBlockDropCount));
  }
  clockStatusLabel_.setText(clockStatus, juce::dontSendNotification);

  updateEngineLabel();
  applySensitivity();
}

void SeedboxPanelView::setAudioManager(juce::AudioDeviceManager* audioManager) { audioManager_ = audioManager; }

void SeedboxPanelView::setModifierStates(bool toneHeld, bool shiftHeld, bool altHeld) {
  setToneHeld(toneHeld, true);
  setShiftHeld(shiftHeld, true);
  setAltHeld(altHeld, true);
}

void SeedboxPanelView::syncKeyboardModifiers(bool toneHeld, bool shiftHeld, bool altHeld) {
  toneHeldByKeyboard_ = toneHeld;
  shiftHeldByKeyboard_ = shiftHeld;
  altHeldByKeyboard_ = altHeld;

  shiftButton_.setToggleState(shiftActive(), juce::dontSendNotification);
  altButton_.setToggleState(altActive(), juce::dontSendNotification);

  applySensitivity();
}

void SeedboxPanelView::setShiftHeld(bool held, bool keyboard) {
  shiftHeldByKeyboard_ = keyboard ? held : shiftHeldByKeyboard_;
  shiftHeldByButton_ = keyboard ? shiftHeldByButton_ : held;
  shiftButton_.setToggleState(shiftActive(), juce::dontSendNotification);
  setPanelButton(hal::Board::ButtonID::Shift, shiftActive());
  applySensitivity();
}

void SeedboxPanelView::setAltHeld(bool held, bool keyboard) {
  altHeldByKeyboard_ = keyboard ? held : altHeldByKeyboard_;
  altHeldByButton_ = keyboard ? altHeldByButton_ : held;
  altButton_.setToggleState(altActive(), juce::dontSendNotification);
  setPanelButton(hal::Board::ButtonID::AltSeed, altActive());
  applySensitivity();
}

void SeedboxPanelView::setToneHeld(bool held, bool keyboard) {
  toneHeldByKeyboard_ = keyboard ? held : toneHeldByKeyboard_;
  toneHeldByButton_ = keyboard ? toneHeldByButton_ : held;
  setPanelButton(hal::Board::ButtonID::EncoderToneTilt, toneActive());
  applySensitivity();
}

void SeedboxPanelView::applySensitivity() {
  toneKnob_.label.setColour(juce::Label::textColourId, toneActive() ? accentColour() : juce::Colours::whitesmoke);
}

void SeedboxPanelView::setPanelButton(hal::Board::ButtonID id, bool pressed) {
  hal::nativeBoardSetButton(id, pressed);
  // Sample each edge now so quick clicks cannot disappear between timer ticks.
  processor_.controlThreadApp().serviceMaintenance();
}

void SeedboxPanelView::updateEngineLabel() {
  const auto& app = processor_.readThreadApp();
  const auto& seeds = app.seeds();
  const int engineId = static_cast<int>(seeds.empty() ? 0 : seeds[app.focusSeed()].engine);
  engineLabel_ = engineName(engineId);
  engineNameLabel_.setText("Mode: " + engineLabel_, juce::dontSendNotification);
  engineHintLabel_.setText(engineHint(engineId), juce::dontSendNotification);
}

void SeedboxPanelView::nudgeActiveControl(double delta) {
  juce::Slider* target = lastActive_;
  if (target == nullptr) {
    target = &seedKnob_.knob;
  }
  const double step = target->getInterval() > 0.0 ? target->getInterval() : 0.1;
  if (delta != 0.0)
    target->setValue(target->getValue() + (delta > 0.0 ? step : -step), juce::sendNotificationSync);
}

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
