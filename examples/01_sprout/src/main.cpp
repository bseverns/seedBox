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
  void setEvents(std::vector<QuietEvent> events) { events_ = std::move(events); }

  void run(int measures, int bpm, bool quietMode) const {
    const auto beatDuration = std::chrono::milliseconds(60000 / bpm);
    std::cout << "[sprout] quiet-mode=" << std::boolalpha << quietMode << "\n";

    const auto waitDuration = quietMode ? beatDuration / 10 : beatDuration;
    for (int beat = 0; beat < measures * 4; ++beat) {
      std::this_thread::sleep_for(waitDuration);
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

struct SproutOptions {
  bool quietMode = true;
  std::string mutation = "default";
  bool showHelp = false;
  bool listMutations = false;
};

SproutOptions parseArgs(int argc, char **argv) {
  SproutOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--quiet") {
      options.quietMode = true;
    } else if (arg == "--loud" || arg == "--no-quiet") {
      options.quietMode = false;
    } else if (arg.rfind("--mutate=", 0) == 0) {
      options.mutation = arg.substr(std::string("--mutate=").size());
    } else if (arg == "--list-mutations") {
      options.listMutations = true;
    } else if (arg == "--help" || arg == "-h") {
      options.showHelp = true;
    } else {
      std::cerr << "[sprout] unknown flag: " << arg << "\n";
      options.showHelp = true;
    }
  }
  return options;
}

void printHelp() {
  std::cout << "sprout controls:\n"
            << "  --quiet          keep the sim sped-up (default)\n"
            << "  --loud           stretch beats to real-time 4/4\n"
            << "  --mutate=<name>  swap in a different ghost kit\n"
            << "  --list-mutations show the known kit options\n"
            << std::endl;
}

using MutationTable = std::vector<std::pair<std::string, std::vector<QuietEvent>>>;

const MutationTable &mutations() {
  static const MutationTable table = {
      {"default",
       {{"kick placeholder", 0}, {"snare scribble", 2}, {"hat rustle", 3}}},
      {"hatless",
       {{"kick placeholder", 0}, {"snare scribble", 2}}},
      {"afterbeat-chop",
       {{"kick placeholder", 0}, {"snare scribble", 3}, {"ghost clap", 1}}},
  };
  return table;
}

void listMutations() {
  std::cout << "available ghost kits:\n";
  for (const auto &entry : mutations()) {
    std::cout << "  - " << entry.first << '\n';
  }
  std::cout << std::endl;
}

std::vector<QuietEvent> resolveMutation(const std::string &mutationName) {
  for (const auto &entry : mutations()) {
    if (entry.first == mutationName) {
      return entry.second;
    }
  }
  std::cerr << "[sprout] missing mutation '" << mutationName
            << "', sliding back to default." << std::endl;
  return mutations().front().second;
}

int main(int argc, char **argv) {
  const auto options = parseArgs(argc, argv);

  if (options.showHelp) {
    printHelp();
  }
  if (options.listMutations) {
    listMutations();
  }
  if (options.showHelp || options.listMutations) {
    return 0;
  }

  QuietSequencer sequencer;
  sequencer.setEvents(resolveMutation(options.mutation));

  std::cout << "[sprout] mutation=" << options.mutation << '\n';
  sequencer.run(/*measures=*/2, /*bpm=*/96, options.quietMode);
  std::cout << "TODO: bounce these ghosts into /out/ once the render rig lands." << std::endl;
  return 0;
}
