#!/usr/bin/env python3
"""Validate an audio_controller STM32F103RCT6 .bin and emit a frozen manifest.

This tool is intentionally non-destructive.  It only reads a local firmware
image, validates the vector table/range against the current linker layout, and
prints/writes metadata that a later remote flasher can require verbatim.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import struct
import sys

FLASH_BASE = 0x08000000
FLASH_SIZE = 256 * 1024
FLASH_END = FLASH_BASE + FLASH_SIZE
FLASH_PAGE_SIZE = 2048

SRAM_BASE = 0x20000000
SRAM_SIZE = 48 * 1024
SRAM_END = SRAM_BASE + SRAM_SIZE

EXPECTED_PID = 0x0414
EXPECTED_FLASH_KB = 256
TARGET = "STM32F103RCT6"
BOARD = "audio_controller_rct6"


def fail(message: str) -> "NoReturn":
    raise ValueError(message)


def validate_image(data: bytes) -> dict:
    if len(data) < 8:
        fail("image is too short to contain the initial MSP and reset vector")
    if len(data) > FLASH_SIZE:
        fail(f"image is {len(data)} bytes; maximum is {FLASH_SIZE}")

    initial_msp, reset_vector = struct.unpack_from("<II", data, 0)

    if initial_msp & 0x3:
        fail(f"initial MSP 0x{initial_msp:08X} is not word-aligned")
    if not (SRAM_BASE <= initial_msp <= SRAM_END):
        fail(
            f"initial MSP 0x{initial_msp:08X} is outside expected SRAM "
            f"0x{SRAM_BASE:08X}..0x{SRAM_END:08X}"
        )

    if (reset_vector & 1) == 0:
        fail(f"reset vector 0x{reset_vector:08X} does not have the Thumb bit set")

    reset_address = reset_vector & ~1
    image_end = FLASH_BASE + len(data)
    if not (FLASH_BASE <= reset_address < image_end):
        fail(
            f"reset handler 0x{reset_address:08X} is outside candidate image "
            f"0x{FLASH_BASE:08X}..0x{image_end - 1:08X}"
        )

    last_byte = image_end - 1
    first_page = 0
    last_page = (last_byte - FLASH_BASE) // FLASH_PAGE_SIZE
    page_count = last_page - first_page + 1

    pages = []
    for page_index in range(first_page, last_page + 1):
        start = page_index * FLASH_PAGE_SIZE
        end = min(start + FLASH_PAGE_SIZE, len(data))
        page = data[start:end]
        pages.append({
            "index": page_index,
            "address": f"0x{FLASH_BASE + start:08X}",
            "size": len(page),
            "sha256": hashlib.sha256(page).hexdigest(),
        })

    manifest = {
        "format": 1,
        "target": TARGET,
        "board": BOARD,
        "rom_product_id": f"0x{EXPECTED_PID:04X}",
        "flash_kb": EXPECTED_FLASH_KB,
        "flash_base": f"0x{FLASH_BASE:08X}",
        "image_size": len(data),
        "image_end_exclusive": f"0x{image_end:08X}",
        "sha256": hashlib.sha256(data).hexdigest(),
        "initial_msp": f"0x{initial_msp:08X}",
        "reset_vector": f"0x{reset_vector:08X}",
        "reset_handler": f"0x{reset_address:08X}",
        "flash_page_size": FLASH_PAGE_SIZE,
        "first_page": first_page,
        "last_page": last_page,
        "page_count": page_count,
        "pages": pages,
    }

    canonical = json.dumps(
        manifest, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    manifest["manifest_sha256"] = hashlib.sha256(canonical).hexdigest()
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate STM32F103RCT6 audio_controller binary and emit JSON manifest"
    )
    parser.add_argument("image", type=pathlib.Path, help="raw .bin firmware image")
    parser.add_argument(
        "-o", "--output", type=pathlib.Path,
        help="write manifest JSON to this path instead of stdout"
    )
    args = parser.parse_args()

    try:
        data = args.image.read_bytes()
        manifest = validate_image(data)
    except (OSError, ValueError) as exc:
        print(f"IMAGE_REJECTED: {exc}", file=sys.stderr)
        return 2

    manifest["image_file"] = args.image.name
    text = json.dumps(manifest, indent=2, sort_keys=True) + "\n"

    if args.output:
        try:
            args.output.write_text(text, encoding="utf-8")
        except OSError as exc:
            print(f"cannot write manifest: {exc}", file=sys.stderr)
            return 3
        print(
            f"IMAGE_VALIDATED target={TARGET} size={manifest['image_size']} "
            f"sha256={manifest['sha256']} pages={manifest['page_count']} "
            f"manifest={args.output}"
        )
    else:
        sys.stdout.write(text)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
