#!/usr/bin/env python3
import socket
import sys

PORT = 5004
PACKET_BYTES = 512
HEARTBEAT_PREFIX = b"ESP01_HEARTBEAT "

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", PORT))

print(f"Listening for ESP-01 UDP on port {PORT}", file=sys.stderr)
print("PCM: 24 kHz mono PCM16LE; heartbeats print to stderr.", file=sys.stderr)

try:
    while True:
        data, addr = sock.recvfrom(2048)

        if data.startswith(HEARTBEAT_PREFIX):
            try:
                text = data.decode("ascii", errors="replace").strip()
            except Exception:
                text = repr(data)
            print(f"heartbeat from {addr[0]}: {text}", file=sys.stderr)
            continue

        if len(data) != PACKET_BYTES:
            print(f"warning: {addr[0]} sent {len(data)} bytes (expected {PACKET_BYTES})", file=sys.stderr)
            continue

        sys.stdout.buffer.write(data)
        sys.stdout.buffer.flush()
except KeyboardInterrupt:
    print("\nStopped.", file=sys.stderr)
