#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "util/ScaleQuantizer.h"
#include "util/ScaleQuantizerFlow.h"

namespace {

constexpr float kDefaultDriftDepth = 0.45f;
constexpr std::size_t kDefaultDriftFrames = 17;  // gives us a full sine cycle

struct Options {
  util::ScaleQuantizer::Scale scale = util::ScaleQuantizer::Scale::kMajor;
  std::uint8_t root = 0;
  util::QuantizerMode mode = util::QuantizerMode::kNearest;
  std::vector<float> offsets{-5.5f, -1.2f, 0.3f, 2.6f, 7.8f};
  bool showHelp = false;
  bool exportCsv = false;
  std::filesystem::path csvPath{"out/scale_quantizer.csv"};
  double driftHz = 0.0;
  std::optional<std::string> oscEndpoint;
  std::optional<std::string> websocketUrl;
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

util::QuantizerMode parseMode(const std::string& value) {
  if (value == "nearest") {
    return util::QuantizerMode::kNearest;
  }
  if (value == "up") {
    return util::QuantizerMode::kUp;
  }
  if (value == "down") {
    return util::QuantizerMode::kDown;
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

std::filesystem::path normalizeOutPath(const std::string& requested) {
  std::filesystem::path path = requested.empty() ? std::filesystem::path{"scale_quantizer.csv"}
                                                : std::filesystem::path{requested};
  if (path.is_absolute()) {
    throw std::invalid_argument("csv path must be relative to the repo (keep it in out/)");
  }
  if (path.empty() || path.begin() == path.end() || *path.begin() != std::filesystem::path{"out"}) {
    path = std::filesystem::path("out") / path;
  }
  return path.lexically_normal();
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
    } else if (arg == "--export-csv") {
      options.exportCsv = true;
      options.csvPath = normalizeOutPath("scale_quantizer.csv");
    } else if (arg.rfind("--export-csv=", 0) == 0) {
      options.exportCsv = true;
      const std::string value = arg.substr(std::string("--export-csv=").size());
      options.csvPath = normalizeOutPath(value);
    } else if (arg.rfind("--drift=", 0) == 0) {
      const std::string value = arg.substr(std::string("--drift=").size());
      options.driftHz = std::stod(value);
      if (options.driftHz < 0.0) {
        throw std::invalid_argument("drift must be >= 0");
      }
    } else if (arg.rfind("--osc=", 0) == 0) {
      options.oscEndpoint = arg.substr(std::string("--osc=").size());
    } else if (arg.rfind("--ws=", 0) == 0) {
      options.websocketUrl = arg.substr(std::string("--ws=").size());
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
            << "          --offsets=<comma-separated floats> [--drift=<Hz>]\\\n"
            << "          [--export-csv[=out/<file>]] [--osc=host:port] [--ws=ws://host:port/path]"
            << std::endl;
}

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

bool isInvalid(SocketHandle socket) {
#if defined(_WIN32)
  return socket == INVALID_SOCKET;
#else
  return socket < 0;
#endif
}

class OscClient {
 public:
  explicit OscClient(const std::string& endpoint) {
    const auto pos = endpoint.find(':');
    if (pos == std::string::npos) {
      throw std::invalid_argument("OSC endpoint must look like host:port");
    }
    const std::string host = endpoint.substr(0, pos);
    const std::string port = endpoint.substr(pos + 1);

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo* result = nullptr;
    const int rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
    if (rc != 0 || result == nullptr) {
      throw std::runtime_error("failed to resolve OSC endpoint");
    }

    socket_ = ::socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (isInvalid(socket_)) {
      freeaddrinfo(result);
      throw std::runtime_error("failed to open OSC socket");
    }

    std::memcpy(&addr_, result->ai_addr, result->ai_addrlen);
    addrLen_ = static_cast<socklen_t>(result->ai_addrlen);
    freeaddrinfo(result);
  }

  ~OscClient() {
    if (!isInvalid(socket_)) {
#if defined(_WIN32)
      closesocket(socket_);
#else
      ::close(socket_);
#endif
    }
  }

  void send(const util::QuantizerSample& sample, util::QuantizerMode mode) const {
    std::vector<std::uint8_t> message;
    appendOscString(message, "/quantizer/sample");
    appendOscString(message, ",ifffffffs");
    appendOscInt(message, static_cast<std::int32_t>(sample.slot));
    appendOscFloat(message, static_cast<float>(sample.timeSeconds));
    appendOscFloat(message, sample.inputPitch);
    appendOscFloat(message, sample.driftedPitch);
    appendOscFloat(message, sample.snappedNearest);
    appendOscFloat(message, sample.snappedUp);
    appendOscFloat(message, sample.snappedDown);
    appendOscFloat(message, sample.activePitch);
    appendOscString(message, util::ToString(mode));

    ::sendto(socket_, reinterpret_cast<const char*>(message.data()),
             static_cast<int>(message.size()), 0,
             reinterpret_cast<const struct sockaddr*>(&addr_), addrLen_);
  }

 private:
  static void appendOscString(std::vector<std::uint8_t>& buffer, const std::string& value) {
    buffer.insert(buffer.end(), value.begin(), value.end());
    buffer.push_back('\0');
    while (buffer.size() % 4 != 0) {
      buffer.push_back('\0');
    }
  }

  static void appendOscInt(std::vector<std::uint8_t>& buffer, std::int32_t value) {
    const std::uint32_t network = htonl(static_cast<std::uint32_t>(value));
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&network);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(network));
  }

  static void appendOscFloat(std::vector<std::uint8_t>& buffer, float value) {
    static_assert(sizeof(float) == sizeof(std::uint32_t), "float size unexpected");
    std::uint32_t raw;
    std::memcpy(&raw, &value, sizeof(float));
    raw = htonl(raw);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&raw);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(raw));
  }

  SocketHandle socket_ = kInvalidSocket;
  struct sockaddr_storage addr_{};
  socklen_t addrLen_ = 0;
};

class WebSocketClient {
 public:
  explicit WebSocketClient(const std::string& url) { connect(url); }

  ~WebSocketClient() { closeSocket(); }

  void send(const std::string& payload) {
    if (isInvalid(socket_)) {
      return;
    }
    const std::uint8_t opcodeText = 0x81;
    std::vector<std::uint8_t> frame;
    frame.push_back(opcodeText);

    const std::size_t len = payload.size();
    if (len <= 125) {
      frame.push_back(static_cast<std::uint8_t>(0x80 | len));
    } else if (len <= 65535) {
      frame.push_back(0x80 | 126);
      const std::uint16_t nlen = htons(static_cast<std::uint16_t>(len));
      const auto* lenBytes = reinterpret_cast<const std::uint8_t*>(&nlen);
      frame.insert(frame.end(), lenBytes, lenBytes + sizeof(nlen));
    } else {
      frame.push_back(0x80 | 127);
      appendLength64(frame, static_cast<std::uint64_t>(len));
    }

    std::array<std::uint8_t, 4> mask = newMask();
    frame.insert(frame.end(), mask.begin(), mask.end());
    for (std::size_t i = 0; i < len; ++i) {
      frame.push_back(static_cast<std::uint8_t>(payload[i] ^ mask[i % 4]));
    }

    ::send(socket_, reinterpret_cast<const char*>(frame.data()),
           static_cast<int>(frame.size()), 0);
  }

 private:
  struct UrlParts {
    std::string host;
    std::string port;
    std::string path;
  };

  static UrlParts parse(const std::string& url) {
    const std::string prefix = "ws://";
    if (url.rfind(prefix, 0) != 0) {
      throw std::invalid_argument("websocket url must start with ws://");
    }
    std::string rest = url.substr(prefix.size());
    std::string path = "/";
    const auto slash = rest.find('/');
    if (slash != std::string::npos) {
      path = rest.substr(slash);
      rest = rest.substr(0, slash);
      if (path.empty()) {
        path = "/";
      }
    }
    std::string host = rest;
    std::string port = "80";
    const auto colon = rest.find(':');
    if (colon != std::string::npos) {
      host = rest.substr(0, colon);
      port = rest.substr(colon + 1);
    }
    return UrlParts{host, port, path};
  }

  void connect(const std::string& url) {
    UrlParts parts = parse(url);

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = nullptr;
    const int rc = getaddrinfo(parts.host.c_str(), parts.port.c_str(), &hints, &result);
    if (rc != 0 || result == nullptr) {
      throw std::runtime_error("failed to resolve websocket host");
    }

    socket_ = ::socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (isInvalid(socket_)) {
      freeaddrinfo(result);
      throw std::runtime_error("failed to open websocket socket");
    }

    if (::connect(socket_, result->ai_addr, result->ai_addrlen) < 0) {
      freeaddrinfo(result);
      closeSocket();
      throw std::runtime_error("failed to connect websocket");
    }
    freeaddrinfo(result);

    const std::string handshake =
        "GET " + parts.path + " HTTP/1.1\r\n" +
        "Host: " + parts.host + ":" + parts.port + "\r\n" +
        "Upgrade: websocket\r\n" +
        "Connection: Upgrade\r\n" +
        "Sec-WebSocket-Key: ZmFrZUtleUZvckFydA==\r\n" +
        "Sec-WebSocket-Version: 13\r\n\r\n";

    ::send(socket_, handshake.c_str(), static_cast<int>(handshake.size()), 0);

    std::array<char, 512> buffer{};
    const int bytes = ::recv(socket_, buffer.data(), static_cast<int>(buffer.size()), 0);
    if (bytes <= 0) {
      closeSocket();
      throw std::runtime_error("websocket handshake failed");
    }
    const std::string response(buffer.data(), buffer.data() + bytes);
    if (response.find(" 101 ") == std::string::npos) {
      closeSocket();
      throw std::runtime_error("websocket server rejected handshake");
    }
  }

  static std::array<std::uint8_t, 4> newMask() {
    std::array<std::uint8_t, 4> mask{};
    std::random_device rd;
    for (std::size_t i = 0; i < mask.size(); ++i) {
      mask[i] = static_cast<std::uint8_t>(rd() & 0xFF);
    }
    return mask;
  }

  static void appendLength64(std::vector<std::uint8_t>& frame, std::uint64_t value) {
    for (int shift = 7; shift >= 0; --shift) {
      frame.push_back(static_cast<std::uint8_t>((value >> (shift * 8)) & 0xFF));
    }
  }

  void closeSocket() {
    if (!isInvalid(socket_)) {
#if defined(_WIN32)
      closesocket(socket_);
#else
      ::close(socket_);
#endif
      socket_ = kInvalidSocket;
    }
  }

  SocketHandle socket_ = kInvalidSocket;
};

class QuantizeHarness {
 public:
  void setScale(util::ScaleQuantizer::Scale scale) { scale_ = scale; }
  void setRoot(std::uint8_t root) { root_ = static_cast<std::uint8_t>(root % 12); }
  void setMode(util::QuantizerMode mode) { mode_ = mode; }

  std::vector<util::QuantizerSample> renderSamples(const std::vector<float>& offsets,
                                                   double driftHz) const {
    const std::size_t frames = (driftHz > 0.0) ? kDefaultDriftFrames : 1;
    return util::GenerateQuantizerSamples(offsets, root_, scale_, mode_, driftHz,
                                          kDefaultDriftDepth, frames);
  }

  void narrate(const std::vector<float>& offsets,
               const std::vector<util::QuantizerSample>& samples,
               double driftHz) const {
    std::cout << "[scale-quantizer] scale=" << toString(scale_)
              << " root=" << static_cast<int>(root_)
              << " mode=" << util::ToString(mode_);
    if (driftHz > 0.0) {
      std::cout << " driftHz=" << std::fixed << std::setprecision(3) << driftHz;
    }
    std::cout << '\n';

    std::cout << "  offsets: ";
    for (std::size_t i = 0; i < offsets.size(); ++i) {
      if (i > 0) {
        std::cout << ", ";
      }
      std::cout << std::fixed << std::setprecision(2) << offsets[i];
    }
    std::cout << '\n';

    std::cout << "\n  table (t=0s):" << std::endl;
    std::cout << " slot    pitch    nearest     up     down" << std::endl;
    for (std::size_t slot = 0; slot < offsets.size(); ++slot) {
      const auto it = std::find_if(samples.begin(), samples.end(), [slot](const auto& sample) {
        return sample.slot == slot && sample.timeSeconds == 0.0;
      });
      if (it == samples.end()) {
        continue;
      }
      const util::QuantizerSample& sample = *it;
      std::cout << std::setw(5) << slot << " " << std::setw(9) << std::fixed << std::setprecision(2)
                << sample.inputPitch << " " << std::setw(9) << sample.snappedNearest << " "
                << std::setw(7) << sample.snappedUp << " " << std::setw(8) << sample.snappedDown;
      if (sample.activePitch == sample.snappedNearest && mode_ == util::QuantizerMode::kNearest) {
        std::cout << "  <-- active";
      } else if (sample.activePitch == sample.snappedUp && mode_ == util::QuantizerMode::kUp) {
        std::cout << "  <-- active";
      } else if (sample.activePitch == sample.snappedDown && mode_ == util::QuantizerMode::kDown) {
        std::cout << "  <-- active";
      }
      std::cout << '\n';
    }

    if (driftHz > 0.0) {
      std::cout << "\n  drift: rendering " << kDefaultDriftFrames
                << " frames of a " << std::fixed << std::setprecision(3) << driftHz
                << " Hz sine wobble (depth +/-" << kDefaultDriftDepth << ")" << std::endl;
    }
    std::cout << std::endl;
  }

  void exportCsv(const std::filesystem::path& path,
                 const std::vector<util::QuantizerSample>& samples) const {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    if (!file) {
      throw std::runtime_error("failed to open csv path: " + path.string());
    }
    file << util::FormatQuantizerCsv(samples, mode_);
    std::cout << "[scale-quantizer] wrote " << path << '\n';
  }

  void streamOsc(const Options& options,
                 const std::vector<util::QuantizerSample>& samples) const {
    if (!options.oscEndpoint) {
      return;
    }
    OscClient client(*options.oscEndpoint);
    for (const auto& sample : samples) {
      client.send(sample, mode_);
    }
    std::cout << "[scale-quantizer] pushed " << samples.size() << " OSC frames to "
              << *options.oscEndpoint << '\n';
  }

  void streamWebsocket(const Options& options,
                       const std::vector<util::QuantizerSample>& samples) const {
    if (!options.websocketUrl) {
      return;
    }
    WebSocketClient client(*options.websocketUrl);
    for (const auto& sample : samples) {
      std::ostringstream payload;
      payload << "{\"slot\":" << sample.slot << ",";
      payload << "\"time\":" << std::fixed << std::setprecision(4) << sample.timeSeconds << ",";
      payload << "\"input\":" << std::fixed << std::setprecision(4) << sample.inputPitch << ",";
      payload << "\"drifted\":" << std::fixed << std::setprecision(4) << sample.driftedPitch << ",";
      payload << "\"nearest\":" << std::fixed << std::setprecision(4) << sample.snappedNearest
              << ",";
      payload << "\"up\":" << std::fixed << std::setprecision(4) << sample.snappedUp << ",";
      payload << "\"down\":" << std::fixed << std::setprecision(4) << sample.snappedDown << ",";
      payload << "\"active\":" << std::fixed << std::setprecision(4) << sample.activePitch << ",";
      payload << "\"mode\":\"" << util::ToString(mode_) << "\"}";
      client.send(payload.str());
    }
    std::cout << "[scale-quantizer] streamed " << samples.size() << " websocket frames to "
              << *options.websocketUrl << '\n';
  }

 private:
  util::ScaleQuantizer::Scale scale_ = util::ScaleQuantizer::Scale::kMajor;
  std::uint8_t root_ = 0;
  util::QuantizerMode mode_ = util::QuantizerMode::kNearest;
};

}  // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
  try {
    const Options options = parseArgs(argc, argv);
    if (options.showHelp) {
      printUsage();
#if defined(_WIN32)
      WSACleanup();
#endif
      return 0;
    }

    QuantizeHarness harness;
    harness.setScale(options.scale);
    harness.setRoot(options.root);
    harness.setMode(options.mode);

    const auto samples = harness.renderSamples(options.offsets, options.driftHz);
    harness.narrate(options.offsets, samples, options.driftHz);

    if (options.exportCsv) {
      harness.exportCsv(options.csvPath, samples);
    }
    harness.streamOsc(options, samples);
    harness.streamWebsocket(options, samples);
  } catch (const std::exception& ex) {
    std::cerr << "[scale-quantizer] " << ex.what() << '\n';
    printUsage();
#if defined(_WIN32)
    WSACleanup();
#endif
    return 1;
  }
#if defined(_WIN32)
  WSACleanup();
#endif
  return 0;
}
