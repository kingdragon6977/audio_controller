#include "stm32f10x.h"
#include "board.h"
#include "uart.h"
#include "cli.h"

static void delay(uint32_t d)
{
    while (d--)
        __asm__("nop");
}

int main(void)
{
    board_init();
    uart2_init();
    esp_uart_init();
    cli_init();

    uart2_print("\r\n");
    uart2_print("---------------------------------\r\n");
    uart2_print(" Directional Mic Controller\r\n");
    uart2_print(" STM32F103RBT6\r\n");
    uart2_print(" 72 MHz / USART2 PA2/PA3\r\n");
    uart2_print(" ESP USART1 PA9/PA10\r\n");
    uart2_print(" CLI Ready\r\n");
    uart2_print("---------------------------------\r\n");
    uart2_print("> ");

    while (1)
    {
        led_on();
        delay(100000);

        led_off();
        delay(100000);

        cli_task();

        /* Pass any ESP-01 response to the debug UART. */
        while (esp_uart_available())
            uart2_putc(esp_uart_getc());
    }
}
