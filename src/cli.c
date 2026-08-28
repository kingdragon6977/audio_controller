#include <string.h>
#include "stm32f10x.h"
#include "uart.h"
#include "i2c.h"
#include "codec.h"
#include "i2s_rx.h"
#include "cli.h"
#include <stdio.h>

static char line[64];
static int line_index = 0;

static int run_i2s_capture(unsigned int sequence, int verbose)
{
    uint32_t timeout = 3000000u;
    const uint16_t *samples;
    unsigned int i;

    if (verbose)
        uart2_print("Starting frame-aligned I2S capture...\r\n");

    if (!i2s_rx_start_capture())
    {
        if (verbose)
            uart2_print("I2S capture FAILED to start (pin safety or WCLK sync).\r\n");
        else
        {
            char buf[64];
            sprintf(buf, "  #%u FAIL start/sync\r\n", sequence);
            uart2_print(buf);
        }
        i2s_rx_stop();
        return 0;
    }

    while (!i2s_rx_capture_complete() && timeout != 0u)
    {
        timeout--;
        __asm__("nop");
    }

    if (i2s_rx_error_flags())
    {
        if (verbose)
            uart2_print("I2S capture FAILED - DMA transfer error.\r\n");
        else
        {
            char buf[64];
            sprintf(buf, "  #%u FAIL DMA\r\n", sequence);
            uart2_print(buf);
        }
        i2s_rx_stop();
        return 0;
    }

    if (!i2s_rx_capture_complete())
    {
        if (verbose)
            uart2_print("I2S capture FAILED - timeout.\r\n");
        else
        {
            char buf[64];
            sprintf(buf, "  #%u FAIL timeout\r\n", sequence);
            uart2_print(buf);
        }
        i2s_rx_stop();
        return 0;
    }

    if (verbose)
    {
        uart2_print("I2S capture PASS.\r\n");
        i2s_rx_print_analysis();
    }
    else
    {
        int32_t min_a = 32767;
        int32_t max_a = -32768;
        int32_t min_b = 32767;
        int32_t max_b = -32768;
        int32_t peak_a = 0;
        int32_t peak_b = 0;
        int32_t sum_a = 0;
        int32_t sum_b = 0;
        unsigned int pairs = 0u;
        char buf[160];

        samples = i2s_rx_buffer();

        /* Match the full analyzer: discard the first captured A/B pair. */
        for (i = 2u; i + 1u < I2S_RX_SAMPLES; i += 2u)
        {
            int32_t a = (int16_t)samples[i];
            int32_t b = (int16_t)samples[i + 1u];
            int32_t aa = (a < 0) ? -a : a;
            int32_t ab = (b < 0) ? -b : b;

            if (a < min_a) min_a = a;
            if (a > max_a) max_a = a;
            if (b < min_b) min_b = b;
            if (b > max_b) max_b = b;
            if (aa > peak_a) peak_a = aa;
            if (ab > peak_b) peak_b = ab;
            sum_a += a;
            sum_b += b;
            pairs++;
        }

        sprintf(buf,
                "  #%u A[min=%ld max=%ld mean=%ld peak=%ld] "
                "B[min=%ld max=%ld mean=%ld peak=%ld]\r\n",
                sequence,
                (long)min_a, (long)max_a,
                (long)(pairs ? sum_a / (int32_t)pairs : 0), (long)peak_a,
                (long)min_b, (long)max_b,
                (long)(pairs ? sum_b / (int32_t)pairs : 0), (long)peak_b);
        uart2_print(buf);
    }

    i2s_rx_stop();
    return 1;
}

static int parse_capture_count(const char *cmd, unsigned int *count)
{
    const char prefix[] = "i2s capture ";
    const char *p;
    unsigned int value = 0u;

    if (strncmp(cmd, prefix, sizeof(prefix) - 1u) != 0)
        return 0;

    p = cmd + sizeof(prefix) - 1u;
    if (*p == 0)
        return 0;

    while (*p >= '0' && *p <= '9')
    {
        value = value * 10u + (unsigned int)(*p - '0');
        if (value > 50u)
            return 0;
        p++;
    }

    if (*p != 0 || value == 0u)
        return 0;

    *count = value;
    return 1;
}

static void execute(char *cmd)
{
    unsigned int capture_count;

    if (strcmp(cmd, "help") == 0)
    {
        uart2_print("\r\nCommands:\r\n");
        uart2_print(" help\r\n");
        uart2_print(" id\r\n");
        uart2_print(" uid\r\n");
        uart2_print(" clock\r\n");
        uart2_print(" codec\r\n");
        uart2_print(" codec dump\r\n");
        uart2_print(" codec apply\r\n");
        uart2_print(" i2s capture        (full one-shot report)\r\n");
        uart2_print(" i2s capture N      (compact repeated captures, N=1..50)\r\n");
        uart2_print(" reboot\r\n");
        uart2_print(" led on\r\n");
        uart2_print(" led off\r\n");
        uart2_print(" esp test\r\n");
        return;
    }

    if (strcmp(cmd, "id") == 0)
    {
        uart2_print("STM32F103RCT6 HD\r\n");
        return;
    }

    if (strcmp(cmd, "clock") == 0)
    {
        uart2_print("RCC->CR   = ");
        {
            static const char hex[] = "0123456789ABCDEF";
            int s;
            for (s = 28; s >= 0; s -= 4)
                uart2_putc(hex[(RCC->CR >> s) & 0x0Fu]);
        }
        uart2_print("\r\nRCC->CFGR = ");
        {
            static const char hex[] = "0123456789ABCDEF";
            int s;
            for (s = 28; s >= 0; s -= 4)
                uart2_putc(hex[(RCC->CFGR >> s) & 0x0Fu]);
        }
        uart2_print("\r\n");
        return;
    }

    if (strcmp(cmd, "codec") == 0)
    {
        uart2_print("TLV320ADC3101 @ 0x18: ");
        uart2_print(i2c1_probe(TLV320ADC3101_ADDR) ? "ACK\r\n" : "NO ACK\r\n");
        return;
    }

    if (strcmp(cmd, "codec dump") == 0)
    {
        if (!i2c1_probe(TLV320ADC3101_ADDR))
        {
            uart2_print("TLV320ADC3101 @ 0x18: NO ACK\r\n");
            return;
        }

        codec_dump_profile();
        return;
    }

    if (strcmp(cmd, "codec apply") == 0)
    {
        uart2_print("Applying captured AV6301 TLV320ADC3101 profile...\r\n");
        uart2_print("WARNING: this drives the shared I2C bus; use only after isolating the AV6301.\r\n");

        if (codec_apply_av6301_profile())
            uart2_print("Codec profile applied successfully.\r\n");
        else
            uart2_print("Codec profile FAILED (I2C timeout/NACK).\r\n");
        return;
    }

    if (strcmp(cmd, "i2s capture") == 0)
    {
        (void)run_i2s_capture(1u, 1);
        return;
    }

    if (parse_capture_count(cmd, &capture_count))
    {
        unsigned int n;
        unsigned int passed = 0u;
        char buf[80];

        sprintf(buf, "Running %u compact frame-aligned I2S captures...\r\n", capture_count);
        uart2_print(buf);

        for (n = 1u; n <= capture_count; ++n)
        {
            if (run_i2s_capture(n, 0))
                passed++;
        }

        sprintf(buf, "Repeated capture summary: %u/%u PASS\r\n", passed, capture_count);
        uart2_print(buf);
        return;
    }

    if (strncmp(cmd, "i2s capture ", 12u) == 0)
    {
        uart2_print("Usage: i2s capture N, where N is 1..50\r\n");
        return;
    }

    if (strcmp(cmd, "uid") == 0)
    {
        uint32_t *uid = (uint32_t *)0x1FFFF7E8;
        char buf[80];

        sprintf(buf, "%08lX %08lX %08lX\r\n",
                uid[0], uid[1], uid[2]);
        uart2_print(buf);
        return;
    }

    if (strcmp(cmd, "led on") == 0)
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_2);
        uart2_print("LED ON\r\n");
        return;
    }

    if (strcmp(cmd, "led off") == 0)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_2);
        uart2_print("LED OFF\r\n");
        return;
    }

    if (strcmp(cmd, "esp test") == 0)
    {
        esp_uart_print("AT\r\n");
        uart2_print("ESP: AT sent\r\n");
        return;
    }

    if (strcmp(cmd, "reboot") == 0)
    {
        NVIC_SystemReset();
    }

    uart2_print("Unknown command\r\n");
}

void cli_init(void)
{
    line_index = 0;
}

void cli_task(void)
{
    while (uart2_available())
    {
        char c = uart2_getc();

        if (c == '\r' || c == '\n')
        {
            line[line_index] = 0;

            uart2_print("\r\n");
            execute(line);
            uart2_print("> ");
            line_index = 0;
        }
        else if (line_index < 63)
        {
            line[line_index++] = c;
            uart2_putc(c);
        }
    }
}
