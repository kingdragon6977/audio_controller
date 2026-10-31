# Coordinated STM32/ESP OTA

The explicit `--flash` path validates the manifest, obtains STM32 authorization,
checks ROM identity/capabilities, erases individual pages, writes bounded blocks,
and verifies pages plus the complete image. The host then confirms application
and PCM recovery before reporting `FLASH_COMPLETE`.

This integration is build/test verified and deployed on 2026-10-12. Automatic
flashing from PA0 OFF, complete readback, application-health/PCM confirmation,
and completed-request retries passed on hardware. Electrical expiry/timing and
failure-injection checks remain pending; see the project context for evidence
and the separate reduced UDP throughput observation.

## Authorization protocol

After host and ESP manifest validation, ESP sends USART1 command `0xF4` followed
by a 40-byte request and a two-byte little-endian CRC-CCITT-FALSE:

| Offset | Meaning |
| --- | --- |
| 0 | 32-bit little-endian nonzero session |
| 4 | 32-bit little-endian image size, 8 through 262,144 |
| 8 | 32-byte canonical manifest SHA-256 |
| 40 | CRC over the preceding 40 bytes |

STM32 stops PCM and accepts the frame within 100 ms. It requires PB2/BOOT1
output/input LOW, grants PA0 LOW for two seconds, verifies readback, and replies
with the existing magic, status, protocol `2`, `RCT6`, and echoed session.
ESP paces bytes for the polled STM32 UART. It repeats the same request after
250 ms and requires another matching ACK before BOOT0 precharge/reset.

The second check cannot renew the lease or change the request. Invalid framing,
CRC, request or pin checks release authorization; READY/STOP also release it.
The 1 ms SysTick interrupt releases PA0 on expiry even when the main loop is
busy. Reset releases PA0; the measured BOOT0 RC hold spans ROM boot selection.
ROM retries require the session and canonical manifest authorized at entry.

The canonical manifest binds target, full image hash, and page hashes. STM32
freezes the opaque digest; it does not inspect all host image bytes. CRC/hash
binding protects against transfer mistakes, not unauthorized network peers or
unsigned firmware. No automatic legacy BOOT_PREP fallback remains in flashing.
Read-only BOOT/GATE bench tests retain manual authorization controls.

## Completion and recovery

FLASH_FINISH verifies the entire installed image, stores completed state,
acknowledges success, then resets STM32. Identical retries do not reset again.
Host prints `FLASH_IMAGE_VERIFIED`, then polls binary type `11`
(`APPLICATION_STATUS`) for up to 60 seconds.

ESP requires parsed PCM since reset, no older than two seconds, and an STM32
health ACK to command `0xF5`. STM32 reports PASS only when streaming is active
and PA0 output/input are both HIGH. Health uses protocol `1` and target `RCT6`.
Only then does the host print
`FLASH_COMPLETE ... application=PASS pcm=PASS bootauth=OFF`. Timeout reports
verified programming with unconfirmed application recovery; it never reflashes.
Health covers streaming and gate release, not audio calibration or all sensors.
ROM readback establishes the installed image identity.

ESP ArduinoOTA keeps STOP/READY coordination and is suspended in STM32 ROM mode.
Failed programming leaves STM32 in ROM rather than starting a partial image.

## One-time migration

For boards running pre-integration STM32 firmware without `0xF4`/`0xF5`, install
the new STM32 **before** the new
ESP, using the currently deployed ESP and the host script from commit `a682235`
with explicit manual `bootauth on`. Use the new image/manifest for that bootstrap
flash. Verify independent readback and application recovery, then upload the
new ESP. Later updates use automatic authorization.

Preserve the previously verified image/manifest; build the new image separately:

```bash
make -j2 BUILD=build/ota-integration
make manifest size BUILD=build/ota-integration
make ota-test BUILD=build/ota-integration
python3 tools/stage_stm32_update.py 192.168.114.54 \
  build/ota-integration/audio_controller.bin \
  build/ota-integration/audio_controller.manifest.json --flash --dry-run
```

After migration, removing `--dry-run` requests an actual flash. This board's
migration and subsequent automatic flash were completed on 2026-10-12.

Bench acceptance still needs malformed/truncated request rejection, measured
PA0 expiry during a busy main loop, settled grant/reset timing, complete automatic
update, lost completion ACK retry, application startup failure, independent
readback, and continuing PCM with PA0 released.

## Receive timing hardening (2026-10-31)

ESP now waits 10 ms after F4 before sending payload byte zero. The previous
2 ms wait was shorter than the approximately 2.76 ms required to send a PCM
half-buffer; two failed authorization attempts each added one STM32 ORE
and released PA0 before erase/programming. A 10 ms guard plus the paced
42-byte frame remains inside the application's 100 ms framing budget under
normal servicing. Long unrelated foreground blocking can still cause a safe
authorization rejection; this change does not weaken CRC/session/lease gates.

STM32 boot now drains USART1 during UART2 startup output, traces errors and
coalesces only idempotent READY bytes until initialization completes. Other
startup bytes are counted/traced and discarded, never deferred for boot
authorization. Normal protocol parsing stays in the main task.
`esp rxtrace` exposes startup status checkpoints, physical RX byte timing,
first errored byte and startup READY/ignored counts. Genuine errors are still
counted; no counter-clearing workaround is used.

ESP normal-loop serial parsing is bounded to 512 bytes per iteration so
continuous PCM cannot indefinitely bypass control/OTA handling. Broadcast
delivery/latency remains less reliable on this LAN; use STREAM_SUBSCRIBE
for measurement. During investigation an unbounded-loop image required a
temporary `i2s meter 10` pause to regain OTA servicing and install the bound.
