#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <thread>

class HeadlessLoop {
 public:
  void onTick(std::function<void(int)> cb) { callback_ = std::move(cb); }

  void run(int cycles, int bpm) const {
    const bool quietMode = true;
    auto tickLength = std::chrono::milliseconds(60000 / (bpm * 4));
    std::cout << "[headless] quiet-mode=" << quietMode << ", bpm=" << bpm << "\n";

    for (int i = 0; i < cycles; ++i) {
      std::this_thread::sleep_for(quietMode ? tickLength / 8 : tickLength);
      if (callback_) {
        callback_(i);
      }
    }
    std::cout << "[headless] loop exited without spinning up audio IO.\n";
  }

 private:
  std::function<void(int)> callback_;
};

class GhostAutomation {
 public:
  void addLane(std::string name, int mod) { lanes_[std::move(name)] = mod; }

  void tick(int frame) {
    for (const auto &lane : lanes_) {
      if (frame % lane.second == 0) {
        std::cout << "  lane " << lane.first << " -> silent poke (frame " << frame << ")\n";
      }
    }
  }

 private:
  std::map<std::string, int> lanes_;
};

int main() {
  HeadlessLoop loop;
  GhostAutomation automation;
  automation.addLane("filter", 6);
  automation.addLane("delay", 9);
  automation.addLane("vca", 4);

  loop.onTick([&](int frame) { automation.tick(frame); });
  loop.run(/*cycles=*/24, /*bpm=*/72);

  std::cout << "TODO: render automation curves into /out/headless-automation.wav when the mix bus is ready.\n";
  return 0;
}
