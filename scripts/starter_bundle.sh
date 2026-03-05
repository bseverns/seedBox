#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_TESTS=1
RUN_PKG_INSTALL=1

usage() {
  cat <<'EOF'
Usage: scripts/starter_bundle.sh [options]

Bootstraps the minimal laptop-native SeedBox workflow:
1) installs PlatformIO packages
2) runs native tests

Options:
  --skip-pkg-install   Skip `pio pkg install`
  --skip-tests         Skip `pio test -e native`
  -h, --help           Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-pkg-install)
      RUN_PKG_INSTALL=0
      shift
      ;;
    --skip-tests)
      RUN_TESTS=0
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown argument: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if ! command -v pio >/dev/null 2>&1; then
  cat >&2 <<'EOF'
error: PlatformIO CLI is required but `pio` was not found.
Install it with:
  pip install --upgrade platformio
EOF
  exit 1
fi

run_step() {
  echo "[starter-bundle] $*"
  "$@"
}

cd "$ROOT_DIR"

if [[ "$RUN_PKG_INSTALL" -eq 1 ]]; then
  run_step pio pkg install
fi

if [[ "$RUN_TESTS" -eq 1 ]]; then
  run_step pio test -e native
fi

cat <<'EOF'

Starter bundle complete.
Next moves:
  - Firmware build: pio run -e teensy40
  - Desktop plugin/app: see docs/juce_build.md
  - Containerized workflow: see containers/native-dev/README.md
EOF
