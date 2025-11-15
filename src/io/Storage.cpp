//
// Storage.cpp
// ------------
// High-level helpers that bridge the legacy storage hooks used by early UI
// experiments with the newer `Store` backends.  The goal is to let firmware
// lessons keep calling `Storage::loadSeedBank` / `Storage::saveScene` while the
// underlying implementation graduates to real EEPROM + SD plumbing.
#include "io/Storage.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#if !SEEDBOX_HW
  #include <iterator>
#endif

#include "SeedBoxConfig.h"
#include "app/AppState.h"
#include "app/Preset.h"
#include "io/Store.h"

#if SEEDBOX_HW
  #include <Arduino.h>
  #include <SdFat.h>
  namespace {
  // Teensy's Arduino core ships both the legacy SD wrapper (`SDClass SD`) and
  // Bill Greiman's SdFat implementation.  The legacy symbol stomps on any
  // attempt to declare our own `SD`, so keep our instance tucked behind a
  // different banner.
  SdFat gSdFat;
  }
#else
  #include <filesystem>
  #include <fstream>
#endif

namespace {

std::vector<::AppState*> gAppStack;

::AppState* activeApp() {
  return gAppStack.empty() ? nullptr : gAppStack.back();
}

enum class Backend { kEeprom, kSd };

struct StorageSpec {
  Backend backend{Backend::kEeprom};
  std::string key;        // Slot name for EEPROM or relative path for SD.
  std::string slotLabel;  // Human label fed into preset snapshots.
};

bool startsWithIgnoreCase(std::string_view value, std::string_view prefix) {
  if (prefix.size() > value.size()) {
    return false;
  }
  for (std::size_t i = 0; i < prefix.size(); ++i) {
    const unsigned char a = static_cast<unsigned char>(value[i]);
    const unsigned char b = static_cast<unsigned char>(prefix[i]);
    if (std::tolower(a) != std::tolower(b)) {
      return false;
    }
  }
  return true;
}

bool endsWithIgnoreCase(std::string_view value, std::string_view suffix) {
  if (suffix.size() > value.size()) {
    return false;
  }
  const std::size_t offset = value.size() - suffix.size();
  for (std::size_t i = 0; i < suffix.size(); ++i) {
    const unsigned char a = static_cast<unsigned char>(value[offset + i]);
    const unsigned char b = static_cast<unsigned char>(suffix[i]);
    if (std::tolower(a) != std::tolower(b)) {
      return false;
    }
  }
  return true;
}

std::string trim(std::string_view view) {
  const auto begin = view.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) {
    return {};
  }
  const auto end = view.find_last_not_of(" \t\r\n");
  return std::string(view.substr(begin, end - begin + 1));
}

std::string sanitizeSlot(std::string_view raw) {
  std::string slot;
  slot.reserve(raw.size());
  for (unsigned char ch : raw) {
    if (std::isalnum(ch) || ch == '-' || ch == '_') {
      slot.push_back(static_cast<char>(ch));
    } else if (ch == ' ') {
      slot.push_back('_');
    }
  }
  if (slot.empty()) {
    slot = "default";
  }
  if (slot.size() > 0xFEu) {
    slot.resize(0xFEu);
  }
  return slot;
}

std::string sanitizeRelativePath(std::string_view raw) {
  std::vector<std::string> segments;
  std::string current;
  auto flushSegment = [&](std::string& seg) {
    if (seg.empty()) {
      return;
    }
    if (seg == ".") {
      seg.clear();
      return;
    }
    if (seg == "..") {
      if (!segments.empty()) {
        segments.pop_back();
      }
      seg.clear();
      return;
    }
    segments.push_back(seg);
    seg.clear();
  };

  for (unsigned char ch : raw) {
    const char normalized = (ch == '\\') ? '/' : static_cast<char>(ch);
    if (normalized == '/') {
      flushSegment(current);
      continue;
    }
    current.push_back(normalized);
  }
  flushSegment(current);

  std::string rebuilt;
  for (std::size_t i = 0; i < segments.size(); ++i) {
    if (i > 0) {
      rebuilt.push_back('/');
    }
    rebuilt += segments[i];
  }
  if (rebuilt.empty()) {
    rebuilt = "default";
  }
  return rebuilt;
}

std::string ensureJsonExtension(std::string path) {
  if (endsWithIgnoreCase(path, ".json")) {
    return path;
  }
  if (!path.empty() && path.back() == '.') {
    path.pop_back();
  }
  path += ".json";
  return path;
}

std::optional<StorageSpec> parseSpec(const char* path) {
  const std::string cleaned = trim(path ? std::string_view{path} : std::string_view{});
  if (cleaned.empty()) {
    return std::nullopt;
  }
  std::string_view view(cleaned);
  StorageSpec spec;
  if (startsWithIgnoreCase(view, "sd://")) {
    spec.backend = Backend::kSd;
    spec.key = ensureJsonExtension(sanitizeRelativePath(view.substr(5)));
  } else if (startsWithIgnoreCase(view, "sd:")) {
    spec.backend = Backend::kSd;
    spec.key = ensureJsonExtension(sanitizeRelativePath(view.substr(3)));
  } else if (startsWithIgnoreCase(view, "file://")) {
    spec.backend = Backend::kSd;
    spec.key = ensureJsonExtension(sanitizeRelativePath(view.substr(7)));
  } else if (startsWithIgnoreCase(view, "file:")) {
    spec.backend = Backend::kSd;
    spec.key = ensureJsonExtension(sanitizeRelativePath(view.substr(5)));
  } else if (startsWithIgnoreCase(view, "eeprom:")) {
    spec.backend = Backend::kEeprom;
    spec.key = sanitizeSlot(view.substr(7));
  } else {
    spec.backend = Backend::kEeprom;
    spec.key = sanitizeSlot(view);
  }

  if (spec.backend == Backend::kSd) {
    spec.slotLabel = spec.key;
    const auto slash = spec.slotLabel.find_last_of('/');
    if (slash != std::string::npos) {
      spec.slotLabel = spec.slotLabel.substr(slash + 1);
    }
    if (endsWithIgnoreCase(spec.slotLabel, ".json")) {
      spec.slotLabel.erase(spec.slotLabel.size() - 5);
    }
    if (spec.slotLabel.empty()) {
      spec.slotLabel = "default";
    }
  } else {
    spec.slotLabel = spec.key;
  }
  return spec;
}

seedbox::io::StoreEeprom& eepromStore() {
#if SEEDBOX_HW
  static seedbox::io::StoreEeprom store;
#else
  static seedbox::io::StoreEeprom store(4096);
#endif
  return store;
}

#if SEEDBOX_HW
bool ensureSdReady() {
  static bool initialised = false;
  static bool mounted = false;
  if (!initialised) {
#if defined(BUILTIN_SDCARD)
    mounted = gSdFat.begin(BUILTIN_SDCARD);
#else
    mounted = gSdFat.begin();
#endif
    initialised = true;
  }
  return mounted;
}

bool ensureSdDirectories(const std::string& absolutePath) {
  if (absolutePath.empty()) {
    return false;
  }
  std::size_t start = 0;
  std::string prefix;
  if (absolutePath.front() == '/') {
    prefix = "/";
    start = 1;
  }
  while (start < absolutePath.size()) {
    const std::size_t slash = absolutePath.find('/', start);
    if (slash == std::string::npos) {
      break;
    }
    const std::size_t length = slash - start;
    if (length == 0) {
      start = slash + 1;
      continue;
    }
    if (prefix.size() > 1 && prefix.back() != '/') {
      prefix.push_back('/');
    }
    prefix += absolutePath.substr(start, length);
    if (!gSdFat.exists(prefix.c_str())) {
      if (!gSdFat.mkdir(prefix.c_str())) {
        return false;
      }
    }
    start = slash + 1;
  }
  return true;
}
#else
std::filesystem::path hostStorageRoot() {
  static std::filesystem::path root = [] {
    if (const char* env = std::getenv("SEEDBOX_STORAGE_ROOT"); env && *env) {
      return std::filesystem::path(env);
    }
    return std::filesystem::current_path() / "out" / "storage";
  }();
  std::error_code ec;
  std::filesystem::create_directories(root, ec);
  return root;
}

std::filesystem::path buildHostPath(const std::string& relativeKey) {
  auto path = hostStorageRoot() / relativeKey;
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  return path;
}
#endif

bool deserializePreset(const std::vector<std::uint8_t>& bytes, std::vector<Seed>& outSeeds) {
  seedbox::Preset preset;
  if (!seedbox::Preset::deserialize(bytes, preset)) {
    return false;
  }
  outSeeds = preset.seeds;
  return true;
}

}  // namespace

namespace Storage {

void registerApp(::AppState& app) {
  const auto existing = std::find(gAppStack.begin(), gAppStack.end(), &app);
  if (existing != gAppStack.end()) {
    gAppStack.erase(existing);
  }
  gAppStack.push_back(&app);
}

void unregisterApp(const ::AppState& app) {
  if (!gAppStack.empty() && gAppStack.back() == &app) {
    gAppStack.pop_back();
    return;
  }
  const auto existing = std::find(gAppStack.begin(), gAppStack.end(), &app);
  if (existing != gAppStack.end()) {
    gAppStack.erase(existing);
  }
}

bool loadSeedBank(const char* path, std::vector<Seed>& out) {
  out.clear();
  const auto specOpt = parseSpec(path);
  if (!specOpt) {
    return false;
  }
  const StorageSpec& spec = *specOpt;

  switch (spec.backend) {
    case Backend::kEeprom: {
      std::vector<std::uint8_t> bytes;
      if (!eepromStore().load(spec.key, bytes)) {
        return false;
      }
      return deserializePreset(bytes, out);
    }
    case Backend::kSd: {
#if SEEDBOX_HW
      if (!ensureSdReady()) {
        return false;
      }
      std::string sdPath = spec.key;
      if (sdPath.empty()) {
        return false;
      }
      if (sdPath.front() != '/') {
        sdPath.insert(sdPath.begin(), '/');
      }
      FsFile file = gSdFat.open(sdPath.c_str(), O_RDONLY);
      if (!file) {
        return false;
      }
      const auto fileSize = static_cast<std::size_t>(file.fileSize());
      std::vector<std::uint8_t> bytes(fileSize);
      if (fileSize > 0) {
        const auto read = file.read(bytes.data(), fileSize);
        file.close();
        if (read != static_cast<int>(fileSize)) {
          return false;
        }
      } else {
        file.close();
      }
      return deserializePreset(bytes, out);
#else
      const std::filesystem::path hostPath = buildHostPath(spec.key);
      std::ifstream in(hostPath, std::ios::binary);
      if (!in.good()) {
        return false;
      }
      std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      return deserializePreset(bytes, out);
#endif
    }
  }
  return false;
}

bool saveScene(const char* path) {
  const auto specOpt = parseSpec(path);
  if (!specOpt) {
    return false;
  }
  const StorageSpec& spec = *specOpt;

  if constexpr (SeedBoxConfig::kQuietMode && SeedBoxConfig::kHardwareBuild) {
    (void)spec;
    return false;
  }

  ::AppState* app = activeApp();
  if (!app) {
    return false;
  }
  const seedbox::Preset snapshot = app->snapshotPreset(spec.slotLabel);
  const std::vector<std::uint8_t> bytes = snapshot.serialize();
  if (bytes.empty()) {
    return false;
  }

  switch (spec.backend) {
    case Backend::kEeprom:
      return eepromStore().save(spec.key, bytes);
    case Backend::kSd: {
#if SEEDBOX_HW
      if (!ensureSdReady()) {
        return false;
      }
      std::string sdPath = spec.key;
      if (sdPath.empty()) {
        return false;
      }
      if (sdPath.front() != '/') {
        sdPath.insert(sdPath.begin(), '/');
      }
      if (!ensureSdDirectories(sdPath)) {
        return false;
      }
      FsFile file = gSdFat.open(sdPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
      if (!file) {
        return false;
      }
      const auto written = file.write(bytes.data(), bytes.size());
      file.sync();
      file.close();
      return written == static_cast<int>(bytes.size());
#else
      const std::filesystem::path hostPath = buildHostPath(spec.key);
      std::ofstream out(hostPath, std::ios::binary | std::ios::trunc);
      if (!out.good()) {
        return false;
      }
      out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
      return out.good();
#endif
    }
  }
  return false;
}

}  // namespace Storage

