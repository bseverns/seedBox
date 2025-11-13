#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "util/ScaleQuantizer.h"

namespace {

enum class SnapMode { kNearest, kUp, kDown };

struct Options {
  util::ScaleQuantizer::Scale scale = util::ScaleQuantizer::Scale::kMajor;
  std::uint8_t root = 0;
  SnapMode mode = SnapMode::kNearest;
  std::vector<float> offsets{-5.5f, -1.2f, 0.3f, 2.6f, 7.8f};
  bool showHelp = false;
};

const std::map<std::string, util::ScaleQuantizer::Scale>& scaleLookup() {
  static const std::map<std::string, util::ScaleQuantizer::Scale> table = {
      {"chromatic", util::ScaleQuantizer::Scale::kChromatic},
      {"major", util::ScaleQuantizer::Scale::kMajor},
      {"minor", util::ScaleQuantizer::Scale::kMinor},
      {"penta-major", util::ScaleQuantizer::Scale::kPentatonicMajor},
      {"penta-minor", util::ScaleQuantizer::Scale::kPentatonicMinor},
  };
  return table;
}

const char* toString(util::ScaleQuantizer::Scale scale) {
  switch (scale) {
    case util::ScaleQuantizer::Scale::kChromatic:
      return "chromatic";
    case util::ScaleQuantizer::Scale::kMajor:
      return "major";
    case util::ScaleQuantizer::Scale::kMinor:
      return "minor";
    case util::ScaleQuantizer::Scale::kPentatonicMajor:
      return "penta-major";
    case util::ScaleQuantizer::Scale::kPentatonicMinor:
      return "penta-minor";
  }
  return "chromatic";
}

const char* toString(SnapMode mode) {
  switch (mode) {
    case SnapMode::kNearest:
      return "nearest";
    case SnapMode::kUp:
      return "up";
    case SnapMode::kDown:
      return "down";
  }
  return "nearest";
}

SnapMode parseMode(const std::string& value) {
  if (value == "nearest") {
    return SnapMode::kNearest;
  }
  if (value == "up") {
    return SnapMode::kUp;
  }
  if (value == "down") {
    return SnapMode::kDown;
  }
  throw std::invalid_argument("unknown mode: " + value);
}

std::vector<float> parseOffsets(const std::string& csv) {
  std::vector<float> result;
  std::stringstream ss(csv);
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (token.empty()) {
      continue;
    }
    result.push_back(std::stof(token));
  }
  if (result.empty()) {
    result.push_back(0.0f);
  }
  return result;
}

Options parseArgs(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      options.showHelp = true;
      break;
    }
    if (arg.rfind("--scale=", 0) == 0) {
      const std::string value = arg.substr(std::string("--scale=").size());
      const auto it = scaleLookup().find(value);
      if (it == scaleLookup().end()) {
        throw std::invalid_argument("unknown scale: " + value);
      }
      options.scale = it->second;
    } else if (arg.rfind("--root=", 0) == 0) {
      const std::string value = arg.substr(std::string("--root=").size());
      const int parsed = std::stoi(value);
      options.root = static_cast<std::uint8_t>(parsed);
    } else if (arg.rfind("--mode=", 0) == 0) {
      const std::string value = arg.substr(std::string("--mode=").size());
      options.mode = parseMode(value);
    } else if (arg.rfind("--offsets=", 0) == 0) {
      const std::string value = arg.substr(std::string("--offsets=").size());
      options.offsets = parseOffsets(value);
    } else {
      throw std::invalid_argument("unknown flag: " + arg);
    }
  }
  return options;
}

void printUsage() {
  std::cout << "scale quantizer harness" << std::endl;
  std::cout << "Usage:\n  program --scale=<chromatic|major|minor|penta-major|penta-minor>\\\n"
            << "          --root=<0-11> --mode=<nearest|up|down>\\\n"
            << "          --offsets=<comma-separated floats>" << std::endl;
}

class QuantizeHarness {
 public:
  void setScale(util::ScaleQuantizer::Scale scale) { scale_ = scale; }
  void setRoot(std::uint8_t root) { root_ = static_cast<std::uint8_t>(root % 12); }
  void setMode(SnapMode mode) { mode_ = mode; }

  float quantize(float pitch, SnapMode mode) const {
    switch (mode) {
      case SnapMode::kNearest:
        return util::ScaleQuantizer::SnapToScale(pitch, root_, scale_);
      case SnapMode::kUp:
        return util::ScaleQuantizer::SnapUp(pitch, root_, scale_);
      case SnapMode::kDown:
        return util::ScaleQuantizer::SnapDown(pitch, root_, scale_);
    }
    return pitch;
  }

  void narrate(const std::vector<float>& offsets) const {
    std::cout << "[scale-quantizer] scale=" << toString(scale_)
              << " root=" << static_cast<int>(root_)
              << " mode=" << toString(mode_) << '\n';
    std::cout << "  offsets: ";
    for (std::size_t i = 0; i < offsets.size(); ++i) {
      if (i > 0) {
        std::cout << ", ";
      }
      std::cout << std::fixed << std::setprecision(2) << offsets[i];
    }
    std::cout << '\n';

    std::cout << "\n  table:" << std::endl;
    std::cout << "    pitch    nearest     up     down" << std::endl;
    for (float pitch : offsets) {
      const float nearest = quantize(pitch, SnapMode::kNearest);
      const float up = quantize(pitch, SnapMode::kUp);
      const float down = quantize(pitch, SnapMode::kDown);
      const float chosen = quantize(pitch, mode_);
      std::cout << std::setw(10) << std::fixed << std::setprecision(2) << pitch << " "
                << std::setw(9) << nearest << " "
                << std::setw(7) << up << " "
                << std::setw(8) << down;
      if (chosen == (mode_ == SnapMode::kNearest ? nearest
                                                 : mode_ == SnapMode::kUp ? up : down)) {
        std::cout << "  <-- active";
      }
      std::cout << '\n';
    }
    std::cout << std::endl;
  }

 private:
  util::ScaleQuantizer::Scale scale_ = util::ScaleQuantizer::Scale::kMajor;
  std::uint8_t root_ = 0;
  SnapMode mode_ = SnapMode::kNearest;
};

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = parseArgs(argc, argv);
    if (options.showHelp) {
      printUsage();
      return 0;
    }

    QuantizeHarness harness;
    harness.setScale(options.scale);
    harness.setRoot(options.root);
    harness.setMode(options.mode);

    harness.narrate(options.offsets);
  } catch (const std::exception& ex) {
    std::cerr << "[scale-quantizer] " << ex.what() << '\n';
    printUsage();
    return 1;
  }
  return 0;
}
