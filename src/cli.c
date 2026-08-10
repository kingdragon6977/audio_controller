#include <string.h>
#include "stm32f10x.h"
#include "uart.h"
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
        uart2_print(" flash\r\n");
        uart2_print(" ram\r\n");
        uart2_print(" reboot\r\n");
        uart2_print(" led on\r\n");
        uart2_print(" led off\r\n");
        uart2_print(" esp test\r\n");
        return;
    }

    if (strcmp(cmd, "id") == 0)
    {
        uart2_print("STM32F103RBT6 MD\r\n");
        return;
    }

    if (strcmp(cmd, "flash") == 0)
    {
        uart2_print("FLASH = 128 KB\r\n");
        return;
    }

    if (strcmp(cmd, "ram") == 0)
    {
        uart2_print("RAM = 20 KB\r\n");
        return;
    }

    if (strcmp(cmd, "uid") == 0)
    {
        uint32_t *uid = (uint32_t *)0x1FFFF7E8;
        char buf[80];

        sprintf(buf,
                "%08lX %08lX %08lX\r\n",
                uid[0], uid[1], uid[2]);

        uart2_print(buf);
        return;
    }

    if (strcmp(cmd, "led on") == 0)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_2);
        uart2_print("LED ON\r\n");
        return;
    }

    if (strcmp(cmd, "led off") == 0)
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_2);
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
