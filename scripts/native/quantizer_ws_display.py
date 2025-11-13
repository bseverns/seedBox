#!/usr/bin/env python3
"""Minimal WebSocket sink that mirrors quantizer frames to the console.

The C++ harness acts as a WebSocket client. Launch this script first and then
point the harness at ``ws://127.0.0.1:8765/quantizer``.
"""


import argparse
import base64
import hashlib
import json
import socket
import struct
import sys
import threading
from typing import Dict

_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def _handshake(connection: socket.socket) -> None:
    request = connection.recv(1024).decode("utf-8", errors="ignore")
    headers = {}
    for line in request.split("\r\n"):
        if ":" in line:
            key, value = line.split(":", 1)
            headers[key.strip().lower()] = value.strip()
    key = headers.get("sec-websocket-key")
    if not key:
        raise RuntimeError("websocket client did not send Sec-WebSocket-Key")
    accept = base64.b64encode(hashlib.sha1((key + _GUID).encode("utf-8")).digest())
    response = (
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        f"Sec-WebSocket-Accept: {accept.decode('utf-8')}\r\n\r\n"
    )
    connection.sendall(response.encode("utf-8"))


def _recv_exact(connection: socket.socket, size: int) -> bytes:
    data = b""
    while len(data) < size:
        chunk = connection.recv(size - len(data))
        if not chunk:
            raise ConnectionError("socket closed during frame read")
        data += chunk
    return data


def _read_frame(connection: socket.socket) -> str:
    header = _recv_exact(connection, 2)
    opcode = header[0] & 0x0F
    masked = header[1] & 0x80
    length = header[1] & 0x7F
    if opcode == 0x8:
        raise ConnectionError("websocket client closed the connection")
    if length == 126:
        length = struct.unpack(">H", _recv_exact(connection, 2))[0]
    elif length == 127:
        length = struct.unpack(">Q", _recv_exact(connection, 8))[0]
    mask = b""
    if masked:
        mask = _recv_exact(connection, 4)
    payload = bytearray(_recv_exact(connection, length))
    if masked:
        for idx in range(length):
            payload[idx] ^= mask[idx % 4]
    return payload.decode("utf-8")


def _serve_once(connection: socket.socket, address: str) -> None:
    try:
        _handshake(connection)
        slots: Dict[int, Dict[str, float]] = {}
        print(f"[quantizer-ws] client connected from {address}")
        while True:
            message = _read_frame(connection)
            event = json.loads(message)
            slots[event["slot"]] = event
            _render(slots)
    except ConnectionError:
        print("[quantizer-ws] client closed the connection")
    except Exception as exc:  # pragma: no cover - diagnostic only
        print(f"[quantizer-ws] error: {exc}")
    finally:
        connection.close()


def _render(slots: Dict[int, Dict[str, float]]) -> None:
    if not slots:
        return
    sys.stdout.write("\033[2J\033[H")
    print("quantizer websocket feed -> ui ghost")
    print("slot | time  | drifted | nearest | up | down | active | mode")
    print("-----+-------+---------+---------+----+------+--------+------")
    for slot in sorted(slots):
        frame = slots[slot]
        print(
            f"{slot:>4} | {frame['time']:>5.2f} | {frame['drifted']:>7.3f} | "
            f"{frame['nearest']:>7.3f} | {frame['up']:>4.1f} | {frame['down']:>6.2f} | "
            f"{frame['active']:>6.3f} | {frame['mode']}"
        )
    sys.stdout.flush()


def main() -> None:
    parser = argparse.ArgumentParser(description="Simple websocket UI sink")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", args.port))
    server.listen(1)
    print(f"[quantizer-ws] listening on ws://127.0.0.1:{args.port}/quantizer")
    try:
        while True:
            conn, addr = server.accept()
            thread = threading.Thread(target=_serve_once, args=(conn, addr[0]), daemon=True)
            thread.start()
    except KeyboardInterrupt:
        print("\n[quantizer-ws] shutting down")
    finally:
        server.close()


if __name__ == "__main__":
    main()
