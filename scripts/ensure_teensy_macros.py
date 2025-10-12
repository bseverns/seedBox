"""PlatformIO pre-build hook to ensure Teensy hardware ID macros exist.

We occasionally see CI builds where PlatformIO forgets to define the
Teensy-flavoured identity macros that the PJRC Audio library expects.
When those macros vanish, classes like ``AudioMixer4`` stop providing
their concrete implementations and the synth graph crumbles into a pile
of abstract base classes. This hook double-checks the preprocessor state
and patches in the missing macros only when they are absent so we avoid
``-D`` redefinition warnings.
"""
from __future__ import annotations

from SCons.Script import Import  # type: ignore

Import("env")  # PlatformIO injects this SCons construction environment

# Map of required macro -> value. ``None`` means ``-D MACRO`` with no value.
REQUIRED_MACROS = {
    "ARDUINO_TEENSY40": "1",
    "TEENSYDUINO": "158",
    "__ARM_ARCH_7EM__": "1",
    "__IMXRT1062__": "1",
}


def _iter_defines():
    """Yield every CPPDEFINES entry that already exists on the env."""

    defines = env.get("CPPDEFINES") or []  # type: ignore[attr-defined]
    if isinstance(defines, (str, tuple)):
        defines = [defines]

    # SCons happily stores either strings or (name, value) tuples in here.
    for entry in defines:  # type: ignore[assignment]
        yield entry


def _defined(name: str) -> bool:
    """Return True if *name* already appears in the CPPDEFINES list."""
    for entry in _iter_defines():
        if isinstance(entry, tuple):
            macro, *_ = entry
            if macro == name:
                return True
        elif entry == name:
            return True
    return False


def _append_define(name: str, value: str | None) -> None:
    if value is None:
        env.Append(CPPDEFINES=[name])  # type: ignore[attr-defined]
    else:
        env.Append(CPPDEFINES=[(name, value)])  # type: ignore[attr-defined]


patched = []
for macro, value in REQUIRED_MACROS.items():
    if not _defined(macro):
        _append_define(macro, value)
        patched.append(f"{macro}={value}" if value is not None else macro)

if patched:
    print(
        "[ensure_teensy_macros] Injected missing CPPDEFINES: "
        + ", ".join(patched)
    )
else:
    print("[ensure_teensy_macros] All required CPPDEFINES already present.")
