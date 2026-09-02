#include "stm32f10x.h"
#include "board.h"
#include "uart.h"
#include "i2c.h"
#include "cli.h"
#include "codec.h"
#include "diagnostics.h"
#include "i2s_rx.h"
#include "audio_stream.h"

static void delay(uint32_t d)
{
    while (d--)
        __asm__("nop");
}

static void print_hex16(uint16_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    uart2_putc(hex[(value >> 12) & 0x0Fu]);
    uart2_putc(hex[(value >> 8) & 0x0Fu]);
    uart2_putc(hex[(value >> 4) & 0x0Fu]);
    uart2_putc(hex[value & 0x0Fu]);
}

static void print_hex32(uint32_t value)
{
    print_hex16((uint16_t)(value >> 16));
    print_hex16((uint16_t)value);
}

static int codec_probe_with_recovery(void)
{
    unsigned int attempt;

    for (attempt = 0u; attempt < 3u; ++attempt)
    {
        if (i2c1_probe(TLV320ADC3101_ADDR))
            return 1;

        if (attempt == 2u)
            break;

        uart2_print("  Probe retry: reinitializing I2C1 and hard-resetting codec...\r\n");
        i2c1_init();
        delay(100000u);
        codec_reset();
        delay(100000u);
    }

    return 0;
}

static int codec_apply_profile_with_recovery(void)
{
    unsigned int attempt;

    for (attempt = 0u; attempt < 3u; ++attempt)
    {
        if (codec_apply_av6301_profile())
            return 1;

        if (attempt == 2u)
            break;

        uart2_print("Codec profile write failed; resetting I2C1 + codec and retrying...\r\n");
        i2c1_init();
        delay(100000u);
        codec_reset();
        delay(100000u);

        if (!codec_probe_with_recovery())
        {
            uart2_print("Codec did not ACK during profile recovery.\r\n");
            continue;
        }
    }

    return 0;
}

int main(void)
{
    int codec_present = 0;
    int i2c_safe;
    int i2s_safe;
    int i2s_clock_active = 0;
    int i2s_capture_started = 0;
    int i2s_capture_complete = 0;
    uint32_t timeout;
    unsigned int i;
    const uint16_t *samples;
    i2s_rx_debug_t i2s_debug;

    board_init();
    uart2_init();
    esp_uart_init();
    cli_init();

    uart2_print("\r\n========================================\r\n");
    uart2_print(" audio_controller - RCT6 bring-up\r\n");
    uart2_print("========================================\r\n");
    uart2_print("USART2: PA2=TX PA3=RX 115200 8N1\r\n");
    uart2_print("USART1: PA9=TX PA10=RX 2000000 8N1 (ESP-01 PCM link)\r\n");

    diagnostics_print_mcu();
    diagnostics_print_clock();
    diagnostics_print_audio_pins();
    diagnostics_print_i2s_clock_ownership();

    i2s_safe = diagnostics_i2s2_safe();
    uart2_print("\r\nI2S PIN SAFETY: ");
    uart2_print(i2s_safe ? "PASS - codec-driven pins are not MCU outputs\r\n"
                         : "FAIL - I2S pin is configured as an MCU output\r\n");

    uart2_print("\r\nInitializing I2C1...\r\n");
    i2c1_init();
    uart2_print("I2C1 initialization returned.\r\n");
    diagnostics_print_i2c1();
    diagnostics_print_audio_pins();

    i2c_safe = diagnostics_i2c1_safe();
    uart2_print("I2C1 BUS SAFETY: ");
    uart2_print(i2c_safe ? "PASS - AF open-drain and SDA/SCL released HIGH\r\n"
                         : "FAIL - refusing I2C transaction because pin/bus state is unsafe\r\n");

    uart2_print("\r\nResetting TLV320ADC3101 on PB14...\r\n");
    codec_reset();
    uart2_print("TLV320 reset released.\r\n");
    diagnostics_print_audio_pins();

    if (i2c_safe)
    {
        uart2_print("\r\nTLV320ADC3101:\r\n");
        uart2_print("  RESET: PB14 active-low hardware reset\r\n");
        uart2_print("  I2C1: PB6=SCL PB7=SDA\r\n");
        uart2_print("  Address: 0x18 (7-bit)\r\n");
        uart2_print("  Probing... ");

        codec_present = codec_probe_with_recovery();
        uart2_print(codec_present ? "ACK - codec responded\r\n"
                                   : "NO ACK / ERROR after retries\r\n");
        diagnostics_print_i2c1();
    }
    else
    {
        uart2_print("\r\nCodec probe SKIPPED for safety.\r\n");
    }

    i2s_safe = diagnostics_i2s2_safe();
    uart2_print("I2S PRE-ACTIVATION SAFETY: ");
    uart2_print(i2s_safe ? "PASS\r\n"
                         : "FAIL - codec I2S activation BLOCKED\r\n");

    if (codec_present && i2s_safe)
    {
        uart2_print("Applying AV6301 codec profile...\r\n");
        if (codec_apply_profile_with_recovery())
        {
            uart2_print("Codec profile applied.\r\n");
            uart2_print("Read-back trail follows:\r\n");
            codec_dump_profile();

            diagnostics_print_i2s_clock_ownership();
            uart2_print("\r\nI2S CLOCK ACTIVITY CHECK: ");
            i2s_clock_active = diagnostics_i2s_clock_pins_active();
            uart2_print(i2s_clock_active
                        ? "PASS - WCLK activity observed on PB12\r\n"
                        : "WARN - no WCLK transition observed by coarse MCU poll\r\n");
            uart2_print("  NOTE: logic analyzer/scope remains authoritative for exact clock frequency and duty cycle.\r\n");

            if (i2s_clock_active && diagnostics_i2s2_safe())
            {
                uart2_print("\r\nI2S RX DMA ACTIVATION PRE-FLIGHT: PASS\r\n");
                uart2_print("  DMA1 CH4 = SPI2/I2S RX\r\n");
                uart2_print("  FORMAT   = Philips I2S, 16-bit, slave RX\r\n");
                uart2_print("  SYNC     = receiver live first; DMA handoff at PB12 falling edge\r\n");
                uart2_print("  BUFFER   = 256 x 16-bit slots\r\n");

                i2s_capture_started = i2s_rx_start_capture();
                uart2_print(i2s_capture_started
                            ? "I2S RX DMA START: PASS\r\n"
                            : "I2S RX DMA START: FAIL - activation/frame sync refused\r\n");

                if (i2s_capture_started)
                {
                    timeout = 2000000u;
                    while (timeout--)
                    {
                        if (i2s_rx_capture_complete())
                        {
                            i2s_capture_complete = 1;
                            break;
                        }
                        __asm__("nop");
                    }

                    if (i2s_rx_error_flags())
                    {
                        uart2_print("I2S RX DMA CAPTURE: FAIL - DMA transfer error\r\n");
                    }
                    else if (!i2s_capture_complete)
                    {
                        uart2_print("I2S RX DMA CAPTURE: FAIL - timeout waiting for 256 slots\r\n");
                    }
                    else
                    {
                        uint16_t min_sample = 0xFFFFu;
                        uint16_t max_sample = 0x0000u;
                        uint32_t zero_count = 0u;
                        uint32_t min_count = 0u;
                        uint32_t max_count = 0u;
                        uint32_t identical_pairs = 0u;
                        uint32_t even_sum = 0u;
                        uint32_t odd_sum = 0u;

                        uart2_print("I2S RX DMA CAPTURE: PASS - 256 x 16-bit slots captured\r\n");

                        samples = i2s_rx_buffer();
                        for (i = 0u; i < I2S_RX_SAMPLES; ++i)
                        {
                            uint16_t sample = samples[i];

                            if (sample < min_sample) min_sample = sample;
                            if (sample > max_sample) max_sample = sample;
                            if (sample == 0x0000u) zero_count++;
                            if (sample == 0x8000u) min_count++;
                            if (sample == 0xFFFFu) max_count++;
                            if (i != 0u && sample == samples[i - 1u]) identical_pairs++;
                            if ((i & 1u) == 0u) even_sum += sample;
                            else odd_sum += sample;
                        }

                        uart2_print("  FIRST 32 SAMPLES: ");
                        for (i = 0u; i < 32u; ++i)
                        {
                            uart2_print("0x");
                            print_hex16(samples[i]);
                            if (i != 31u) uart2_print(" ");
                        }
                        uart2_print("\r\n");

                        uart2_print("  MIN SAMPLE       = 0x");
                        print_hex16(min_sample);
                        uart2_print("\r\n");
                        uart2_print("  MAX SAMPLE       = 0x");
                        print_hex16(max_sample);
                        uart2_print("\r\n");
                        uart2_print("  ZERO COUNT       = 0x");
                        print_hex32(zero_count);
                        uart2_print("\r\n");
                        uart2_print("  0x8000 COUNT     = 0x");
                        print_hex32(min_count);
                        uart2_print("\r\n");
                        uart2_print("  0xFFFF COUNT     = 0x");
                        print_hex32(max_count);
                        uart2_print("\r\n");
                        uart2_print("  IDENTICAL PAIRS  = 0x");
                        print_hex32(identical_pairs);
                        uart2_print("\r\n");
                        uart2_print("  EVEN SLOT SUM    = 0x");
                        print_hex32(even_sum);
                        uart2_print("\r\n");
                        uart2_print("  ODD SLOT SUM     = 0x");
                        print_hex32(odd_sum);
                        uart2_print("\r\n");

                        i2s_rx_print_analysis();

                        i2s_rx_get_debug(&i2s_debug);
                        uart2_print("\r\nI2S RX DEBUG SNAPSHOT:\r\n");
                        uart2_print("  WS SYNC         = ");
                        uart2_print(i2s_debug.ws_sync_ok ? "PASS\r\n" : "FAIL\r\n");
                        uart2_print("  WS AT DMA       = ");
                        uart2_print(i2s_debug.ws_level_at_enable ? "HIGH\r\n" : "LOW\r\n");
                        uart2_print("  SPI2 SR BEFORE  = 0x");
                        print_hex32(i2s_debug.sr_before);
                        uart2_print("\r\n");
                        uart2_print("  SPI2 SR ENABLED = 0x");
                        print_hex32(i2s_debug.sr_after_enable);
                        uart2_print("\r\n");
                        uart2_print("  SPI2 SR COMPLETE= 0x");
                        print_hex32(i2s_debug.sr_after_capture);
                        uart2_print("\r\n");
                        uart2_print("  SPI2 DR LAST    = 0x");
                        print_hex32(i2s_debug.dr_after_capture);
                        uart2_print("\r\n");
                        uart2_print("  DMA1 ISR        = 0x");
                        print_hex32(i2s_debug.dma_isr_after_capture);
                        uart2_print("\r\n");
                        uart2_print("  DMA1 CH4 CNDTR  = 0x");
                        print_hex16(i2s_debug.dma_cndtr_after_capture);
                        uart2_print("\r\n");
                    }

                    i2s_rx_stop();
                    uart2_print("I2S RX DMA STOP: SAFE\r\n");
                }
            }
            else
            {
                uart2_print("I2S RX DMA ACTIVATION: BLOCKED - live clock/safety prerequisite failed\r\n");
            }
        }
        else
        {
            uart2_print("Codec profile write failed after retries.\r\n");
        }
    }
    else if (!i2s_safe)
    {
        uart2_print("Codec profile NOT applied: I2S pins are not in a safe input state.\r\n");
    }
    else
    {
        uart2_print("Codec profile NOT applied because no ACK was received.\r\n");
    }

    diagnostics_print_i2s2();
    diagnostics_print_audio_pins();

    delay(500000u);

    uart2_print("\r\nBring-up complete. CLI ready.\r\n");
    uart2_print("ESP-01 stream waits for Wi-Fi READY token on USART1.\r\n");
    uart2_print("> ");

    while (1)
    {
        cli_task();
        audio_stream_task();
    }
}
