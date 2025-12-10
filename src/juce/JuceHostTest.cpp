#if SEEDBOX_JUCE

#include <juce_core/juce_core.h>

#include <vector>

#include "hal/hal_audio.h"
#include "hal/Board.h"
#include "juce/JuceHost.h"

namespace {
class JuceHostSanityTest : public juce::UnitTest {
 public:
  JuceHostSanityTest() : juce::UnitTest("JuceHostSanity", "seedbox") {}

  void runTest() override {
    beginTest("honours host sample rate and block size");
    AppState app(hal::board());
    seedbox::juce_bridge::JuceHost host(app);
    host.configureForTests(48000.0, 256);
    expectWithinAbsoluteError(hal::audio::sampleRate(), 48000.0f, 0.001f);
    expectEquals(static_cast<int>(hal::audio::framesPerBlock()), 256);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    hal::audio::renderHostBuffer(left.data(), right.data(), left.size());
    expectEquals(static_cast<int>(hal::audio::sampleClock()), 256);
  }
};

JuceHostSanityTest juceHostSanityTest;
}  // namespace

#endif  // SEEDBOX_JUCE
