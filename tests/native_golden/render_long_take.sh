#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

cd "${REPO_ROOT}"

export ENABLE_GOLDEN=1

# We rely on PlatformIO's CLI, but depending on how the repo is used the
# executable may be exposed as `pio`, `platformio`, or only available via
# Python's module entrypoint. Try each option so the script works in CI,
# developer shells, and hermetic sandboxes alike.
for candidate in pio platformio; do
  if command -v "${candidate}" >/dev/null 2>&1; then
    exec "${candidate}" test -e native_golden --filter test_render_long_take_golden "$@"
  fi
done

for py_candidate in python3 python; do
  if command -v "${py_candidate}" >/dev/null 2>&1 \
    && "${py_candidate}" -c "import platformio" >/dev/null 2>&1; then
    exec "${py_candidate}" -m platformio test -e native_golden --filter test_render_long_take_golden "$@"
  fi
done

cat <<'EOF' >&2
PlatformIO CLI not found. Install it with `pip install -U platformio` or ensure
`pio`/`platformio` is on your PATH.
EOF
exit 127
