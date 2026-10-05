# Safe STM32F103RCT6 remote flashing procedure

This document defines the intended fail-closed update path for the audio controller's
STM32F103RCT6.  It deliberately separates **target identification**, **boot-mode
authorization**, **erase/write**, and **verification** so that a malformed or out-of-order
remote command cannot turn directly into a flash write.

## Fixed target profile

The current board/firmware target is:

- MCU family: STM32F103 high-density
- MCU: STM32F103RCT6
- ROM bootloader product ID: `0x0414`
- application flash: `0x08000000 .. 0x0803FFFF`
- expected flash-size register: `0x1FFFF7E0 == 256 KiB`
- high-density flash page size: 2048 bytes
- application vector table base: `0x08000000`
- SRAM used by the present linker script: `0x20000000 .. 0x2000BFFF`

A future board revision must get a different explicit target profile rather than silently
relaxing these checks.

## Authority split

The ESP-01 is the transport/programmer, but it must not be the sole authority that decides
whether the STM32 enters its ROM bootloader.

Normal application firmware must first approve the operation.  The planned external
BOOT0-verify circuit is the hardware enforcement for that approval.

The existing `CTRL_BOOT_PREP` / PB2 BOOT1-low check remains a prerequisite.  Once the new
BOOT0 permission circuit is installed, its proof must become an additional prerequisite
before the ESP is allowed to pulse NRST.

If any prerequisite is missing, timing out, contradictory, or unreadable, the operation
fails closed and no erase/write ROM command is sent.

## State machine

Only these transitions are legal:

```text
IDLE
  |
  | candidate image received
  v
IMAGE_VALIDATED
  |
  | running STM32 explicitly authorizes boot preparation
  v
BOOT_AUTHORIZED
  |
  | BOOT0 hardware permission observed + reset into ROM
  v
ROM_SYNCED
  |
  | GET + GETID + flash-size checks match the fixed target profile
  v
TARGET_IDENTIFIED
  |
  | destructive commit token still matches candidate hash/target/session
  v
ERASE
  v
WRITE
  v
READBACK_VERIFY
  |
  | exact byte-for-byte verification passed
  v
BOOT_APPLICATION
```

Any unexpected command, reset, changed image hash, changed target identity, Wi-Fi reconnect,
timeout, UART framing error, or boot-authorization loss returns to a non-destructive state.

## Candidate-image checks before reset

Before requesting bootloader entry, the candidate must pass all of the following:

1. Image length is non-zero and no larger than 256 KiB.
2. Destination is exactly `0x08000000` for the current non-offset linker layout.
3. `destination + image_length` does not exceed `0x08040000`.
4. The first word (initial MSP) is aligned and lies in STM32 SRAM:
   `0x20000000 <= MSP < 0x2000C000`.
5. The reset vector has the Thumb bit set.
6. After clearing the Thumb bit, the reset-vector address lies inside the candidate
   application image in flash.
7. A SHA-256 (or stronger) digest is calculated for the exact bytes that will be written.
8. The manifest contains a SHA-256 for every 2 KiB flash page (the final page may be shorter).
9. A canonical manifest SHA-256 covers the fixed target metadata and complete page-hash list.
10. The validated image digest, manifest digest, image length, target profile and update-session
    number are frozen into the destructive-operation authorization record.

A changed byte changes its page hash and the full-image digest.  A changed page list or target
field changes the manifest digest.  Any of those changes invalidates authorization.


## Low-memory transport strategy

The ESP-01 must not need to buffer the full STM32 image.  The intended transport is therefore
manifest-first and page-at-a-time:

1. The PC/laptop validates the complete `.bin` with `tools/stm32_image_manifest.py`.
2. It sends the complete manifest before STM32 reset or erase.
3. The ESP validates/fixes the session metadata and stores the page-hash list.  At the maximum
   256 KiB RCT6 image size there are 128 x 2 KiB pages, so raw SHA-256 page hashes require only
   4096 bytes plus small metadata.
4. The running STM32 grants boot preparation and the hardware BOOT0 authorization condition is
   verified.
5. The ESP enters the ROM bootloader and independently verifies ROM capabilities, PID and flash
   size.
6. Only then may a page transfer begin.
7. The ESP receives one complete page into a 2 KiB RAM buffer and hashes it **before erasing or
   writing that page**.
8. The received page hash must exactly equal the hash frozen in the original manifest.
9. Only a validated page buffer may be erased/programmed, in <=256-byte ROM write blocks.
10. After programming, the ESP reads the page back and hashes/compares it before advancing.

This arrangement prevents an out-of-order, truncated, stale, or changed network transfer from
being treated as the image that was authorized earlier.  The network sender never supplies a
free-form flash address during the destructive phase; page addresses are derived only from the
frozen manifest page index.

The full-image SHA-256 is checked again over read-back data after all pages pass their individual
verification.

## ROM target identification

After bootloader sync (`0x7F -> ACK 0x79`), the ESP must:

1. Run ROM `GET (0x00)` and record the advertised bootloader version and supported commands.
2. Require `READ MEMORY (0x11)` before using the ROM for identification.
3. Record whether `WRITE MEMORY (0x31)` and an erase command (`0x43` or `0x44`) are actually
   advertised; destructive flashing is not eligible unless both are present.
4. Run ROM `GET ID (0x02)`.
5. Require product ID `0x0414`.
6. Use ROM `READ MEMORY (0x11)` to read two bytes at `0x1FFFF7E0`.
7. Require a little-endian value of `256` KiB.
8. Optionally read the 96-bit unique-ID area and log it as extra evidence; it is not a
   substitute for product-ID/flash-size validation.

The ESP must state how it identified the target.  A write authorization is never represented
only as `safe=true`.

Example evidence:

```text
target_match=PASS
pid=0x0414
flash_kb=256
boot_version=0x..
image_base=0x08000000
image_size=...
image_sha256=...
```

## Erase strategy

Do not mass-erase by default.

For the RCT6 high-density part, use 2 KiB pages.  Calculate exactly which pages intersect
the candidate image and erase only those pages.  A mass erase may be implemented later as a
manual recovery-only operation and must require a separate explicit command.

Before sending any erase command, repeat the in-memory authorization checks:

- same session number
- same candidate digest
- same image length and base
- same detected PID
- same detected flash size

The first erase command is the destructive boundary.  Everything before it must remain
read-only.

## Write strategy

Use ROM `WRITE MEMORY (0x31)` in blocks no larger than 256 bytes.

For every block:

1. Confirm destination range is inside the validated image range.
2. Send address and address checksum.
3. Send block length/data/checksum.
4. Require ROM ACK.
5. Advance only by the number of bytes actually authorized for that image.

Never accept an address supplied independently by a later network command.  Addresses are
derived from the frozen candidate base plus the current verified offset.

## Read-back verification

After all writes, use ROM `READ MEMORY (0x11)` to read the programmed region back.

Verification is byte-for-byte against the frozen candidate.  Also recalculate the final
digest over read-back data and compare it with the pre-write digest.

Only after full verification passes may the ESP:

1. drive BOOT0 back to normal-application state,
2. reset the STM32,
3. wait for a recognizable application-start/identity message,
4. report success.

A ROM ACK to the final write is **not** success.

## Failure behavior

On any failure after bootloader entry:

- stop issuing erase/write commands,
- force BOOT0 toward normal application state,
- preserve/report the exact failure stage,
- do not claim that the application is valid unless read-back verification completed,
- do not automatically retry an erase or write with changed parameters.

Reports must distinguish at least:

```text
FAILED_BEFORE_ERASE
FAILED_DURING_ERASE
FAILED_DURING_WRITE offset=...
FAILED_VERIFY offset=...
FLASH_VERIFIED
```

## Existing implementation status

The current ESP firmware already provides a read-only ROM bootloader test:

- asks the running STM32 to stop PCM,
- requires PB2/BOOT1 low acknowledgment,
- repeats the PB2 test after the 250 ms settling interval,
- resets into system memory,
- syncs the ROM protocol,
- executes `GET`,
- executes `GET ID`,
- returns to the normal application.

Implemented groundwork now includes ROM read-memory, PID/flash-size verification, advertised
ROM command tracking, pre-reset application identity, and a manifest generator with whole-image,
per-page, and canonical-manifest SHA-256 values.

The next safe implementation steps are:

1. add the external BOOT0 authorization proof once the hardware is finalized;
2. implement the manifest-first/page-at-a-time transport without destructive commands;
3. bind the received manifest to an update session;
4. implement page erase behind the complete authorization gate;
5. implement <=256-byte writes from a pre-hashed 2 KiB page buffer;
6. implement per-page and final full-image read-back verification;
7. only then expose a destructive remote update command.

Until all prerequisites exist, `STM32_BOOT_TEST` remains read-only.
