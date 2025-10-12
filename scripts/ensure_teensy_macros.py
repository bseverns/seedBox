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

from dataclasses import dataclass
from typing import Iterable, Iterator, List, Optional, Tuple

from SCons.Script import Import  # type: ignore

Import("env")  # PlatformIO injects this SCons construction environment


@dataclass(frozen=True)
class Macro:
    """A cleaned representation of a preprocessor macro."""

    name: str
    value: Optional[str]

    @classmethod
    def from_raw(cls, entry: object) -> "Macro":
        """Normalise the wild west of CPPDEFINES inputs into ``Macro`` objects."""

        if isinstance(entry, tuple):
            if not entry:
                raise ValueError("CPPDEFINES tuple cannot be empty")
            name = str(entry[0])
            value = None if len(entry) == 1 else str(entry[1])
            return cls(name=name, value=value)

        if isinstance(entry, str):
            if "=" in entry:
                name, value = entry.split("=", 1)
                return cls(name=name, value=value or None)
            return cls(name=entry, value=None)

        raise TypeError(f"Unsupported CPPDEFINES entry: {entry!r}")


def _iter_macros() -> Iterator[Macro]:
    """Yield every CPPDEFINES entry that already exists on the env."""

    defines = env.get("CPPDEFINES") or []  # type: ignore[attr-defined]

    if isinstance(defines, (str, tuple)):
        defines = [defines]

    for entry in defines:  # type: ignore[assignment]
        try:
            yield Macro.from_raw(entry)
        except Exception as exc:  # pragma: no cover - defensive guardrail
            print(f"[ensure_teensy_macros] Skipping malformed CPPDEFINE {entry!r}: {exc}")


def _append_define(name: str, value: Optional[str]) -> None:
    if value is None:
        env.Append(CPPDEFINES=[name])  # type: ignore[attr-defined]
    else:
        env.Append(CPPDEFINES=[(name, value)])  # type: ignore[attr-defined]


def _missing_macros(existing: Iterable[Macro]) -> List[Tuple[str, Optional[str]]]:
    """Return the macros from :data:`REQUIRED_MACROS` that need injecting."""

    have = {macro.name: macro.value for macro in existing}
    missing: List[Tuple[str, Optional[str]]] = []
    for name, value in REQUIRED_MACROS.items():
        if name not in have:
            missing.append((name, value))
        elif value is not None and have[name] != value:
            # PlatformIO sometimes defines the macro but leaves it empty.
            missing.append((name, value))
    return missing


# Map of required macro -> value. ``None`` means ``-D MACRO`` with no value.
REQUIRED_MACROS = {
    "ARDUINO_TEENSY40": "1",
    "TEENSYDUINO": "158",
    "__ARM_ARCH_7EM__": "1",
    "__IMXRT1062__": "1",
}


patched: List[str] = []
for name, value in _missing_macros(_iter_macros()):
    _append_define(name, value)
    patched.append(f"{name}={value}" if value is not None else name)

if patched:
    print(
        "[ensure_teensy_macros] Injected missing CPPDEFINES: "
        + ", ".join(patched)
    )
else:
    print("[ensure_teensy_macros] All required CPPDEFINES already present.")
