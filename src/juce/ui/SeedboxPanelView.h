#pragma once

#if SEEDBOX_JUCE

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <optional>

namespace juce {
class AudioDeviceManager;
}

#include "juce/SeedboxAudioProcessor.h"

namespace seedbox::juce_bridge {

class SeedboxPanelView : public juce::Component {
 public:
  explicit SeedboxPanelView(SeedboxAudioProcessor& processor, juce::AudioDeviceManager* audioManager = nullptr);
  ~SeedboxPanelView() override;

  void paint(juce::Graphics& g) override;
  void resized() override;

  void refresh();
  void setAudioManager(juce::AudioDeviceManager* audioManager);
  void setModifierStates(bool toneHeld, bool shiftHeld, bool altHeld);
  void syncKeyboardModifiers(bool toneHeld, bool shiftHeld, bool altHeld);
  void nudgeActiveControl(double delta);

 private:
  class PanelLookAndFeel : public juce::LookAndFeel_V4 {
   public:
    PanelLookAndFeel();
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
  };

  class PanelKnob : public juce::Slider {
   public:
    PanelKnob();
    std::function<void()> onPress;

   private:
    void mouseUp(const juce::MouseEvent& event) override;
  };

  class PanelButton : public juce::TextButton {
   public:
    using juce::TextButton::TextButton;
    std::function<void(const juce::MouseEvent&)> onDown;
    std::function<void(const juce::MouseEvent&)> onUp;

   private:
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
  };

  class JackIcon : public juce::Component {
   public:
    using MenuBuilder = std::function<juce::PopupMenu()>;

    JackIcon(juce::String name, MenuBuilder menuBuilder, std::function<void()> onClick = nullptr);
    void paint(juce::Graphics& g) override;

   private:
    void mouseUp(const juce::MouseEvent& event) override;
    juce::String label_;
    MenuBuilder menuBuilder_{};
    std::function<void()> onClick_{};
  };

  struct LabeledKnob {
    PanelKnob knob;
    juce::Label label;
    juce::Label helper;
  };

  void layoutControls();
  void paintPanel(juce::Graphics& g);
  void updateEngineLabel();
  void setShiftHeld(bool held, bool keyboard);
  void setAltHeld(bool held, bool keyboard);
  void setToneHeld(bool held, bool keyboard);
  bool shiftActive() const { return shiftHeldByButton_ || shiftHeldByKeyboard_; }
  bool altActive() const { return altHeldByButton_ || altHeldByKeyboard_; }
  bool toneActive() const { return toneHeldByButton_ || toneHeldByKeyboard_; }
  void applySensitivity();
  void handleTap(bool longPress);
  void handleReseed();
  void handleLock();
  void saveQuickPreset();
  void recallQuickPreset();
  void updateLockIndicator();

  SeedboxAudioProcessor& processor_;
  PanelLookAndFeel lookAndFeel_;

  LabeledKnob seedKnob_;
  LabeledKnob densityKnob_;
  LabeledKnob toneKnob_;
  LabeledKnob fxKnob_;

  PanelButton tapButton_;
  PanelButton shiftButton_;
  PanelButton altButton_;
  PanelButton reseedButton_;
  PanelButton lockButton_;

  juce::OwnedArray<JackIcon> jackIcons_;
  juce::Label oledLabel_;
  juce::Label clockStatusLabel_;

  juce::String engineLabel_;
  juce::Label engineNameLabel_;

  juce::Rectangle<float> viewBounds_{};
  juce::Rectangle<float> panelBounds_{};

  bool shiftHeldByButton_{false};
  bool altHeldByButton_{false};
  bool toneHeldByButton_{false};

  bool shiftHeldByKeyboard_{false};
  bool altHeldByKeyboard_{false};
  bool toneHeldByKeyboard_{false};

  juce::Slider* lastActive_{nullptr};
  double lastTapMs_{0.0};
  juce::AudioDeviceManager* audioManager_{nullptr};
};

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
