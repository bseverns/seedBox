#include "io/Store.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>
#if !SEEDBOX_HW
  #include <filesystem>
  #include <fstream>
  #include <iterator>
#endif

#if SEEDBOX_HW
  #include <EEPROM.h>
#endif

namespace {
constexpr std::uint32_t kMagic = 0x53545231u;  // 'STR1'
constexpr std::uint8_t kVersion = 1u;

constexpr std::uint8_t kCompressedMarker = 0x00u;
constexpr std::uint8_t kTokenMarker = 0x1Fu;

// Raw-string tokens mirror verbatim JSON fragments. The custom delimiter keeps
// the punctuation readable so future tweaks don't devolve into escape soup.
constexpr std::array<std::string_view, 40> kPresetTokens{
    R"tok(,"transportLatch":false)tok",
    R"tok(,"followExternal":false)tok",
    R"tok(,"debugMeters":false)tok",
    R"tok("engineSelections":[)tok",
    R"tok(,"stereoSpread":)tok",
    R"tok(,"probability":)tok",
    R"tok(,"grainSizeMs":)tok",
    R"tok("clock":{"bpm":)tok",
    R"tok(,"windowSkew":)tok",
    R"tok(,"resonator":{)tok",
    R"tok(,"brightness":)tok",
    R"tok("masterSeed":)tok",
    R"tok(,"mutateAmt":)tok",
    R"tok(,"transpose":)tok",
    R"tok(,"granular":{)tok",
    R"tok(,"focusSeed":)tok",
    R"tok(,"sampleIdx":)tok",
    R"tok(,"feedback":)tok",
    R"tok(,"exciteMs":)tok",
    R"tok(,"jitterMs":)tok",
    R"tok(,"damping":)tok",
    R"tok(,"density":)tok",
    R"tok(,"sprayMs":)tok",
    R"tok(,"sdSlot":)tok",
    R"tok(,"spread":)tok",
    R"tok(,"source":)tok",
    R"tok(,"engine":)tok",
    R"tok("seeds":[)tok",
    R"tok(,"pitch":)tok",
    R"tok(,"mode":)tok",
    R"tok(,"prng":)tok",
    R"tok(,"envS":)tok",
    R"tok(,"tone":)tok",
    R"tok(,"bank":)tok",
    R"tok(,"envA":)tok",
    R"tok(,"envD":)tok",
    R"tok(,"envR":)tok",
    R"tok({"id":)tok",
    R"tok(},)tok",
    R"tok(}])tok",
};

std::vector<std::uint8_t> compressPresetBlob(const std::vector<std::uint8_t>& input) {
  if (input.empty()) {
    return input;
  }

  std::vector<std::uint8_t> encoded;
  encoded.reserve(input.size());

  std::size_t i = 0;
  while (i < input.size()) {
    bool matched = false;
    for (std::uint8_t tokenIndex = 0; tokenIndex < kPresetTokens.size(); ++tokenIndex) {
      const std::string_view token = kPresetTokens[tokenIndex];
      if (token.empty()) {
        continue;
      }
      if (i + token.size() <= input.size() &&
          std::memcmp(input.data() + static_cast<std::ptrdiff_t>(i), token.data(), token.size()) == 0) {
        encoded.push_back(kTokenMarker);
        encoded.push_back(tokenIndex);
        i += token.size();
        matched = true;
        break;
      }
    }
    if (matched) {
      continue;
    }

    const std::uint8_t byte = input[i++];
    if (byte == kTokenMarker) {
      encoded.push_back(kTokenMarker);
      encoded.push_back(static_cast<std::uint8_t>(kPresetTokens.size()));
    }
    encoded.push_back(byte);
  }

  if (encoded.size() >= input.size()) {
    return input;
  }

  std::vector<std::uint8_t> result;
  result.reserve(encoded.size() + 1);
  result.push_back(kCompressedMarker);
  result.insert(result.end(), encoded.begin(), encoded.end());
  return result;
}

std::vector<std::uint8_t> decompressPresetBlob(const std::vector<std::uint8_t>& stored) {
  if (stored.empty() || stored[0] != kCompressedMarker) {
    return stored;
  }

  std::vector<std::uint8_t> decoded;
  decoded.reserve(stored.size());

  std::size_t i = 1;
  while (i < stored.size()) {
    const std::uint8_t byte = stored[i++];
    if (byte == kTokenMarker) {
      if (i >= stored.size()) {
        return {};
      }
      const std::uint8_t code = stored[i++];
      if (code == kPresetTokens.size()) {
        decoded.push_back(kTokenMarker);
        continue;
      }
      if (code >= kPresetTokens.size()) {
        return {};
      }
      const std::string_view token = kPresetTokens[code];
      decoded.insert(decoded.end(), token.begin(), token.end());
      continue;
    }
    decoded.push_back(byte);
  }

  return decoded;
}

}  // namespace

namespace seedbox::io {

StoreEeprom::StoreEeprom(std::size_t capacity) {
#if SEEDBOX_HW
  const std::size_t hwLen = EEPROM.length();
  capacity_ = capacity ? std::min(capacity, hwLen) : hwLen;
#else
  capacity_ = capacity ? capacity : static_cast<std::size_t>(1024);
  mockEeprom_.assign(capacity_, 0xFFu);
#endif
  if (capacity_ == 0) {
    capacity_ = 1;  // never let encode math divide by zero
  }
}

std::vector<std::string> StoreEeprom::list() const {
  std::vector<std::uint8_t> raw;
  if (!readRaw(raw)) {
    return {};
  }
  const auto entries = decode(raw);
  std::vector<std::string> names;
  names.reserve(entries.size());
  for (const auto& e : entries) {
    names.push_back(e.slot);
  }
  return names;
}

bool StoreEeprom::load(std::string_view slot, std::vector<std::uint8_t>& out) const {
  std::vector<std::uint8_t> raw;
  if (!readRaw(raw)) {
    return false;
  }
  const auto entries = decode(raw);
  for (const auto& e : entries) {
    if (e.slot == slot) {
      auto decoded = decompressPresetBlob(e.data);
      if (decoded.empty() && !e.data.empty() && e.data[0] == kCompressedMarker) {
        return false;
      }
      out = std::move(decoded);
      return true;
    }
  }
  return false;
}

bool StoreEeprom::save(std::string_view slot, const std::vector<std::uint8_t>& data) {
  // Quiet-mode labs can still persist presets when we're running the simulator;
  // we only short-circuit writes on real hardware so classroom rigs stay read-only
  // unless QUIET_MODE is cleared on purpose.
  if constexpr (SeedBoxConfig::kQuietMode && SeedBoxConfig::kHardwareBuild) {
    (void)slot;
    (void)data;
    return false;
  }
  std::vector<std::uint8_t> raw;
  if (!readRaw(raw)) {
    return false;
  }
  auto entries = decode(raw);
  auto it = std::find_if(entries.begin(), entries.end(), [&](const Entry& e) { return e.slot == slot; });
  const auto compressed = compressPresetBlob(data);
  if (it == entries.end()) {
    entries.push_back(Entry{std::string(slot), compressed});
  } else {
    it->data = compressed;
  }
  const auto encoded = encode(entries);
  if (encoded.empty()) {
    return false;
  }
  return writeRaw(encoded);
}

bool StoreEeprom::readRaw(std::vector<std::uint8_t>& raw) const {
  raw.assign(capacity_, 0xFFu);
#if SEEDBOX_HW
  const std::size_t hwLen = EEPROM.length();
  const std::size_t copy = std::min(capacity_, hwLen);
  for (std::size_t i = 0; i < copy; ++i) {
    raw[i] = EEPROM.read(static_cast<int>(i));
  }
#else
  if (mockEeprom_.size() != capacity_) {
    mockEeprom_.assign(capacity_, 0xFFu);
  }
  std::copy(mockEeprom_.begin(), mockEeprom_.begin() + std::min(capacity_, mockEeprom_.size()), raw.begin());
#endif
  return true;
}

bool StoreEeprom::writeRaw(const std::vector<std::uint8_t>& raw) {
  if (raw.size() > capacity_) {
    return false;
  }
#if SEEDBOX_HW
  const std::size_t hwLen = EEPROM.length();
  const std::size_t copy = std::min(raw.size(), hwLen);
  for (std::size_t i = 0; i < copy; ++i) {
    EEPROM.write(static_cast<int>(i), raw[i]);
  }
  // Clear any trailing bytes beyond new payload so old junk doesn't stick
  for (std::size_t i = copy; i < std::min(capacity_, hwLen); ++i) {
    EEPROM.write(static_cast<int>(i), 0xFFu);
  }
#else
  mockEeprom_.assign(capacity_, 0xFFu);
  std::copy(raw.begin(), raw.end(), mockEeprom_.begin());
#endif
  return true;
}

std::vector<StoreEeprom::Entry> StoreEeprom::decode(const std::vector<std::uint8_t>& raw) const {
  std::vector<Entry> entries;
  if (raw.size() < 6) {
    return entries;
  }
  const std::uint32_t magic = static_cast<std::uint32_t>(raw[0]) |
                              (static_cast<std::uint32_t>(raw[1]) << 8u) |
                              (static_cast<std::uint32_t>(raw[2]) << 16u) |
                              (static_cast<std::uint32_t>(raw[3]) << 24u);
  if (magic != kMagic) {
    return entries;
  }
  const std::uint8_t version = raw[4];
  if (version != kVersion) {
    return entries;
  }
  const std::uint8_t count = raw[5];
  std::size_t offset = 6;
  for (std::uint8_t i = 0; i < count; ++i) {
    if (offset >= raw.size()) {
      break;
    }
    const std::uint8_t nameLen = raw[offset++];
    if (nameLen == 0 || offset + nameLen > raw.size()) {
      break;
    }
    const std::string slot(reinterpret_cast<const char*>(&raw[offset]), nameLen);
    offset += nameLen;
    if (offset + 2 > raw.size()) {
      break;
    }
    const std::uint16_t dataLen = static_cast<std::uint16_t>(raw[offset]) |
                                  (static_cast<std::uint16_t>(raw[offset + 1]) << 8u);
    offset += 2;
    if (offset + dataLen > raw.size()) {
      break;
    }
    Entry entry{slot, {}};
    entry.data.insert(entry.data.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                      raw.begin() + static_cast<std::ptrdiff_t>(offset + dataLen));
    entries.push_back(std::move(entry));
    offset += dataLen;
  }
  return entries;
}

std::vector<std::uint8_t> StoreEeprom::encode(const std::vector<Entry>& entries) const {
  std::vector<std::uint8_t> payload;
  payload.reserve(capacity_);
  payload.push_back(static_cast<std::uint8_t>(kMagic & 0xFFu));
  payload.push_back(static_cast<std::uint8_t>((kMagic >> 8u) & 0xFFu));
  payload.push_back(static_cast<std::uint8_t>((kMagic >> 16u) & 0xFFu));
  payload.push_back(static_cast<std::uint8_t>((kMagic >> 24u) & 0xFFu));
  payload.push_back(kVersion);
  payload.push_back(static_cast<std::uint8_t>(std::min<std::size_t>(entries.size(), 0xFFu)));
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const Entry& entry = entries[i];
    const std::size_t nameLen = std::min<std::size_t>(entry.slot.size(), 0xFEu);
    const std::size_t dataLen = std::min<std::size_t>(entry.data.size(), 0xFFFFu);
    if (payload.size() + 1 + nameLen + 2 + dataLen > capacity_) {
      return {};
    }
    payload.push_back(static_cast<std::uint8_t>(nameLen));
    payload.insert(payload.end(), entry.slot.begin(), entry.slot.begin() + static_cast<std::ptrdiff_t>(nameLen));
    payload.push_back(static_cast<std::uint8_t>(dataLen & 0xFFu));
    payload.push_back(static_cast<std::uint8_t>((dataLen >> 8u) & 0xFFu));
    payload.insert(payload.end(), entry.data.begin(), entry.data.begin() + static_cast<std::ptrdiff_t>(dataLen));
  }
  if (payload.size() < capacity_) {
    payload.resize(capacity_, 0xFFu);
  }
  return payload;
}

StoreSd::StoreSd(std::string basePath) : basePath_(std::move(basePath)) {
#if !SEEDBOX_HW
  if (basePath_.empty()) {
    basePath_ = "presets";
  }
  std::filesystem::create_directories(basePath_);
#endif
}

std::vector<std::string> StoreSd::list() const {
#if SEEDBOX_HW
  (void)basePath_;
  return {};
#else
  std::vector<std::string> names;
  if (basePath_.empty()) {
    return names;
  }
  for (const auto& dirEntry : std::filesystem::directory_iterator(basePath_)) {
    if (!dirEntry.is_regular_file()) {
      continue;
    }
    names.push_back(dirEntry.path().stem().string());
  }
  std::sort(names.begin(), names.end());
  return names;
#endif
}

bool StoreSd::load(std::string_view slot, std::vector<std::uint8_t>& out) const {
#if SEEDBOX_HW
  (void)slot;
  (void)out;
  return false;
#else
  if (basePath_.empty()) {
    return false;
  }
  std::filesystem::path file = std::filesystem::path(basePath_) / (std::string(slot) + ".json");
  std::ifstream in(file, std::ios::binary);
  if (!in.good()) {
    return false;
  }
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
#endif
}

bool StoreSd::save(std::string_view slot, const std::vector<std::uint8_t>& data) {
  // Same story as the EEPROM backend: native sims may write freely while
  // hardware quiet-mode stays locked down.
  if constexpr (SeedBoxConfig::kQuietMode && SeedBoxConfig::kHardwareBuild) {
    (void)slot;
    (void)data;
    return false;
  }
#if SEEDBOX_HW
  (void)slot;
  (void)data;
  return false;
#else
  if (basePath_.empty()) {
    return false;
  }
  std::filesystem::path file = std::filesystem::path(basePath_) / (std::string(slot) + ".json");
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  if (!out.good()) {
    return false;
  }
  out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  return out.good();
#endif
}

}  // namespace seedbox::io
