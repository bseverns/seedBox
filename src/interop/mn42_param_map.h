#pragma once

//
// MN42 → SeedBox parameter map skeleton.
// -------------------------------------
// The controller integration is still cooking, but we expose the structure now
// so future PRs can drop real mappings without re-litigating design decisions.
// Treat this as a studio notebook: enough scaffolding to explain the intent,
// with plenty of TODO space left for the actual patchwork of parameters.

#include <array>
#include <cstdint>
#include <string_view>

namespace seedbox::interop::mn42 {

struct ParamDescriptor {
  std::uint8_t controller = 0;
  std::string_view label{};
  std::string_view notes{};
};

struct ParamMap {
  std::array<ParamDescriptor, 32> entries{};
  std::size_t size = 0;
};

// Returns a shared, mutable map stub so higher layers can plug in controller
// definitions at runtime.  Keeping this inline avoids static init cost on the
// Teensy build while still letting unit tests tweak the layout.
inline ParamMap& GetMutableParamMap() {
  static ParamMap map{};
  return map;
}

}  // namespace seedbox::interop::mn42
