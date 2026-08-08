#include "stm32f10x.h"
#include "board.h"
#include "uart.h"
#include "cli.h"

static void delay(uint32_t d)
{
    while(d--)
        __asm__("nop");
}

int main(void)
{
    board_init();
    uart4_init();
    cli_init();

    uart4_print("\r\n");
    uart4_print("---------------------------------\r\n");
    uart4_print(" Directional Mic Controller\r\n");
    uart4_print(" STM32F103RBT6\r\n");
    uart4_print(" 72 MHz / USART2 PA2/PA3\r\n");
    uart4_print(" CLI Ready\r\n");
    uart4_print("---------------------------------\r\n");
    uart4_print("> ");

    while(1)
    {
        led_on();
        delay(100000);

        led_off();
        delay(100000);

        cli_task();
    }
}
