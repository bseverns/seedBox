#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

struct QuietEvent {
  std::string label;
  int beat;
};

class QuietSequencer {
 public:
  void addEvent(std::string label, int beat) { events_.push_back({std::move(label), beat}); }

  void run(int measures, int bpm) const {
    const bool quietMode = true;
    const auto beatDuration = std::chrono::milliseconds(60000 / bpm);
    std::cout << "[sprout] quiet-mode=" << std::boolalpha << quietMode << "\n";

    for (int beat = 0; beat < measures * 4; ++beat) {
      std::this_thread::sleep_for(quietMode ? beatDuration / 10 : beatDuration);
      for (const auto &event : events_) {
        if (event.beat == beat % 4) {
          std::cout << "  • ghosting " << event.label << " @beat " << beat << "\n";
        }
      }
    }
    std::cout << "[sprout] finished without touching the DAC." << std::endl;
  }

 private:
  std::vector<QuietEvent> events_;
};

int main() {
  QuietSequencer sequencer;
  sequencer.addEvent("kick placeholder", 0);
  sequencer.addEvent("snare scribble", 2);
  sequencer.addEvent("hat rustle", 3);

  sequencer.run(/*measures=*/2, /*bpm=*/96);
  std::cout << "TODO: bounce these ghosts into /out/ once the render rig lands." << std::endl;
  return 0;
}
