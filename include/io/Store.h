#pragma once

//
// Store.h
// -------
// Abstraction for non-volatile storage backends.  The firmware leans on this
// to stash presets regardless of whether we're running on a laptop (sim builds)
// or a Teensy with real EEPROM / SD hanging off it.  Keeping the interface tiny
// means we can swap backends in labs without rewriting UI flows.
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "SeedBoxConfig.h"

namespace seedbox::io {

class Store {
public:
  virtual ~Store() = default;

  // Enumerate known preset slots.  Backends are free to return human names
  // ("A", "B", "jam-2024-05"...) or be empty if nothing lives in storage yet.
  virtual std::vector<std::string> list() const = 0;

  // Load raw preset bytes for a given slot.  Returns false if the slot does not
  // exist or the backend hit IO drama.  The caller decides how to decode the
  // blob (JSON, binary, whatever).
  virtual bool load(std::string_view slot, std::vector<std::uint8_t>& out) const = 0;

  // Persist a preset blob.  Implementations decide where it lands; callers
  // should treat a false return as "nope, storage stayed untouched".
  virtual bool save(std::string_view slot, const std::vector<std::uint8_t>& data) = 0;
};

// Null backend: always empty, never writes.  Handy for tests that want to
// assert "no storage" behaviour or when QUIET_MODE keeps hardware sealed.
class StoreNull : public Store {
public:
  std::vector<std::string> list() const override { return {}; }
  bool load(std::string_view, std::vector<std::uint8_t>&) const override { return false; }
  bool save(std::string_view, const std::vector<std::uint8_t>&) override { return false; }
};

// EEPROM-backed store.  Teensy builds talk to the on-board EEPROM; native builds
// get a simulated byte array so tests can run without hardware.  Writes are
// automatically gated when QUIET_MODE stays true so classroom rigs remain
// read-only by default.
class StoreEeprom : public Store {
public:
  explicit StoreEeprom(std::size_t capacity = 0);

  std::vector<std::string> list() const override;
  bool load(std::string_view slot, std::vector<std::uint8_t>& out) const override;
  bool save(std::string_view slot, const std::vector<std::uint8_t>& data) override;

private:
  struct Entry {
    std::string slot;
    std::vector<std::uint8_t> data;
  };

  bool readRaw(std::vector<std::uint8_t>& raw) const;
  bool writeRaw(const std::vector<std::uint8_t>& raw);
  std::vector<Entry> decode(const std::vector<std::uint8_t>& raw) const;
  std::vector<std::uint8_t> encode(const std::vector<Entry>& entries) const;

  std::size_t capacity_{0};
#if !SEEDBOX_HW
  mutable std::vector<std::uint8_t> mockEeprom_{};
#endif
};

// Optional SD card backend.  On hardware we expect an SD card mounted via the
// usual `SdFat` stack; on native builds we fall back to a host directory so
// tests can prove the JSON flow without a solder party.
class StoreSd : public Store {
public:
  explicit StoreSd(std::string basePath = {});

  std::vector<std::string> list() const override;
  bool load(std::string_view slot, std::vector<std::uint8_t>& out) const override;
  bool save(std::string_view slot, const std::vector<std::uint8_t>& data) override;

private:
  std::string basePath_;
};

}  // namespace seedbox::io
