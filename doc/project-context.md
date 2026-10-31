# Audio controller shared context

Snapshot: 2026-10-11. This is the common handoff for ChatGPT, local Codex,
and GitHub work. It is a curated snapshot, not automatic chat synchronization.

## Repository and current work

- Repository: https://github.com/kingdragon6977/audio_controller
- Local folder: `/home/blankone/projects/audio_controller`
- Local Codex project: `audio controller` (`c8e885e4-aa56-4796-a7c2-648da0257ad2`).
- Checked-out branch at collection: `stm32-safe-flash`, HEAD `805efe0`.
- Local tracking ref: `origin/stm32-safe-flash` at the same commit; remote refs
  were not fetched during this collection.
- Local `master` is at `e28c827`, reported 33 commits ahead of its tracking ref.
- Existing untracked captures, build outputs, and BOOT0 file were left untouched.

Primary technical references:

- [Remote flashing procedure](stm32-safe-flash.md)
- [ESP bridge, wiring, OTA, and boot tests](../esp01_stream/README.md)
- [Diagnostic and bring-up standard](../DIAGNOSTICS.md)
- Full local history: `git log --all --date=iso --format='%h %ad %d %s'`.

## ChatGPT source history

### Build Directional Microphone

Source: https://chatgpt.com/c/695b4b2e-6878-8333-9342-df38d526a5a2

Read the available recent turns through the desktop thread reader. The returned
page reported no older page; this does not establish a full original transcript.

The discussion records Phase 1A build modernization (`1baedbd`), moving the
status LED to PB2 (`09c955b`), and active-low LED handling (`73344b3`). Those
commits also exist in local Git history. The conversation's next proposed work
was 72 MHz initialization and resolving duplicated SPL locations.

Historical hardware was described as STM32F103VFT6/XL with PB2 LED wired toward
3.3 V. Do not reuse that pinout or polarity without checking current hardware.
Later history contains `43286e2` (PB2 active-high for RCT6) and `08b3280`
(keep PB2 power LED on after init). The current flashing documentation fixes
the target to STM32F103RCT6, PID `0x0414`, 256 KiB flash, 2 KiB pages.

### Connect GitHub Plugin

Source: https://chatgpt.com/share/6acbcf62-8da0-83e9-8193-ae5ef6a3239f

Read the rendered shared conversation on 2026-10-11. It contains firmware
bring-up, analog input observations, repaired PB13/PB14 solder joints, advancing
ESP PCM counters, successful ESP OTA output, and review of `7f3bc48`.
Uploaded-file contents are unavailable in the rendered share; these were not
imported or independently analyzed.

The user explicitly confirms that the ROM test metadata came from actual local
outputs and that both matching firmwares were flashed before testing. The user
designed the PB2/BOOT1 pre-check; Codex implemented it. Preserve that attribution.

Design intent: a mounted field unit should support independent Wi-Fi updates of
ESP networking and STM32 DSP/audio firmware without opening the unit or changing
programming jumpers. ESP uses its own OTA plus STM32 ROM programming on the shared
USART1; normal operation returns to 1 Mbaud PCM transport. Updates require PCM
stop, PB2/BOOT1 LOW checks before reset, target/image validation, readback evidence,
and application recovery/health confirmation. Keep firmware identity, diagnostics,
and verification provenance together with the implementation.

An early response incorrectly calls the RCT6 medium-density and suggests replacing
the HD startup selection. Do not follow that historical suggestion: current
repository target documentation identifies RCT6 as high-density. Historical IPs
are observations, not stable configuration.

### Install Remote Control Tools

Source: https://chatgpt.com/share/6acbcef6-731c-83e9-afb7-fa55af1005f9

Read the rendered shared conversation on 2026-10-11. Despite its title, its later
turns cover the current bounded STM32 flash path and completion-reporting fixes.
Earlier SSH/remote-laptop suggestions are ancillary proposals, not evidence of
installed services. Uploaded analyzer files are unavailable in the share.

Recorded flash evidence:

- Session `0x075AB799`, image 53,168 bytes, 26 pages.
- User output reports `FLASHED_VERIFIED` for pages 0–25; final page 1,968 bytes.
- Local image and manifest whole-image SHA-256:
  `42d9141bd2765f934eea788de883e1712d07d3e4fa88d0d21ac11168ab264ba5`.
- Final host error: `FLASH_FINISH: INCOMPLETE arg=26 hashes=26 received=0`;
  a successful `FLASH_COMPLETE` line is absent from the supplied output.
- Follow-up code fixes: `98e746a` makes finish retries idempotent, `805efe0`
  sends the verification acknowledgment before application reset. Both exist
  in local history and HEAD includes them.

The prior assistant inferred lost completion acknowledgment after final
verification. Treat that as an inference: the visible user output proves page
verification, but contains no independent whole-image `STM32_FLASH_VERIFY PASS`
or post-flash application-health result. The proposed next step was updated ESP
firmware and independent read-only image verification before another reflash.
No OTA upload, hardware command, or reflash was performed during this collection.

Three ChatGPT sources are now indexed. A fourth, if relevant, remains unidentified.

## Local Codex source history

### Debug ESP-01 UDP bootloader

Source thread: `01a10672-ed52-79c1-8e41-faa915ae268d`.
Its sidebar folder is `/home/blankone/projects/dvr-netaccess`, but inspected
commands operated in this audio-controller repository. Its latest three turns
were read for this snapshot; older turns remain available through pagination.

Recorded outcome: UART wiring correction followed by ROM sync/GET/GET ID PASS,
bootloader `0x22`, PID `0x0414`. Correct UART wiring is STM32 PA9 TX to ESP GPIO3
RX, ESP GPIO1 TX to STM32 PA10 RX. Normal streaming is 1 Mbaud, 24 kHz mono
PCM16LE over UDP 5004. The chat records both STM32 and ESP builds passing and
commit `7f3bc48` pushed to master. This is a historical result, not a new bench
verification performed during context collection.

### Add ChatGPT and GitHub activity

Source thread: `01a12c1e-8711-7772-92aa-6eb72c113ec2`.
Moved into the local `audio controller` project during this collection.
Other local task histories still need identification if they are absent from
the visible recent-chat inventory.

## GitHub tasks

The connected GitHub search returned no standalone issues and two PRs:

- [PR #1: Diagnostics: add hardware breadcrumb trail and safe bring-up checks](https://github.com/kingdragon6977/audio_controller/pull/1)
  adds MCU/clock/GPIO/I2C/I2S evidence and electrical safety gates.
- [PR #2: Add fail-closed STM32 remote flashing groundwork](https://github.com/kingdragon6977/audio_controller/pull/2)
  describes target-bound manifests, validation, staging, and a non-destructive
  stopping point. Its pending-work description predates newer local flashing
  commits; compare with current branch before using it as a task list.

Search results did not provide reliable PR state fields; merge/open status is
not asserted here. GitHub's commit search returned default-branch history headed
by [7f3bc48](https://github.com/kingdragon6977/audio_controller/commit/7f3bc48a160cc4fd2cd31b3337cc3dac849452f5).
It did not represent the latest `stm32-safe-flash` branch history.

Recent checked-out branch milestones, from local Git:

| Commit | Change |
| --- | --- |
| `24212be` | Require PA0 authorization for destructive flash prep |
| `9c3df28` | Define manifest-bound flash callback interface |
| `5df6821` | Add manifest-bound page flash session protocol |
| `6b68d00` | Implement bounded ROM erase/write/readback callbacks |
| `b6fc3b9` | Add explicit end-to-end host flash mode |
| `efa2aab` | Bootstrap legacy boot prep and pad final ROM write |
| `98e746a` | Make successful flash finish retries idempotent |
| `805efe0` | Acknowledge verified flash before application reset |

These commit subjects show implemented work, not proof of successful hardware
deployment. Inspect code, tests, and subsequent task evidence before proceeding.

## Keeping everyone aligned

1. Start with this file and the actual checked-out branch/source.
2. Link each confirmed source chat, PR, and relevant commit here.
3. Record current decisions and verified results, with dates and sources.
4. Mark proposed work, implemented code, and hardware verification separately.
5. Make this document available in each ChatGPT chat/project that needs it;
   local files and sidebar organization alone do not establish shared context
   in every existing conversation.

## Hardware verification — 2026-10-12

Performed locally at source commit `a682235` on `stm32-safe-flash`, with ESP at
`192.168.114.54` and STM32 UART2 `/dev/ttyACM1` at 115200 8N1.

- PlatformIO `esp01_1m_ota` build and OTA upload succeeded (uploader Result: OK).
  Uploaded ESP binary: 331,200 bytes, SHA-256
  `2f3981ec28d7921399160a796d81b1a8134fe3767947d2d4eb0ad115c2162265`.
- Preserved the original STM32 binary/manifest; staged manifest only.
- Read-only ROM test: sync ACK, GET/GET ID OK, both PB2 checks PASS,
  bootloader `0x22`, PID `0x0414`, 256 KiB flash, target PASS.
- Whole installed application verification: PASS, 53,168 bytes; actual and
  expected SHA-256 both
  `42d9141bd2765f934eea788de883e1712d07d3e4fa88d0d21ac11168ab264ba5`.
  This confirms the earlier application programming succeeded. It hashes the
  manifest image range, not unused bytes across the entire 256 KiB flash.
- Application restarted: 72 MHz SYSCLK, codec ACK, I2S WS synchronization PASS,
  bring-up complete, CLI ready, READY received and PCM stream enabled.
  Received 97 512-byte UDP PCM packets during the recovery observation.
- Final cleanup explicitly confirmed PA0 authorization OFF/HIGH, ODR=1 IDR=1.
- Raw verification/serial evidence: [verification log](verification-2026-10-12.log).

No STM32 erase/write or reflash was performed. Completion ACK ordering and retry
fixes are now uploaded to ESP, but the destructive FLASH_FINISH exchange was not
rerun; its reporting fix is not yet independently exercised on hardware.

## Automatic OTA integration — 2026-10-12

Implemented in the working tree based on `a682235` (`stm32-safe-flash`):

- CRC-checked `0xF4` STM32 request binds session, image size, and canonical
  manifest digest; PB2/BOOT1 readback precedes a two-second PA0 grant.
- A 1 ms SysTick expires the grant independently of the main loop. Rechecks
  cannot renew it; failed requests and READY/STOP release authorization.
- ESP checks the same grant twice with 250 ms settling, removes the automatic
  legacy fallback, and restricts ROM retries to the authorized session/manifest.
- Existing target/capability/page/full-image verification remains mandatory.
- Host distinguishes `FLASH_IMAGE_VERIFIED` from final `FLASH_COMPLETE`, polling
  for fresh PCM plus STM32 `0xF5` health (stream active and PA0 released).
- ESP's own OTA retains STOP/READY coordination; STOP now releases authorization.
- Protocol, migration order, and bench acceptance: [OTA integration](ota-integration.md).

Validation performed: STM32 cross-build, ESP PlatformIO build, new-image manifest
generation and offline `--flash --dry-run`, three native test executables
(authorization/CRC/expiry/session mutation; board interrupt expiry; ESP finish
ordering/retry/readback failures/recovery gates), and 11 Python manifest/recovery
tests all PASS. `make ota-test BUILD=build/ota-integration` reruns the tests.

New STM32 image is 54,200 bytes, 27 pages, SHA-256
`e31a0f89d593a0b3c51f1b4f3375f8f1c2462eff193feccfa4dca05386a433fe`,
in `build/ota-integration/`. New ESP binary SHA-256:
`8833d9f53eae4ae80f445967e500dad6365f6562c04df86d132586cfbde49f3e`.
The previously hardware-verified `build/audio_controller.bin` was preserved and
its original hash rechecked. These new changes have not been uploaded, committed,
or pushed. Deploy STM32 first through the currently installed ESP/manual grant,
verify recovery, then update ESP; its new routine requires the new STM32 protocol.
Hardware frame timing, timer/PA0 traces, automatic flash and failure/retry behavior
remain pending; host tests are not new hardware evidence.

## Coordinated deployment and hardware tests — 2026-10-12

User authorized flashing both processors and verifying their existing locations.
ESP remains `192.168.114.54` (MAC `ec:fa:bc:53:d1:1b`); STM32 UART2 remains
`/dev/ttyACM1`, 115200 8N1. Deployed the integration binaries identified above
from the working tree based on `a682235`; source changes remain uncommitted.

1. Bootstrapped STM32 through the previously installed ESP with manual PA0
   authorization and the host script from `a682235`. An initial UDP timeout
   occurred before programming; a direct manifest-only request subsequently
   returned a valid ACK. Retry programmed/verified all 27 pages, verified the
   whole 54,200-byte image, reported completion, and recovered application/PCM.
2. Uploaded new ESP firmware using PlatformIO OTA: uploader `Result: OK`.
3. Started with PA0 OFF and exercised the new automatic `--flash` routine using
   the same validated STM32 image. Both new authorization ACKs passed; all
   pages and the installed image verified. Final output:
   `FLASH_COMPLETE session=0x05826AAE application=PASS pcm=PASS bootauth=OFF`.
4. Repeated FLASH_FINISH and APPLICATION_STATUS for the completed session:
   both returned OK, 27 pages; no new startup banner/reset was observed.
5. Ran independent read-only ROM verification with the new ESP. PID `0x0414`,
   flash 256 KiB, target PASS; image SHA-256 actual/expected both
   `e31a0f89d593a0b3c51f1b4f3375f8f1c2462eff193feccfa4dca05386a433fe`.
   Application restarted, codec/I2S checks passed and PCM resumed.
6. UART2 confirmed PA0 OFF/HIGH, ODR=1 IDR=1. Stream counters advanced
   11,321 to 11,828, DMA errors 0, FE/NE 0, ORE 1 (unchanged between reads).

Evidence: [bootstrap](ota-deployment-2026-10-12.log),
[automatic update/retry](ota-automatic-2026-10-12.log),
[independent readback/recovery](ota-readback-2026-10-12.log).

Separate unresolved audio transport observation: passive capture received
97 PCM UDP packets in 12 seconds; a follow-up eight-second capture received
exactly 8 packets per second and no heartbeats. Nominal 24 kHz mono PCM with
256 samples per UDP packet requires 93.75 packets/second. This verifies some
continued delivery, not complete/lossless audio throughput. Determine whether
the limitation is sampling/stream production, ESP forwarding, LAN broadcast,
or host reception before trusting sustained audio timing. No cause is asserted.
Electrical grant-expiry traces, malformed-frame hardware checks, and simulated
application-failure recovery remain future bench validation.

## Audio delivery/timing investigation — 2026-10-12

User authorized continued diagnosis and confirms TLV320 analog inputs are
disconnected. Added ESP stream counters and receiver-selected unicast delivery;
updated the receiver with subscription retries, telemetry, and finite checks.
Unicast received 407 of 407 observed ESP UDP submissions over 12 seconds, with
12 heartbeats; broadcast received 65 while ESP submitted 271 over 8 seconds.
No UART sequence gaps, malformed headers, or UDP submission failures were
reported in either comparison. Exact broadcast discard point remains unknown.

An independent upstream timing mismatch remains: ESP receives ~135.26 PCM
frames/s (~8.66 ksample/s), and three STM32 GPIO WCLK estimates are near 17.2 kHz,
not 48 kHz. Codec readback matches the intended 16-MHz-MCLK profile. The physical
MCLK source/frequency is awaiting user confirmation; no PLL retuning was done.
Nominal 24 kHz labels must not be treated as measured capture timing.

Current deployed STM32 SHA-256:
`fd91cc7236e967d0da478a11d9d16c61d290c661a53bb757b3a8bd976379128d`
(54,560 bytes; adds `i2s rate`). Current ESP SHA-256:
`318bb5882f945823143b3b6ec8ec88e6f06de33f1047f8a0a77e7b30d4ff0320`.
Automatic STM32 programming completed with verified image/application/PCM and
PA0 OFF; final ESP OTA returned OK. The firmware source remains uncommitted.
Read [audio investigation](audio-transport-investigation.md) for measurements,
idle-input statistics, evidence links, and remaining clock validation.

Clock-source clarification: user reports a 16 MHz crystal on the AV6301 board,
but the AV6301 was isolated and left unpowered. The original reference clock was
likely removed. TLV master mode derives BCLK/WCLK from an external reference;
it still needs MCLK. TI confirms MCLK is pin 24 and IOVDD is pin 21; the earlier
investigation document's incorrect pin number has been corrected. Hardware
connectivity/waveform is not yet measured. Clock-source selection is pending
(dedicated oscillator, STM32 PA8 output with matching profile, or original feed).
Do not retune against the apparent output frequency or treat idle data as
calibrated until a valid reference and sample timing are established.

Clock-source decision: user will build/wire a dedicated 16 MHz oscillator
daughterboard, preserving the current boards and 16 MHz codec profile. No MCU
clock-output changes or PLL retuning are needed for this choice. A passive
crystal requires powered oscillator circuitry; use its digital output at a
level compatible with TLV IOVDD, feeding MCLK pin 24 with a common ground and
the former AV6301 clock source isolated. Hardware installation is pending.
After connection, verify approximately 48 kHz WCLK, 1.536 MHz BCLK, 24 ksample/s
decimated PCM and 93.75 UDP packets/s, plus stable counters and no clipping.

2026-10-13 hardware proposal: user plans to salvage a three-pin resonator and
associated capacitors from a spare board. Exact part/markings and connections
are unidentified; do not assume the center pin is clock output or attach supply
to unknown terminals. Common three-terminal ceramic resonators use the center
as capacitor ground and the outer terminals in an active oscillator loop. The
resonator/capacitors alone do not supply the required driven MCLK. Identify the
part and donor oscillator IC/circuit before choosing the daughterboard wiring.

2026-10-30 clock alternative: user reports PB9 is accessible, while accessing
the nearby pins would risk tested wiring. Corrected the earlier pin-name mixup:
dedicated MCO is PA8, not PB8. ST's STM32F103RC pin table maps PB9 to TIM4_CH4.
Current compiled source has no PB9/TIM4 use. A proposed alternative is hardware
PWM on PB9 at 12 MHz (72 MHz TIM4 clock, PSC=0, ARR=5, CCR4=3), enabling only
channel 4 and leaving PB6/PB7 I2C unchanged. TLV 12 MHz profile would use
P=1, R=1, J=8, D=1920, retaining the 48 kHz ADC target and 24 kHz PCM target.
This is a verified pin-mapping/calculated design proposal, not implemented or
hardware-tested. Do not bridge PB8/PB9; PB9 can generate its own timer output.
Confirm PB9 has no external load/driver and TLV IOVDD is compatible with the
STM32's 3.3 V output before wiring MCLK pin 24. Firmware must be changed before
PB9 supplies a clock. Dedicated resonator oscillator hardware remains an option.

2026-10-30 follow-up: user confirms PA8 is accessible and will check circuit
integrity before jumpering it to TLV MCLK. Preferred proposal is PA8 TIM1_CH1
hardware PWM at 12 MHz/50% (72 MHz timer, PSC=0 ARR=5 CCR1=3) with the matching
12 MHz TLV PLL profile, rather than adding a discrete oscillator. PA8 also has
MCO, but a 16 MHz clock is not directly available from the present 72 MHz clock
tree; do not imply that selecting PA8 automatically supplies 16 MHz.
Suggested wiring is PA8 through a 33-ohm starting-value series damping resistor
placed near the STM32, then a short wire to MCLK pin 24, with nearby digital
ground return. Confirm TLV IOVDD is 3.3 V for direct STM32-level drive, isolate
the old clock source, and do not drive an unpowered TLV. Pin wiring/IOVDD and
clock waveform are not yet confirmed; no new clock firmware was flashed.

2026-10-31 wiring completion reported by user: STM32 PA8 to a solder point
about 1.5 cm away, then a 40-ohm series resistor, followed by 7–8 cm of clock
wire twisted with ground to TLV MCLK pin 24; common ground about 1 cm from TLV.
No new clock firmware has been flashed. Source corresponding to the last
uploaded STM32 binary (SHA-256 fd91cc7236e967d0da478a11d9d16c61d290c661a53bb757b3a8bd976379128d)
does not configure PA8; startup clears MCO and no TIM1 output is enabled.
Expected PA8 state is reset-default floating input/high impedance, inferred
from source, not newly measured GPIO registers. With the former clock source
isolated and wiring correct, no new output drive/contention is expected.
TLV IOVDD voltage remains unconfirmed; measure pin 21 to digital ground before
enabling the proposed 3.3 V PA8 output. The 40-ohm resistor is damping, not a
level shifter or protection against driving an unpowered TLV. MCLK remains
invalid until a suitable clock is enabled and measured.

## PA8 clock firmware prepared for GitHub — 2026-10-31

User confirms TLV IOVDD pin 21 shares the STM32 3.3 V rail and requests local
implementation, safety/diagnostics, and commit/push. The prior wiring report
(PA8, 40 ohms, short twisted clock/ground to TLV pin 24) is the hardware contract.

Implemented PA8 TIM1_CH1 at a calculated 12 MHz/50%, with no timer IRQ/DMA,
and codec P=1 R=1 J=8 D=1920 for calculated 48 kHz ADC / 24 kHz mono PCM.
The codec is held reset until MCLK register checks pass. Critical codec
readback errors and I2C programming errors hold reset; adjacent PLL D writes
and settling delay precede ADC activation. Live WCLK polling must estimate
48 kHz within 1% before capture/stream activation, with a bounded loop and
once-per-second streaming retries. Profile/reset changes invalidate the rate.

DMA buffering now provides 5.333 ms per half against approximately 2.76 ms of
UART work. Completed halves are copied to snapshots with DMA ownership checks
before/after copying. Missed halves and 100 ms progress stalls stop/resynchronize
capture, increment counters, and signal a sequence discontinuity. Existing
analog gain and transport format are retained; calibrated analog response and
anti-alias DSP are not established by these checks.

UART2 adds `mclk status`; `i2s rate` uses the new bounded preflight; `esp status`
reports overruns/stalls and the WCLK estimate. See [PA8 clock procedure](pa8-audio-clock.md).
All earlier related OTA/transport changes, native tests, and recorded hardware
evidence are included in this GitHub update; unrelated captures/build caches
and private Wi-Fi configuration are excluded.

Validation: STM32 and ESP builds PASS; six native test executables and 11 Python
tests PASS; manifest generation and host `--flash --dry-run` PASS. New STM32
binary in `build/pa8-clock/`: 57,264 bytes, SHA-256
`ea8d57569d1af89555980df71ca2bdbbac8d59a2fee1458896e07502803630d2`.
[Manifest](firmware/pa8-clock-manifest.json) and
[validation metadata](firmware/pa8-clock-validation.json) preserve build identity.
This PA8 version has not been flashed: output voltage/duty/ringing, corrected
sample timing, sustained capture under CLI load, and physical failure behavior
still require bench verification. The boards remain on the previously recorded
diagnostic firmware until an authorized upload.

Pending: identify any
fourth ChatGPT chat and additional local Codex tasks,
and choose the intended ChatGPT project if the original chats should be moved.
The available ChatGPT project is named `apm32f030 wifi mic`; its membership in
this RCT6 project has not been confirmed.

## PA8 firmware OTA deployed — 2026-10-31

On branch `stm32-safe-flash`, firmware source commit `423693a`, user authorized
OTA via ESP `192.168.114.54`. Host `stage_stm32_update.py --flash` installed
the 57,264-byte PA8 image (SHA-256
`ea8d57569d1af89555980df71ca2bdbbac8d59a2fee1458896e07502803630d2`).
All 28 programmed pages and complete-image ROM readback passed. Session
`0x7E75CDEA` completed with `application=PASS pcm=PASS bootauth=OFF`.
This was STM32 OTA through the existing ESP firmware; ESP itself was not reflashed.

Post-update unicast observation: 866 packets / 20.011 seconds = 43.28 packets/s.
ESP reported `missing=0 bad_header=0 udp_fail=0 rom=0` throughout; however,
this is below the intended 93.75 packets/s (24 ksample/s). Source counter
throughput was also low, so full-rate acceptance remains unresolved despite
application-health PASS. `/dev/ttyACM1` was absent, preventing new UART2
clock/register/DMA diagnostic collection. No separate independent readback
was run beyond the updater's complete-image verification. Scope waveform,
physical fault injection, and analog calibration remain pending.

## UART2 investigation — 2026-10-31

UART2 `/dev/ttyACM1` became accessible after user closed Minicom. CLI requires
paced characters: an initial burst lost input bytes; subsequent commands used
35–40 ms spacing. See [UART diagnostics](pa8-uart-diagnostics-2026-10-31.log)
and [fresh startup](pa8-startup-diagnostics-2026-10-31.log).

MCLK TIM1/PA8 register checks PASS; codec PLL/interface/divider readback matches
the intended profile. Initial stream status showed no DMA errors, overruns,
stalls, or UART errors and a cached WCLK estimate of 48000 Hz. A blocking
`codec dump` caused one missed-half overrun. Subsequent live WCLK checks
estimated 17050–17100 Hz, blocking recovery. A software reboot reproduced
17050 Hz before DMA activation; the board is now STOPPED with capture blocked
by the existing rate gate. No firmware was changed or safety gate bypassed.
The previous OTA health PASS is not proof of sustained correct sample timing.

Unresolved: distinguish actual clock/routing/codec behavior from GPIO polling
measurement aliasing using an independent counter measurement; reconcile
the low delivery rate and inconsistent previous rate acceptance. A new scope
measurement is still unavailable. The codec dump also demonstrates that long
blocking CLI output interrupts capture and needs a deliberate pause/recovery
policy. STM32 reboot resets its counters; ESP was not rebooted/reflashed.

## Independent WCLK measurement implemented and deployed — 2026-10-31

Branch `stm32-safe-flash`, source parent `5173d0a`. Replaced GPIO polling
preflight with rising-edge EXTI12 counting on existing PB12 against a TIM2
1 MHz/50 ms gate. No rewiring or GPIO output is introduced. Preflight retains
48 kHz +/-1% acceptance and runs only with capture stopped. It rejects resource
conflicts and invalid timing, bounds stuck-timer loops/excessive interrupts,
and restores owned resources. SysTick/boot authorization expiry stays enabled.
Both methods share the MCU oscillator; this is not external clock calibration
or proof of MCLK voltage/waveform integrity. See [counter design](pa8-audio-clock.md#independent-wclk-counter-2026-10-31).

Validation: STM32 cross-build, manifest, flash dry-run, seven native executables
and 11 Python tests PASS. New native tests cover correct/incorrect/absent clock,
excessive edge rate, stuck/overflowed timer, resource conflicts, interrupt mask,
active capture, GPIO output rejection, and resource cleanup. ESP source/image
was unchanged. STM32 image is 57,832 bytes, SHA-256
`202f2af61735ccfd2b8ba4ae4c08d27db6edef7285a7ba96e90719a47d3d573b`;
[manifest](firmware/exti-clock-manifest.json).

New hardware verification via ESP `192.168.114.54`: all 29 pages plus complete
image ROM verification PASS ([OTA log](exti-clock-ota-2026-10-31.log)).
Application/PCM recovery timed out because the new measurement correctly
blocked capture; do not describe the OTA transaction as FLASH_COMPLETE.
Startup measured 853 edges / 50000 us = 17060 Hz; two manual `i2s rate`
checks each measured 854 edges / 50000 us = 17080 Hz ([UART log](exti-clock-uart-2026-10-31.log)).
`bootauth status` confirms PA0 OFF/HIGH. Stream STOPPED, no DMA errors,
overruns, stalls or UART errors on the new boot. No automatic reflash attempted.

The independent result agrees with previous GPIO estimates, making a real
clock-path/codec problem more likely than polling aliasing; exact root cause
remains unresolved. Do not bypass the rate gate or label PCM calibrated/24k
until corrected and measured. Next investigate clock routing and codec clock
source/PLL behavior. Waveform measurements and analog calibration remain pending.

## Offline clock-path audit and comparison prepared — 2026-10-31

User explicitly reports device unpowered for circuit checks; no hardware access
or upload attempted in this work. Branch `stm32-safe-flash`, source parent
`65d0224`. TI Rev B section 10.3.7 and register tables 19–36 confirm current
P=1/R=1/J=8/D=1920, NADC=8/MADC=2/AOSR=128 and BCLK divider 8 / ADC_CLK
are the intended 12 MHz-to-48 kHz configuration. PA8 timer register math also
remains correct. No offline root cause is established. Corrected an IADC
comment: encoding 0x80 means 256 instructions, not 128; register value unchanged.
Source: https://www.ti.com/lit/ds/symlink/tlv320adc3101.pdf

Prepared `codec clocktest` to compare fractional PLL (48000 Hz), integer PLL
(46875 Hz) and direct MCLK bypass (46875 Hz) without changing wiring. Capture
is paused; ADC data stays muted during diagnostic modes, clock changes occur
with ADC off, PLL is programmed disabled and settled after power selection,
mode readback precedes measurement. Configuration/readback failure holds reset.
At completion normal profile is restored and the normal 48 kHz gate rechecked;
diagnostic rates never authorize PCM. Active boot authorization blocks test.
See [procedure](pa8-audio-clock.md#controlled-clock-path-comparison-prepared-not-hardware-tested).

Validation: STM32 cross-build/manifest PASS; seven native executables and
11 Python tests PASS, including comparison-mode settings, muting, profile
restoration and injected I2C failure holding reset. Built image
`build/clock-path-test/audio_controller.bin`: 58984 bytes, SHA-256
`8f2f2c8069172c44a9005f62a3b1d504a5f67bc091f9f41869de9c02f0fb9107`.
[Manifest](firmware/clock-path-test-manifest.json). This image has not been
flashed. Powered comparison is pending user's completion of circuit checks.
Previous hardware measurement remains approximately 17.08 kHz; no new hardware
result is implied by the build/tests.

## Controlled clock-path comparison deployed — 2026-10-31

User confirms unpowered continuity checks passed for PA8-to-MCLK24,
PB12-to-WCLK2, PB13-to-BCLK1, PB15-to-DOUT3 and PB14-to-RESET4, then
reports power restored and UART2 free. Source commit `18a39a3`, branch
`stm32-safe-flash`. OTA through ESP `192.168.114.54` installed the
58,984-byte image with SHA-256
`8f2f2c8069172c44a9005f62a3b1d504a5f67bc091f9f41869de9c02f0fb9107`.
All 29 pages and full-image ROM verification PASS; see
[OTA log](clock-path-ota-2026-10-31.log). PCM/application health acceptance
remains blocked by the wrong normal WCLK rate; image verification alone
is not successful PCM recovery. ESP image unchanged.

New powered UART test (`codec clocktest`), all mode register reads accepted:

| Mode | Expected Hz | Observed Hz | Count / gate |
| --- | --- | --- | --- |
| Fractional PLL | 48000 | 17080 | 854 / 50000 us |
| Integer PLL | 46875 | 17100 | 855 / 50000 us |
| MCLK bypass | 46875 | 0 | 0 / 50000 us |

The bypass result means no WCLK edges were detected; its measurement flag
is INVALID due to zero count, not a missing TIM2 gate. Normal profile was
restored, normal preflight remained approximately 17.08–17.10 kHz,
PA0 boot authorization confirmed OFF/HIGH. Stream STOPPED; DMA error,
overrun, stall and UART error counters remain zero.
[UART evidence](clock-path-uart-2026-10-31.log).

Inference, not confirmed root cause: nearly unchanged PLL output and absent
bypass output point toward missing/unusable reference at the codec, despite
continuity and register checks. No physical PA8 waveform has been measured.
Next distinguish actual PA8 output activity from codec-side reference reception
and input power/clock behavior. Do not repeat continuity claims as proof of
clock quality or bypass the production rate gate.

## PA8 pad and codec-side MCLK return diagnostics — 2026-10-31

User asks to continue with board powered and UART2 free. Branch
`stm32-safe-flash`, source parent `49be14a`. Implemented `mclk test`: hold
codec reset and stop capture, slow PA8 TIM1 to 12 kHz, count PA8 pad via
EXTI8/TIM2, restore 12 MHz even on measurement failure. Only a successful
self-test releases reset/reapplies normal profile and normal WCLK gate.
The 12 kHz test exercises the pad/counter but does not prove 12 MHz waveform
quality or reception after the series resistor. Added `codec mclkpath`: with
ADC/dividers and PLL off, select MCLK/128 CLKOUT and route to existing
DOUT/PB15; test DOUT LOW/HIGH first, then count EXTI15 over 20 ms. PB15
remains input; active capture, shared interrupt ownership or boot authorization
blocks relevant tests. Config readback failure holds reset. Both tests restore
normal production configuration; no diagnostic rate can authorize PCM.

Two STM32 builds/OTA deployments occurred, both passing programmed-page and
full-image ROM readback; neither met final application/PCM acceptance because
the WCLK gate remained wrong. ESP unchanged. Intermediate PA8-only image:
60080 bytes, SHA-256
`2db76caa5e05e8537543cbd6f2f5bcd6fb4a4ca0dc8cc633b9ceee9a8a0c8fff`,
[manifest](firmware/pa8-pad-test-manifest.json), [OTA](pa8-pad-ota-2026-10-31.log),
[UART](pa8-pad-uart-2026-10-31.log). Current deployed image with both diagnostics:
61168 bytes, SHA-256
`fa46958bc438a8e5315d930c23dc5d03b043c8226965cb162af93650bd51187c`,
[manifest](firmware/mclk-return-test-manifest.json), [OTA](mclk-return-ota-2026-10-31.log),
[UART](mclk-return-uart-2026-10-31.log). STM32 builds/manifests/dry-runs, seven
native executables and 11 Python tests PASS. Tests cover PA8 restoration after
good/bad counts, reset/active-capture guards, EXTI routing/cleanup, DOUT
93.75 kHz counting, codec routing/ADC/PLL shutdown and normal DOUT restoration.

New hardware: PA8 pad measured 600 edges / 50000 us = 12000 Hz, then 12 MHz
register restoration PASS. Codec DOUT LOW/HIGH returned 0/1 on PB15 PASS.
Two MCLK/128 return measurements each found 0 edges / 20000 us (93750 Hz
expected). Normal profile restored, WCLK still approximately 17.06 kHz,
stream STOPPED; PA0 OFF, DMA/overrun/stall/UART error counters zero.
This supports investigating reference delivery/operating-frequency integrity
or codec reference handling; it does not prove a particular open/clamp or
damaged chip. MCLK electrical waveform remains unmeasured.

Requested next user evidence with board powered: DC averages at PA8 before
40-ohm resistor and TLV MCLK pin24 after resistor, plus whether pin24 retains
a connection to unpowered AV6301 circuitry. DC averages may reveal clamping
but cannot certify frequency or waveform. No further circuit modification
requested. Reference for clock return routes: TI Rev B tables 31, 32 and 57,
https://www.ti.com/lit/ds/symlink/tlv320adc3101.pdf .

## MCLK short repaired; powered software checks pass — 2026-10-31

User reports finding/fixing approximately 130 ohms to ground on MCLK; powered
DC reading changed from 0.64 V to approximately 1.6 V. Those resistance/DC
readings are user measurements. No firmware change or upload was performed
for this verification: source remains `89234a3` on `stm32-safe-flash`, deployed
STM32 SHA-256
`fa46958bc438a8e5315d930c23dc5d03b043c8226965cb162af93650bd51187c`.

New powered checks via UART2 ([log](mclk-repair-uart-2026-10-31.log)):
PA8 self-test 600 edges / 50000 us = 12000 Hz PASS; normal timer restored.
Codec DOUT LOW/HIGH readback 0/1 PASS. Codec MCLK/128 return measured
1875 edges / 20000 us = 93750 Hz PASS, inferring 12 MHz reference reception.
Normal codec profile readback PASS, WCLK 2400 edges / 50000 us = 48000 Hz
PASS, PA0 authorization OFF/HIGH, stream RUNNING. This before/after result
strongly supports the repaired MCLK loading fault as the cause of the prior
clock failures. It does not establish analog accuracy or waveform margins.

Long blocking `esp status` UART output still causes missed DMA half-buffers
and automatic resynchronization. Two such diagnostic interruptions appeared
in the first mixed observation ([stream log](mclk-repair-stream-2026-10-31.log));
no DMA transfer errors, clock stalls or UART errors were reported.
Do not describe this as zero-overrun operation under CLI load.

A separate quiet 20.003-second unicast observation received 1883 packets
(94.14 packets/s, nominal target 93.75). Every sampled ESP status retained
`missing=2 bad_header=0 udp_fail=0 rom=0`: no new missing-frame count during
the quiet window. [Quiet stream evidence](mclk-repair-quiet-stream-2026-10-31.log).
The wall-time packet rate is approximately 0.4% above nominal; MCU clock
checks share its reference, and precise absolute frequency/audio calibration
remains unestablished. No waveform measurement or calibrated audio input
was used. Remaining firmware issue: avoid/coordinate blocking CLI output
with capture so diagnostics do not themselves cause recoverable loss.
