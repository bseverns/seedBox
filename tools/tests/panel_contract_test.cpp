// Offscreen check of the actual JUCE component tree and mouse-to-Board routing.
#include "juce/ui/SeedboxPanelView.h"
#include "hal/PanelControls.h"
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace seedbox::juce_bridge;
namespace {
void require(bool value, const std::string& message) {
  if (!value) throw std::runtime_error(message);
}
void edge(juce::Component& component, bool down, bool right) {
  const auto now = juce::Time::getCurrentTime();
  const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(), {10, 10},
      juce::ModifierKeys(right ? juce::ModifierKeys::rightButtonModifier : juce::ModifierKeys::leftButtonModifier),
      1, 0, 0, 0, 0, &component, &component, now, {10, 10}, now, 1, false);
  if (down) component.mouseDown(event); else component.mouseUp(event);
}
void checkSwitch(juce::Component& component, hal::Board::ButtonID id, bool right) {
  for (const bool down : {true, false}) {
    edge(component, down, right);
    for (std::size_t i = 0; i < hal::panel::buttonCount; ++i) {
      const auto other = static_cast<hal::Board::ButtonID>(i);
      require(hal::nativeBoard().sampleButton(other).pressed == (down && other == id),
              "JUCE switch missing or cross-wired: " + std::to_string(static_cast<unsigned>(id)));
    }
  }
}
}  // namespace

int main() {
  juce::ScopedJuceInitialiser_GUI gui;
  try {
    hal::nativeBoardReset();
    SeedboxAudioProcessor processor;
    processor.prepareToPlay(48000, 256);  // No audio device is opened.
    SeedboxPanelView panel(processor);
    panel.setSize(1000, 722);
    std::vector<juce::Slider*> knobs;
    std::vector<juce::Button*> buttons;
    const auto& children = panel.getChildren();
    for (int i = 0; i < children.size(); ++i) {
      if (auto* knob = dynamic_cast<juce::Slider*>(children[i])) {
        const auto index = knobs.size();
        require(index < hal::panel::encoders.size(), "Extra JUCE encoder");
        // The visible label immediately follows its knob in LabeledKnob setup.
        auto* label = i + 1 < children.size() ? dynamic_cast<juce::Label*>(children[i + 1]) : nullptr;
        require(label && label->getText() == hal::panel::encoders[index].label,
                "JUCE encoder label mismatch at " + std::to_string(index));
        knobs.push_back(knob);
      }
      if (auto* button = dynamic_cast<juce::Button*>(children[i])) buttons.push_back(button);
    }
    require(knobs.size() == hal::panel::encoders.size(), "JUCE encoder count mismatch");
    require(buttons.size() == hal::panel::buttons.size(), "JUCE standalone button count mismatch");
    for (std::size_t i = 0; i < knobs.size(); ++i)
      checkSwitch(*knobs[i], hal::panel::encoders[i].button_id, true);
    for (std::size_t i = 0; i < buttons.size(); ++i) {
      require(buttons[i]->getButtonText() == hal::panel::buttons[i].label,
              "JUCE standalone label mismatch at " + std::to_string(i));
      checkSwitch(*buttons[i], hal::panel::buttons[i].id, false);
    }
    processor.controlThreadApp().setMode(AppState::Mode::SEEDS);
    const auto focus = processor.readThreadApp().focusSeed();
    knobs[0]->setValue(1, juce::sendNotificationSync);
    require(processor.readThreadApp().focusSeed() != focus, "Seed encoder turn did not reach the application");
    processor.releaseResources();
    std::cout << "JUCE panel contract OK: labels, counts, eight switch routes and Seed turn\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "JUCE panel contract failed: " << error.what() << '\n';
    return 1;
  }
}
