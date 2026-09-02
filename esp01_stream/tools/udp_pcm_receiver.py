#!/usr/bin/env python3
import socket
import sys

PORT = 5004
PACKET_BYTES = 512

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", PORT))

print(f"Listening for 24 kHz mono PCM16LE UDP on port {PORT}", file=sys.stderr)
print("Writing raw PCM to stdout; pipe into ffplay/aplay.", file=sys.stderr)

while True:
    data, addr = sock.recvfrom(2048)
    if len(data) != PACKET_BYTES:
        print(f"warning: {addr[0]} sent {len(data)} bytes (expected {PACKET_BYTES})", file=sys.stderr)
        continue
    sys.stdout.buffer.write(data)
    sys.stdout.buffer.flush()
