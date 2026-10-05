# ESP-01 PCM to UDP bridge

This project receives the STM32 audio stream on the ESP-01 hardware UART at 1,000,000 baud and forwards 24 kHz mono signed PCM16LE over UDP port 5004.

## Wiring

- STM32 PA9 (USART1 TX) -> ESP-01 GPIO3/RX
- STM32 PA10 (USART1 RX) <- ESP-01 GPIO1/TX
- STM32 GND <-> ESP-01 GND
- ESP-01 VCC = regulated 3.3 V only
- ESP-01 EN/CH_PD pulled HIGH
- ESP-01 GPIO0 HIGH for normal boot, LOW only while flashing
- ESP-01 GPIO2 pulled HIGH for normal boot

Use a solid 3.3 V supply capable of handling ESP8266 current bursts. The first OTA-capable firmware still has to be flashed over serial. Hold the STM32 in reset or disconnect PA9 while doing that initial ESP serial flash so STM32 UART traffic cannot interfere with the ESP bootloader.

## Configure Wi-Fi and OTA

```bash
cd esp01_stream
cp include/wifi_config.example.h include/wifi_config.h
nano include/wifi_config.h
```

Set `WIFI_SSID`, `WIFI_PASSWORD`, and optionally `UDP_TARGET_IP`. The real `wifi_config.h` is gitignored.

`OTA_HOSTNAME` defaults to `audio-esp01`. `OTA_PASSWORD` is optional in the firmware, but a password is strongly recommended on any network you do not fully trust. If you enable a password, pass the matching authentication option to the PlatformIO espota uploader.

For first testing you can leave `UDP_TARGET_IP` as `255.255.255.255` for LAN broadcast. If your network blocks broadcast, set it to the PC's LAN IP.

## First build and serial flash

```bash
cd esp01_stream
pio run -e esp01_1m
pio run -e esp01_1m -t upload --upload-port /dev/ttyUSB0
```

After flashing, return GPIO0 HIGH and reset/power-cycle the ESP-01. This is the last routine update that should require unplugging the ESP-01 if OTA works on your network.

## Later OTA updates

With the ESP running the OTA-capable firmware and connected to Wi-Fi:

```bash
cd esp01_stream
pio run -e esp01_1m_ota -t upload
```

The OTA environment targets `audio-esp01.local`. If mDNS name resolution is unavailable, use the ESP address shown by the UDP heartbeat:

```bash
pio run -e esp01_1m_ota -t upload --upload-port 192.168.x.x
```

At OTA start the ESP sends control byte `0xF1`, causing the STM32 to stop PCM streaming. The ESP pauses UART/UDP audio processing while its flash is being updated. After a successful update the ESP reboots, reconnects to Wi-Fi, sends `0xF0`, and the STM32 resumes streaming automatically. This prevents the high-rate PCM path from competing with the OTA transfer.

Once Wi-Fi connects, the ESP sends control byte `0xF0` to the STM32. The STM32 then starts 24 kHz mono PCM streaming automatically on USART1. UART2 remains available for the normal STM32 CLI.

## Listen on Linux

With ffplay installed:

```bash
python3 esp01_stream/tools/udp_pcm_receiver.py | \
  ffplay -nodisp -autoexit -f s16le -ar 24000 -ac 1 -i pipe:0
```

Or with ALSA `aplay`:

```bash
python3 esp01_stream/tools/udp_pcm_receiver.py | \
  aplay -q -f S16_LE -r 24000 -c 1
```

The ESP aggregates four 64-sample STM32 UART frames into each 512-byte UDP packet.


## Read-only STM32 ROM bootloader test

With the BOOT0 and NRST MOSFET control stages installed, the ESP-01 can test
the STM32F103 factory ROM bootloader without erasing or writing flash.

Control logic used by the ESP firmware:

- GPIO0 HIGH -> STM32 BOOT0 LOW (normal application)
- GPIO0 LOW -> STM32 BOOT0 HIGH (system-memory bootloader)
- GPIO2 LOW -> STM32 NRST released
- GPIO2 HIGH -> STM32 NRST asserted LOW

The firmware establishes the normal GPIO0/GPIO2 states immediately after ESP
startup, before Wi-Fi/audio initialization.

The ESP uses a dedicated `WiFiUDP` listener on local port 5004 for control.
PCM and heartbeat transmission use a separate UDP object, so their remote
destination cannot filter control packets sent from the PC's ephemeral source
port.

Flash matching STM32 and ESP firmware before running this test. The STM32
firmware recognizes the ESP's boot-preparation request, stops PCM, drives the
shared PB2 LED/BOOT1 pin LOW, verifies both its output latch and pin readback,
and returns a binary acknowledgment. The ESP performs this check immediately
and again after holding PB2 LOW for 250 ms. It skips the reset and ROM commands
if either check fails.

Send the diagnostic from Linux with a socket that waits for both responses
(replace the address if DHCP changes it):

```bash
python3 -c 'import socket; s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.settimeout(5); s.sendto(b"STM32_BOOT_TEST\n",("192.168.114.54",5004)); print(s.recvfrom(2048)[0].decode(),end=""); print(s.recvfrom(2048)[0].decode(),end="")'
```

The ESP sends an immediate `STM32_BOOT_TEST starting (read-only)` response,
performs the PB2 checks, enters the STM32 ROM bootloader, switches the shared
UART to 115200 8E1, sends the 0x7F autobaud synchronization byte, and expects
0x79 ACK. If synchronization succeeds it issues only the read-only GET (0x00)
and GET ID (0x02) commands. It then restores BOOT0 LOW, resets the STM32 into
the normal application, restores the UART to 1,000,000 8N1, and sends READY so
PCM streaming can resume. A failed sync reports `TIMEOUT` or the actual byte
received as `RX_0xNN`.

A successful result has this form:

```text
STM32_BOOT_TEST sync=ACK get=OK getid=OK bootprep_initial=PASS bootprep_settled=PASS bootver=0x22 pid=0x0414
```

This exact result was verified on the STM32F103RCT6 hardware. Device ID 0x0414
identifies the STM32F10xxx high-density family.

This diagnostic contains no erase, write-memory, write-protect, or
readout-protect commands.


## Safe STM32 flashing design

The ROM diagnostic now requires `pid=0x0414` and `flash_kb=256` for `target=PASS`. See [`doc/stm32-safe-flash.md`](../doc/stm32-safe-flash.md) for the fail-closed flashing procedure. The current test remains read-only.
