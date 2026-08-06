#include <string.h>
#include "stm32f10x.h"
#include "uart.h"
#include "cli.h"
#include <stdio.h>
static char line[64];
static int index=0;

static void execute(char *cmd)
{
    if(strcmp(cmd,"help")==0)
    {
        uart4_print("\r\nCommands:\r\n");
        uart4_print(" help\r\n");
        uart4_print(" id\r\n");
        uart4_print(" uid\r\n");
        uart4_print(" flash\r\n");
        uart4_print(" ram\r\n");
        uart4_print(" reboot\r\n");
        uart4_print(" led on\r\n");
        uart4_print(" led off\r\n");
        return;
    }

    if(strcmp(cmd,"id")==0)
    {
        uart4_print("STM32F103VFT6 XL\r\n");
        return;
    }

    if(strcmp(cmd,"flash")==0)
    {
        uart4_print("FLASH = 768 KB\r\n");
        return;
    }

    if(strcmp(cmd,"ram")==0)
    {
        uart4_print("RAM = 96 KB\r\n");
        return;
    }

    if(strcmp(cmd,"uid")==0)
    {
        uint32_t *uid=(uint32_t*)0x1FFFF7E8;

        char buf[80];

        sprintf(buf,
                "%08lX %08lX %08lX\r\n",
                uid[0],
                uid[1],
                uid[2]);

        uart4_print(buf);
        return;
    }

    if(strcmp(cmd,"led on")==0)
    {
        GPIO_SetBits(GPIOA,GPIO_Pin_8);
        uart4_print("LED ON\r\n");
        return;
    }

    if(strcmp(cmd,"led off")==0)
    {
        GPIO_ResetBits(GPIOA,GPIO_Pin_8);
        uart4_print("LED OFF\r\n");
        return;
    }

    if(strcmp(cmd,"reboot")==0)
    {
        NVIC_SystemReset();
    }

    uart4_print("Unknown command\r\n");
}

void cli_init(void)
{
    index=0;
}

int uart4_available(void)
{
    return (UART4->SR & USART_SR_RXNE);
}

char uart4_getc(void)
{
    return UART4->DR;
}
void cli_task(void)
{
    while(uart4_available())
    {
        char c=uart4_getc();

        if(c=='\r' || c=='\n')
        {
            line[index]=0;

            uart4_print("\r\n");

            execute(line);

            uart4_print("> ");

            index=0;
        }
        else
        {
            if(index<63)
            {
                line[index++]=c;
                uart4_putc(c);
            }
        }
    }
}
