#!/usr/bin/env python3
"""Stage a validated STM32 image through the ESP-01 without touching STM32 flash.

The ESP accepts a manifest, verifies the canonical manifest digest, then receives
one 2 KiB page at a time in ordered <=256-byte chunks and verifies each page
locally with SHA-256.  The current ESP transport intentionally stops at
STAGED_VERIFIED and has no erase/write call path.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import socket
import struct
import sys
import time

MAGIC = b"S32U"
VERSION = 1

PKT_MANIFEST_BEGIN = 1
PKT_PAGE_HASH = 2
PKT_MANIFEST_COMMIT = 3
PKT_PAGE_BEGIN = 4
PKT_PAGE_DATA = 5
PKT_PAGE_SEAL = 6
PKT_ABORT = 7
PKT_STATUS = 8

REPLY_NAMES = {
    0: "OK",
    1: "BAD_PACKET",
    2: "BAD_STATE",
    3: "BAD_SESSION",
    4: "BAD_RANGE",
    5: "BAD_HASH",
    6: "INCOMPLETE",
}

PAGE_SIZE = 2048
CHUNK_SIZE = 256
EXPECTED_PID = 0x0414
EXPECTED_FLASH_KB = 256
FLASH_BASE = 0x08000000


def header(packet_type: int, session: int) -> bytes:
    return struct.pack("<4sBBBBI", MAGIC, VERSION, packet_type, 0, 0, session)


def parse_status(data: bytes, expected_type: int, expected_session: int):
    if len(data) != 18:
        raise RuntimeError(f"bad status length {len(data)}")
    magic, version, packet_type, request_type, code, session, arg, hashes, received = (
        struct.unpack("<4sBBBBIHHH", data)
    )
    if magic != MAGIC or version != VERSION or packet_type != PKT_STATUS:
        raise RuntimeError("invalid status packet")
    if request_type != expected_type:
        raise RuntimeError(
            f"status is for request {request_type}, expected {expected_type}"
        )
    if session != expected_session:
        raise RuntimeError(
            f"status session 0x{session:08X} != expected 0x{expected_session:08X}"
        )
    return code, arg, hashes, received


def transact(sock, target, packet_type, session, payload=b"", retries=5, timeout=0.8):
    packet = header(packet_type, session) + payload
    last_error = None
    for attempt in range(1, retries + 1):
        sock.sendto(packet, target)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                data, peer = sock.recvfrom(512)
            except socket.timeout:
                break
            if peer[0] != target[0] and target[0] not in ("255.255.255.255", "<broadcast>"):
                continue
            try:
                status = parse_status(data, packet_type, session)
            except RuntimeError as exc:
                last_error = exc
                continue
            return status
        last_error = TimeoutError(f"no valid ACK on attempt {attempt}/{retries}")
    raise RuntimeError(str(last_error))


def require_ok(status, label):
    code, arg, hashes, received = status
    if code != 0:
        raise RuntimeError(
            f"{label}: {REPLY_NAMES.get(code, 'CODE_'+str(code))} "
            f"arg={arg} hashes={hashes} received={received}"
        )
    return arg, hashes, received


def load_manifest(path: pathlib.Path) -> dict:
    m = json.loads(path.read_text(encoding="utf-8"))
    required = [
        "target", "rom_product_id", "flash_kb", "image_size", "sha256",
        "page_count", "pages", "transport_manifest_sha256"
    ]
    missing = [k for k in required if k not in m]
    if missing:
        raise ValueError(f"manifest missing fields: {', '.join(missing)}")

    if m["target"] != "STM32F103RCT6":
        raise ValueError(f"wrong target {m['target']!r}")
    if m["rom_product_id"].lower() != "0x0414":
        raise ValueError(f"wrong product id {m['rom_product_id']!r}")
    if int(m["flash_kb"]) != 256:
        raise ValueError(f"wrong flash size {m['flash_kb']!r}")
    if int(m["page_count"]) != len(m["pages"]):
        raise ValueError("page_count does not match page list")

    return m


def canonical_transport_hash(m: dict) -> bytes:
    image_hash = bytes.fromhex(m["sha256"])
    page_hashes = b"".join(bytes.fromhex(p["sha256"]) for p in m["pages"])
    canonical = struct.pack(
        "<IHHHI",
        int(m["image_size"]),
        int(m["page_count"]),
        EXPECTED_PID,
        EXPECTED_FLASH_KB,
        FLASH_BASE,
    )
    canonical += image_hash + page_hashes
    return hashlib.sha256(canonical).digest()


def stage(args) -> int:
    manifest = load_manifest(args.manifest)
    image = args.image.read_bytes()

    if len(image) != int(manifest["image_size"]):
        raise ValueError(
            f"image size {len(image)} != manifest {manifest['image_size']}"
        )

    image_hash = hashlib.sha256(image).hexdigest()
    if image_hash != manifest["sha256"]:
        raise ValueError("image SHA-256 does not match manifest")

    transport_hash = canonical_transport_hash(manifest)
    if transport_hash.hex() != manifest["transport_manifest_sha256"]:
        raise ValueError("transport manifest SHA-256 does not match manifest contents")

    session = int.from_bytes(transport_hash[:4], "little")
    if session == 0:
        session = 1

    if args.dry_run:
        print(
            f"DRY_RUN_OK session=0x{session:08X} image={len(image)} "
            f"pages={manifest['page_count']} sha256={image_hash}"
        )
        return 0

    resolved_host = socket.gethostbyname(args.host)
    target = (resolved_host, args.port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(args.timeout)

    begin_payload = (
        struct.pack("<IH", len(image), int(manifest["page_count"]))
        + b"\x00\x00"
        + bytes.fromhex(manifest["sha256"])
        + transport_hash
    )
    require_ok(
        transact(sock, target, PKT_MANIFEST_BEGIN, session, begin_payload,
                 args.retries, args.timeout),
        "MANIFEST_BEGIN",
    )

    for page in manifest["pages"]:
        index = int(page["index"])
        payload = struct.pack("<H", index) + bytes.fromhex(page["sha256"])
        require_ok(
            transact(sock, target, PKT_PAGE_HASH, session, payload,
                     args.retries, args.timeout),
            f"PAGE_HASH[{index}]",
        )

    require_ok(
        transact(sock, target, PKT_MANIFEST_COMMIT, session, b"",
                 args.retries, args.timeout),
        "MANIFEST_COMMIT",
    )

    print(
        f"MANIFEST_ACCEPTED session=0x{session:08X} "
        f"pages={manifest['page_count']}"
    )

    pages_to_send = manifest["pages"]
    if args.page is not None:
        if args.page < 0 or args.page >= len(pages_to_send):
            raise ValueError("--page outside manifest range")
        pages_to_send = [pages_to_send[args.page]]

    for page in pages_to_send:
        index = int(page["index"])
        start = index * PAGE_SIZE
        length = int(page["size"])
        page_data = image[start:start + length]

        if hashlib.sha256(page_data).hexdigest() != page["sha256"]:
            raise ValueError(f"local page {index} hash mismatch before send")

        require_ok(
            transact(
                sock, target, PKT_PAGE_BEGIN, session,
                struct.pack("<HH", index, length),
                args.retries, args.timeout,
            ),
            f"PAGE_BEGIN[{index}]",
        )

        offset = 0
        while offset < length:
            chunk = page_data[offset:offset + CHUNK_SIZE]
            payload = struct.pack("<HH", index, offset) + chunk
            require_ok(
                transact(
                    sock, target, PKT_PAGE_DATA, session, payload,
                    args.retries, args.timeout,
                ),
                f"PAGE_DATA[{index}@{offset}]",
            )
            offset += len(chunk)

        require_ok(
            transact(
                sock, target, PKT_PAGE_SEAL, session,
                struct.pack("<H", index),
                args.retries, args.timeout,
            ),
            f"PAGE_SEAL[{index}]",
        )
        print(
            f"STAGED_VERIFIED page={index} size={length} "
            f"sha256={page['sha256']}"
        )

    return 0


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("host", help="ESP-01 IP address")
    p.add_argument("image", type=pathlib.Path)
    p.add_argument("manifest", type=pathlib.Path)
    p.add_argument("--port", type=int, default=5004)
    p.add_argument("--page", type=int, help="stage only one page")
    p.add_argument("--retries", type=int, default=5)
    p.add_argument("--timeout", type=float, default=0.8)
    p.add_argument("--dry-run", action="store_true")
    args = p.parse_args()

    try:
        return stage(args)
    except (OSError, ValueError, RuntimeError) as exc:
        print(f"STAGE_FAILED: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
