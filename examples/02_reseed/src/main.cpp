#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

struct Stem {
  std::string name;
  int lane;
};

class QuietGarden {
 public:
  explicit QuietGarden(unsigned int seed) : rng_(seed) {}

  void reseed(unsigned int seed) {
    rng_.seed(seed);
    std::cout << "[reseed] swapped seed -> " << seed << " (still quiet)\n";
  }

  void plant(std::string name, int lane) { stems_.push_back({std::move(name), lane}); }

  void audition(int beats, int bpm) {
    const bool quietMode = true;
    std::shuffle(stems_.begin(), stems_.end(), rng_);
    auto beatSpan = std::chrono::milliseconds(60000 / bpm);
    std::cout << "[reseed] auditioning " << stems_.size() << " stems @" << bpm << " BPM\n";

    for (int beat = 0; beat < beats; ++beat) {
      std::this_thread::sleep_for(quietMode ? beatSpan / 12 : beatSpan);
      const Stem &stem = stems_[beat % stems_.size()];
      std::cout << "  lane " << stem.lane << ": " << stem.name << " (ghost trigger)\n";
    }
    std::cout << "[reseed] zero audio buffers touched.\n";
  }

 private:
  std::mt19937 rng_;
  std::vector<Stem> stems_;
};

int main() {
  QuietGarden garden(0xCAFE);
  garden.plant("kick compost", 0);
  garden.plant("snare clipping", 1);
  garden.plant("ride oxidation", 2);

  garden.audition(/*beats=*/8, /*bpm=*/124);
  garden.reseed(0xBEEF);
  garden.audition(/*beats=*/6, /*bpm=*/124);

  std::cout << "TODO: record both seeds to /out/reseed-A.wav and /out/reseed-B.wav when the bus is live.\n";
  return 0;
}
