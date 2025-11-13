#!/usr/bin/env bash
# Offline golden harness: build + run the native renderer without PlatformIO.
#
# Usage:
#   ./scripts/offline_native_golden.sh
#
# Requirements: g++, Python 3. This script compiles tools/native_golden_offline.cpp
# against the same engine sources the PlatformIO suite uses, emits fresh
# fixtures into build/fixtures/, and refreshes tests/native_golden/golden.json.
# Handy when the PlatformIO registry blocks downloads or you're hacking offline.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
BINARY="${BUILD_DIR}/native_golden_offline"

mkdir -p "${BUILD_DIR}"

CXXFLAGS=(
  -std=gnu++17
  -O2
  -pipe
  -DSEEDBOX_SIM=1
  -DSEEDBOX_HW=0
  -DQUIET_MODE=0
  -DENABLE_GOLDEN=1
  -DARDUINOJSON_USE_DOUBLE=1
  -I.
  -Iinclude
  -Isrc
  -Itests/native_golden
)

SOURCES=(
  tools/native_golden_offline.cpp
  tests/native_golden/wav_helpers.cpp
  src/hal/board_native.cpp
  src/hal/hal_audio.cpp
  src/hal/hal_io.cpp
  src/hal/hal_midi_serial7.cpp
  src/io/MidiRouter.cpp
  src/io/Storage.cpp
  src/io/Store.cpp
  src/engine/Sampler.cpp
  src/engine/EngineRouter.cpp
  src/engine/Resonator.cpp
  src/engine/Granular.cpp
  src/engine/Patterns.cpp
  src/engine/EuclidEngine.cpp
  src/engine/BurstEngine.cpp
  src/util/ScaleQuantizer.cpp
  src/util/ScaleQuantizerFlow.cpp
)

pushd "${ROOT_DIR}" >/dev/null

echo "[offline-native-golden] compiling helper..."
g++ "${CXXFLAGS[@]}" "${SOURCES[@]}" -o "${BINARY}"

echo "[offline-native-golden] rendering fixtures..."
"${BINARY}"

echo "[offline-native-golden] refreshing manifest..."
python scripts/compute_golden_hashes.py --write

echo "[offline-native-golden] done. Fixtures live in build/fixtures/"

popd >/dev/null
