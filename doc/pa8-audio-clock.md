# PA8 audio reference clock

## Board and clock profile

User-confirmed wiring: STM32 PA8, about 1.5 cm to a 40-ohm series resistor,
then 7–8 cm of clock/ground twisted pair to TLV320ADC3101 MCLK pin 24. Nearby
common ground; TLV IOVDD pin 21 shares the STM32's 3.3 V rail. The old AV6301
clock feed must remain isolated. A series resistor damps edges; it does not
translate voltage or prevent back-powering an unpowered codec.

PA8 uses TIM1_CH1 PWM, not MCO: 72 MHz timer, PSC=0, ARR=5, CCR1=3 gives a
calculated 12 MHz, 50% clock. No TIM1 interrupts or DMA are used; other TIM1
channels remain disabled. PB6/PB7 I2C and PA0/PB2 boot authorization are retained.

The codec uses P=1, R=1, J=8, D=1920: calculated PLL=98.304 MHz, NADC=8,
MADC=2, AOSR=128 gives 48 kHz conversion, and BCLK divider 8 gives 1.536 MHz
for 16-bit stereo I2S. Left-channel decimation retains the existing nominal
24 kHz mono PCM transport. Gain remains 0 dB and the unused right analog routes
remain disconnected. Sample selection is still the existing simple 2:1
decimation; anti-alias filtering and calibrated microphone response are future
DSP work, not certified by these transport/clock checks.

References: [ST pin/clock specifications](https://www.st.com/resource/en/datasheet/stm32f103rc.pdf),
[ST timer operation](https://www.st.com/resource/en/application_note/dm00236305-pwm-generation-using-stm32-general-purpose-timers-stmicroelectronics.pdf),
[TI clock generation/registers](https://www.ti.com/lit/ds/symlink/tlv320adc3101.pdf).

## Startup and failure behavior

1. Drive PA0 authorization OFF and hold PB14 codec RESET LOW.
2. Start UART diagnostics and report MCU clock state.
3. Require HSE/PLL readiness, 72 MHz SYSCLK/HCLK/PCLK2, no MCO output, and
   default TIM1 routing before configuring PA8. Reset/configure TIM1, enable
   only CH1, then read back timer registers and the PA8 GPIO mode. A readback
   failure disables the timer and returns PA8 to an input.
4. Release codec reset only when the reference-clock register check passes.
   Program the 12 MHz profile; keep the D-MSB/D-LSB writes adjacent and wait
   at least 10 ms after completing the pair before enabling ADC paths.
5. Read back critical PLL/divider/interface/power/mute registers. A mismatch
   blocks the profile-ready flag and holds hardware RESET LOW; I2C write errors
   also hold reset, avoiding a blind mute write when the page is unknown.
6. With capture stopped, count PB12 WCLK rising edges in a tick-aligned 20 ms
   window. Require a polling estimate within 1% of 48 kHz before DMA activation.
   The polling loop is bounded even if SysTick stops. Profile reset/application
   invalidates the cached rate; streaming rechecks the rate before each start.

These are register checks and a GPIO estimate, not proof of MCLK voltage,
waveform integrity, exact jitter, or calibrated audio performance. A scope at
the TLV end of the wire is authoritative.

## Capture performance and visibility

The circular DMA buffer is 1,024 16-bit slots. Each half represents 256 stereo
frames (5.333 ms at 48 kHz), yielding two 64-sample mono UART frames. Their
276 bytes take about 2.76 ms at 1 Mbaud 8N1. This provides margin compared with
the prior 1.333 ms half-buffer period and 1.38 ms UART transmission.

A completed half is copied to a separate snapshot while DMA owns the other
half; ownership is checked before and after copying. UART transmission reads
the snapshot. Simultaneously pending HT/TC flags or an unsafe snapshot stop and
resynchronize capture rather than publishing potentially overwritten data.
No DMA progress for 100 ms also stops capture. Error restarts deliberately skip
a sequence number so ESP drops incomplete aggregation across the discontinuity.
Counters persist across stream restarts; retry attempts are limited to once
per second so a bad clock cannot monopolize the main loop.

UART2 commands:

```text
mclk status     TIM1, GPIO and calculated MCLK readback
i2s rate        bounded WCLK preflight, temporarily pauses PCM
codec dump      PLL/divider/analog register evidence
esp status      stream packets, DMA errors, overruns, stalls, WCLK estimate
```

The OTA application-health response also requires a verified codec profile and
valid rate preflight. A correctly programmed image with faulty clock wiring
can produce `FLASH_IMAGE_VERIFIED` followed by a recovery timeout; that does not
justify automatically reflashing it.

## Build and bench acceptance

```bash
make -j2 all manifest size ota-test BUILD=build/pa8-clock
python3 tools/stage_stm32_update.py ESP_IP \
  build/pa8-clock/audio_controller.bin \
  build/pa8-clock/audio_controller.manifest.json --flash --dry-run
```

The source is built/tested locally; this PA8 version has not been flashed.
Before declaring hardware success: scope 12 MHz/50% and voltage/ringing at TLV
MCLK, verify WCLK near 48 kHz and BCLK near 1.536 MHz, run independent image
readback, check PA0 OFF and codec/profile/rate PASS, then observe sustained
unicast at approximately 93.75 512-byte packets/s with stable error counters.
Also exercise removed/incorrect clock and heavy CLI activity. The existing
unconnected-input noise observation is not a calibrated analog baseline.
