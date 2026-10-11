#!/usr/bin/env python3
import hashlib
import pathlib
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import stm32_image_manifest as manifest


def make_image(size=64, msp=None, reset=None):
    if msp is None:
        msp = manifest.SRAM_END
    if reset is None:
        reset = (manifest.FLASH_BASE + 8) | 1
    data = bytearray([0xFF] * size)
    struct.pack_into("<II", data, 0, msp, reset)
    return bytes(data)


class ImageManifestTests(unittest.TestCase):
    def test_valid_image(self):
        result = manifest.validate_image(make_image())
        self.assertEqual(result["target"], "STM32F103RCT6")
        self.assertEqual(result["rom_product_id"], "0x0414")
        self.assertEqual(result["flash_kb"], 256)
        self.assertEqual(result["page_count"], 1)
        self.assertEqual(len(result["pages"]), 1)
        self.assertEqual(result["pages"][0]["index"], 0)
        self.assertEqual(result["pages"][0]["size"], 64)
        self.assertEqual(len(result["sha256"]), 64)
        self.assertEqual(len(result["manifest_sha256"]), 64)
        self.assertEqual(len(result["transport_manifest_sha256"]), 64)

        canonical = (
            struct.pack(
                "<IHHHI",
                result["image_size"],
                result["page_count"],
                manifest.EXPECTED_PID,
                manifest.EXPECTED_FLASH_KB,
                manifest.FLASH_BASE,
            )
            + bytes.fromhex(result["sha256"])
            + b"".join(bytes.fromhex(p["sha256"]) for p in result["pages"])
        )
        self.assertEqual(
            result["transport_manifest_sha256"],
            hashlib.sha256(canonical).hexdigest(),
        )

    def test_rejects_unaligned_msp(self):
        with self.assertRaises(ValueError):
            manifest.validate_image(make_image(msp=manifest.SRAM_BASE + 1))

    def test_rejects_msp_outside_sram(self):
        with self.assertRaises(ValueError):
            manifest.validate_image(make_image(msp=manifest.SRAM_BASE - 4))

    def test_rejects_arm_state_reset_vector(self):
        with self.assertRaises(ValueError):
            manifest.validate_image(make_image(reset=manifest.FLASH_BASE + 8))

    def test_rejects_reset_handler_outside_candidate(self):
        with self.assertRaises(ValueError):
            manifest.validate_image(
                make_image(reset=(manifest.FLASH_BASE + 0x1000) | 1)
            )

    def test_rejects_oversize_image(self):
        data = bytearray(make_image(size=manifest.FLASH_SIZE + 1))
        with self.assertRaises(ValueError):
            manifest.validate_image(bytes(data))

    def test_page_hashes_cover_exact_image_bytes(self):
        size = manifest.FLASH_PAGE_SIZE + 17
        result = manifest.validate_image(make_image(size=size))
        self.assertEqual(result["page_count"], 2)
        self.assertEqual(result["pages"][0]["size"], manifest.FLASH_PAGE_SIZE)
        self.assertEqual(result["pages"][1]["size"], 17)
        self.assertEqual(result["pages"][1]["index"], 1)


if __name__ == "__main__":
    unittest.main()
