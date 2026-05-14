#!/usr/bin/env python3
"""Serve the golden fixture browser with a local-only tone-generation API."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import threading
from functools import partial
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple
from urllib.parse import urlparse


REPO_ROOT = Path(__file__).resolve().parent.parent
PIPELINE_LOCK = threading.Lock()


def _json_bytes(payload: Dict[str, Any]) -> bytes:
    return json.dumps(payload, indent=2).encode("utf-8")


def _write_json(handler: SimpleHTTPRequestHandler, status: int, payload: Dict[str, Any]) -> None:
    body = _json_bytes(payload)
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Content-Length", str(len(body)))
    handler.end_headers()
    handler.wfile.write(body)


def _read_json_body(handler: SimpleHTTPRequestHandler) -> Dict[str, Any]:
    length_header = handler.headers.get("Content-Length", "")
    try:
        length = int(length_header)
    except ValueError as exc:
        raise ValueError("Invalid Content-Length header") from exc
    raw = handler.rfile.read(length)
    try:
        payload = json.loads(raw.decode("utf-8"))
    except json.JSONDecodeError as exc:
        raise ValueError("Request body must be valid JSON") from exc
    if not isinstance(payload, dict):
        raise ValueError("Request JSON must be an object")
    return payload


def _optional_string(payload: Dict[str, Any], key: str) -> Optional[str]:
    value = payload.get(key)
    if value is None:
        return None
    if not isinstance(value, str):
        raise ValueError(f"'{key}' must be a string when provided")
    trimmed = value.strip()
    return trimmed or None


def _required_float(payload: Dict[str, Any], key: str, minimum: float, maximum: float) -> float:
    value = payload.get(key)
    if not isinstance(value, (int, float)):
        raise ValueError(f"'{key}' must be numeric")
    numeric = float(value)
    if not (minimum < numeric <= maximum):
        raise ValueError(f"'{key}' must land in ({minimum}, {maximum}]")
    return numeric


def _build_tone_spec(payload: Dict[str, Any]) -> Tuple[str, Dict[str, Any]]:
    name = _optional_string(payload, "name")
    frequency_hz = _required_float(payload, "frequency_hz", 0.0, 24000.0)
    amplitude = _required_float(payload, "amplitude", 0.0, 1.0)
    duration_seconds = _required_float(payload, "duration_seconds", 0.0, 60.0)

    core = f"{frequency_hz:g}:{amplitude:g}:{duration_seconds:g}"
    spec = f"{name}={core}" if name else core
    return spec, {
        "name": name,
        "frequency_hz": frequency_hz,
        "amplitude": amplitude,
        "duration_seconds": duration_seconds,
    }


def _run_input_tone_pipeline(spec: str) -> subprocess.CompletedProcess[str]:
    command: List[str] = [
        str(REPO_ROOT / "scripts" / "offline_native_golden.sh"),
        "--filter",
        "input-tone",
        "--input-tone",
        spec,
    ]
    return subprocess.run(
        command,
        cwd=str(REPO_ROOT),
        capture_output=True,
        text=True,
        check=False,
        env=os.environ.copy(),
    )


class GoldenFixtureHandler(SimpleHTTPRequestHandler):
    server_version = "SeedBoxGoldenServer/0.1"

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path == "/":
            self.send_response(HTTPStatus.FOUND)
            self.send_header("Location", "/build/fixtures/")
            self.end_headers()
            return
        super().do_GET()

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path != "/api/input-tone":
            _write_json(self, HTTPStatus.NOT_FOUND, {"ok": False, "error": "Unknown API route"})
            return

        try:
            payload = _read_json_body(self)
            spec, normalized = _build_tone_spec(payload)
        except ValueError as exc:
            _write_json(self, HTTPStatus.BAD_REQUEST, {"ok": False, "error": str(exc)})
            return

        if not PIPELINE_LOCK.acquire(blocking=False):
            _write_json(
                self,
                HTTPStatus.CONFLICT,
                {"ok": False, "error": "Golden pipeline already running; wait for the current render to finish."},
            )
            return

        try:
            result = _run_input_tone_pipeline(spec)
        finally:
            PIPELINE_LOCK.release()

        response = {
            "ok": result.returncode == 0,
            "request": normalized,
            "spec": spec,
            "stdout": result.stdout,
            "stderr": result.stderr,
            "browser_url": "/build/fixtures/",
        }
        if result.returncode != 0:
            _write_json(self, HTTPStatus.INTERNAL_SERVER_ERROR, response)
            return

        _write_json(self, HTTPStatus.OK, response)

    def log_message(self, format: str, *args: Any) -> None:
        sys.stdout.write("%s - - [%s] %s\n" % (self.address_string(), self.log_date_time_string(), format % args))


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1", help="Interface to bind (default: 127.0.0.1)")
    parser.add_argument(
        "--port",
        default=0,
        type=int,
        help="Port to bind. Use 0 to let the OS choose a free port (default: 0).",
    )
    args = parser.parse_args(argv)

    handler = partial(GoldenFixtureHandler, directory=str(REPO_ROOT))
    with ThreadingHTTPServer((args.host, args.port), handler) as server:
        bound_host, bound_port = server.server_address[:2]
        print(f"Golden fixture browser: http://{bound_host}:{bound_port}/build/fixtures/")
        print("Tone API: POST /api/input-tone")
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down golden fixture browser server.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
