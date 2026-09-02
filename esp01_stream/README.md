# ESP-01 PCM to UDP bridge

This project receives the STM32 audio stream on the ESP-01 hardware UART at 2,000,000 baud and forwards 24 kHz mono signed PCM16LE over UDP port 5004.

## Wiring

- STM32 PA9 (USART1 TX) -> ESP-01 GPIO3/RX
- STM32 PA10 (USART1 RX) <- ESP-01 GPIO1/TX
- STM32 GND <-> ESP-01 GND
- ESP-01 VCC = regulated 3.3 V only
- ESP-01 EN/CH_PD pulled HIGH
- ESP-01 GPIO0 HIGH for normal boot, LOW only while flashing
- ESP-01 GPIO2 pulled HIGH for normal boot

Use a solid 3.3 V supply capable of handling ESP8266 current bursts. Hold the STM32 in reset or disconnect PA9 while flashing the ESP-01 so STM32 UART traffic cannot interfere with the ESP bootloader.

## Configure Wi-Fi

```bash
cd esp01_stream
cp include/wifi_config.example.h include/wifi_config.h
nano include/wifi_config.h
```

Set `WIFI_SSID`, `WIFI_PASSWORD`, and optionally `UDP_TARGET_IP`. The real `wifi_config.h` is gitignored.

For first testing you can leave `UDP_TARGET_IP` as `255.255.255.255` for LAN broadcast. If your network blocks broadcast, set it to the PC's LAN IP.

## Build and flash

```bash
cd esp01_stream
pio run
pio run -t upload --upload-port /dev/ttyUSB0
```

After flashing, return GPIO0 HIGH and reset/power-cycle the ESP-01.

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
