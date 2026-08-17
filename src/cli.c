#include <string.h>
#include "stm32f10x.h"
#include "uart.h"
#include "i2c.h"
#include "codec.h"
#include "cli.h"
#include <stdio.h>

static char line[64];
static int line_index = 0;

static void execute(char *cmd)
{
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
        uart2_print(i2c2_probe(TLV320ADC3101_I2C_ADDR) ? "ACK\r\n" : "NO ACK\r\n");
        return;
    }

    if (strcmp(cmd, "codec dump") == 0)
    {
        if (!i2c2_probe(TLV320ADC3101_I2C_ADDR))
        {
            uart2_print("TLV320ADC3101 @ 0x18: NO ACK\r\n");
            return;
        }

        codec_dump_profile();
        return;
    }

    if (strcmp(cmd, "codec apply") == 0)
    {
        uart2_print("Applying captured AV6301 TLV320ADC3101 Page-0 profile...\r\n");
        uart2_print("WARNING: this drives the shared I2C bus; use only after isolating the AV6301.\r\n");

        if (codec_apply_av6301_profile())
            uart2_print("Codec profile applied successfully.\r\n");
        else
            uart2_print("Codec profile FAILED (I2C timeout/NACK).\r\n");
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
