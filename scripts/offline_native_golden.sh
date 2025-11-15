#!/usr/bin/env bash
# Offline golden harness: build + run the native renderer without PlatformIO.
#
# Usage:
#   ./scripts/offline_native_golden.sh [--filter name] [...]
#
# Options:
#   --filter <name>    Only refresh fixtures whose manifest name or path contains
#                      <name>. Repeat the flag for multiple filters. Tokens are
#                      case-insensitive and can be comma-separated ("audio,logs").
#   --skip-manifest    Rebuild fixtures but leave tests/native_golden/golden.json
#                      untouched. Handy when you just want WAVs on disk.
#
# Requirements: g++, Python 3. This script compiles tools/native_golden_offline.cpp
# against the same engine sources the PlatformIO suite uses, emits fresh
# fixtures into build/fixtures/, and refreshes tests/native_golden/golden.json.
# Handy when the PlatformIO registry blocks downloads or you're hacking offline.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
BINARY="${BUILD_DIR}/native_golden_offline"

FILTERS=()
REFRESH_MANIFEST=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --filter)
      if [[ $# -lt 2 ]]; then
        echo "error: --filter flag requires an argument" >&2
        exit 1
      fi
      FILTERS+=("$2")
      shift 2
      ;;
    --filter=*)
      FILTERS+=("${1#--filter=}")
      shift
      ;;
    --skip-manifest)
      REFRESH_MANIFEST=0
      shift
      ;;
    --help|-h)
      sed -n '1,40p' "$0"
      exit 0
      ;;
    --)
      shift
      break
      ;;
    *)
      echo "error: unrecognized flag '$1'" >&2
      exit 1
      ;;
  esac
done

if [[ ${#FILTERS[@]} -gt 0 ]]; then
  FILTER_VALUE=$(IFS=','; printf '%s' "${FILTERS[*]}")
  # shellcheck disable=SC2034  # exported for the child binary
  export SEEDBOX_OFFLINE_GOLDEN_FILTER="${FILTER_VALUE}"
fi

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
  examples/shared/offline_renderer.cpp
  tests/native_golden/wav_helpers.cpp
  src/hal/board_native.cpp
  src/hal/hal_audio.cpp
  src/hal/hal_io.cpp
  src/hal/hal_midi_serial7.cpp
  src/io/MidiRouter.cpp
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

if [[ ${REFRESH_MANIFEST} -eq 1 ]]; then
  echo "[offline-native-golden] refreshing manifest..."
  python3 scripts/compute_golden_hashes.py --write
  echo "[offline-native-golden] regenerating fixture header..."
  python3 scripts/generate_native_golden_header.py
else
  echo "[offline-native-golden] skipping manifest refresh (per --skip-manifest)"
fi

echo "[offline-native-golden] done. Fixtures live in build/fixtures/"

popd >/dev/null
