"""Unit tests for scripts/compute_golden_hashes.py."""

from __future__ import annotations

import importlib.util
from pathlib import Path

import pytest

_REPO_ROOT = Path(__file__).resolve().parents[1]
_SCRIPT_PATH = _REPO_ROOT / "scripts" / "compute_golden_hashes.py"
_spec = importlib.util.spec_from_file_location("compute_golden_hashes", _SCRIPT_PATH)
assert _spec and _spec.loader
compute_golden_hashes = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(compute_golden_hashes)  # type: ignore[misc]


def _write_truncated_wav(path: Path) -> None:
    # Minimal RIFF header with an incomplete fmt chunk so wave.open() chokes.
    payload = b"RIFF" + b"\x10\x00\x00\x00" + b"WAVEfmt " + b"\x10\x00\x00"
    path.write_bytes(payload)


def _write_data_chunk_with_mismatch(path: Path) -> bytes:
    fmt_chunk = (
        b"fmt "
        + (16).to_bytes(4, "little")
        + b"\x01\x00"  # PCM
        + b"\x01\x00"  # mono
        + (48000).to_bytes(4, "little")
        + (48000 * 2).to_bytes(4, "little")
        + (2).to_bytes(2, "little")
        + (16).to_bytes(2, "little")
    )
    payload = b"\x00\x01\x02\x03"
    declared = len(payload) + 8  # lie so the helper would need to clamp
    data_chunk = b"data" + declared.to_bytes(4, "little") + payload
    riff_size = 4 + len(fmt_chunk) + len(data_chunk)
    blob = b"RIFF" + riff_size.to_bytes(4, "little") + b"WAVE" + fmt_chunk + data_chunk
    path.write_bytes(blob)
    return payload


def test_read_pcm_strict_mode_raises_on_wave_failure(tmp_path: Path) -> None:
    broken = tmp_path / "broken.wav"
    _write_truncated_wav(broken)
    with pytest.raises(ValueError) as excinfo:
        compute_golden_hashes._read_pcm(broken, allow_salvage=False)
    assert "allow-salvage" in str(excinfo.value)


def test_scan_data_chunk_refuses_to_clamp_without_salvage(tmp_path: Path) -> None:
    mangled = tmp_path / "mangled.wav"
    payload = _write_data_chunk_with_mismatch(mangled)
    blob = mangled.read_bytes()

    with pytest.raises(ValueError):
        compute_golden_hashes._scan_data_chunk(blob, mangled, allow_salvage=False)

    salvaged = compute_golden_hashes._scan_data_chunk(blob, mangled, allow_salvage=True)
    assert salvaged == payload
