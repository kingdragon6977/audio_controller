#!/usr/bin/env python3
"""Receive raw PCM, optionally subscribing to ESP unicast delivery."""
import argparse
import socket
import sys
import time

PACKET_BYTES = 512


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--esp', help='ESP IP/hostname; subscribe to unicast and poll counters')
    parser.add_argument('--port', type=int, default=5004)
    parser.add_argument('--duration', type=float, help='stop after this many seconds')
    parser.add_argument('--discard', action='store_true', help='measure delivery without writing PCM')
    args = parser.parse_args()
    if args.duration is not None and args.duration <= 0:
        parser.error('--duration must be positive')
    peer = (socket.gethostbyname(args.esp), args.port) if args.esp else None
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', args.port))
    sock.settimeout(0.2)
    start = report_time = time.monotonic()
    next_request = start
    received = interval_packets = 0
    subscribed = False
    print(f'Listening on UDP {args.port}; PCM goes to stdout, diagnostics to stderr.', file=sys.stderr)
    if not peer:
        print('Use --esp ESP_IP for unicast delivery and ESP counters.', file=sys.stderr)
    try:
        while args.duration is None or time.monotonic()-start < args.duration:
            now = time.monotonic()
            if peer and now >= next_request:
                sock.sendto(b'STREAM_SUBSCRIBE\n', peer)
                sock.sendto(b'STREAM_STATUS\n', peer)
                next_request = now + (5.0 if subscribed else 1.0)
            try:
                data, addr = sock.recvfrom(4096)
            except socket.timeout:
                data = b''
                addr = None
            if addr and (not peer or addr[0] == peer[0]):
                if len(data) == PACKET_BYTES:
                    received += 1
                    interval_packets += 1
                    if not args.discard:
                        sys.stdout.buffer.write(data)
                        sys.stdout.buffer.flush()
                elif data.startswith((b'ESP01_HEARTBEAT ', b'STREAM_STATUS ', b'STREAM_SUBSCRIBED ')):
                    if data.startswith(b'STREAM_SUBSCRIBED '):
                        subscribed = True
                    print(data.decode('ascii', errors='replace').strip(), file=sys.stderr)
                elif data:
                    print(f'Ignored {len(data)}-byte datagram from {addr[0]}', file=sys.stderr)
            now = time.monotonic()
            if now-report_time >= 5.0:
                print(f'PC_RECEIVE packets={received} interval_pps={interval_packets/(now-report_time):.2f}', file=sys.stderr)
                report_time = now
                interval_packets = 0
    except KeyboardInterrupt:
        pass
    finally:
        elapsed = time.monotonic()-start
        print(f'PC_RECEIVE total={received} elapsed_s={elapsed:.3f} average_pps={received/elapsed:.2f}', file=sys.stderr)
        sock.close()


if __name__ == '__main__':
    main()
