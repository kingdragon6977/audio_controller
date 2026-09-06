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
