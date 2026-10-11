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

Pending: obtain independent whole-image/post-flash health evidence, identify any
fourth ChatGPT chat and additional local Codex tasks,
and choose the intended ChatGPT project if the original chats should be moved.
The available ChatGPT project is named `apm32f030 wifi mic`; its membership in
this RCT6 project has not been confirmed.
