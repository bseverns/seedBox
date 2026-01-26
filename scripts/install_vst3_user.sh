#!/usr/bin/env bash
set -euo pipefail

source_path="${1:-}"

if [[ -z "${source_path}" ]]; then
  source_path=$(find build/juce -path "*/SeedboxVST3_artefacts/*/VST3/SeedBox.vst3" | head -n 1 || true)
fi

if [[ -z "${source_path}" ]]; then
  echo "SeedBox.vst3 not found. Build the VST3 first." >&2
  exit 1
fi

if [[ ! -d "${source_path}" ]]; then
  echo "SeedBox.vst3 bundle not found at: ${source_path}" >&2
  exit 1
fi

dest_dir="${HOME}/Library/Audio/Plug-Ins/VST3"
mkdir -p "${dest_dir}"

rm -rf "${dest_dir}/SeedBox.vst3"
cp -R "${source_path}" "${dest_dir}/SeedBox.vst3"

echo "Installed SeedBox.vst3 to ${dest_dir}"
