#include "stm32f10x.h"
#include "board.h"
#include "uart.h"
#include "i2c.h"
#include "cli.h"

static void delay(uint32_t d)
{
    while (d--)
        __asm__("nop");
}

static void uart_hex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    int shift;

    for (shift = 28; shift >= 0; shift -= 4)
        uart2_putc(hex[(value >> shift) & 0x0Fu]);
}

static void print_clock_status(void)
{
    uint32_t cfgr = RCC->CFGR;
    uint32_t cr = RCC->CR;

    uart2_print("\r\nCLOCK:\r\n");
    uart2_print("  RCC->CR   = 0x");
    uart_hex32(cr);
    uart2_print("\r\n");
    uart2_print("  RCC->CFGR = 0x");
    uart_hex32(cfgr);
    uart2_print("\r\n");
    uart2_print("  HSI ready = ");
    uart2_print((cr & RCC_CR_HSIRDY) ? "YES\r\n" : "NO\r\n");
    uart2_print("  HSE ready = ");
    uart2_print((cr & RCC_CR_HSERDY) ? "YES\r\n" : "NO\r\n");
    uart2_print("  PLL ready = ");
    uart2_print((cr & RCC_CR_PLLRDY) ? "YES\r\n" : "NO\r\n");
}

static void codec_probe(void)
{
    uart2_print("\r\nTLV320ADC3101:\r\n");
    uart2_print("  I2C2: PB10=SCL PB11=SDA\r\n");
    uart2_print("  Address: 0x18 (7-bit)\r\n");

    if (i2c2_probe(TLV320ADC3101_I2C_ADDR))
    {
        uart2_print("  Probe: ACK - codec responded\r\n");
    }
    else
    {
        uart2_print("  Probe: NO ACK\r\n");
        uart2_print("  Check codec power, I2C pull-ups, ADR0/ADR1, and wiring.\r\n");
    }
}

int main(void)
{
    board_init();
    uart2_init();
    cli_init();

    uart2_print("\r\n========================================\r\n");
    uart2_print(" audio_controller - RCT6 bring-up\r\n");
    uart2_print("========================================\r\n");
    uart2_print("USART2: PA2=TX PA3=RX 115200 8N1\r\n");
    uart2_print("System clock: 72 MHz target\r\n");

    print_clock_status();

    /* Configure PB10/PB11 for I2C2 as AF open-drain.  Do not drive either
     * line push-pull here: the TLV320ADC3101 bus is open-drain and relies on
     * external pull-ups. */
    i2c2_init();

    /* Give the codec time to finish power/reset settling before its first
     * I2C transaction.  This is intentionally before the first probe. */
    delay(500000u);
    codec_probe();

    uart2_print("\r\nBring-up complete. CLI ready.\r\n");
    uart2_print("> ");

    while (1)
    {
        cli_task();

        /* Slow heartbeat; peripherals remain available while CLI runs. */
        led_on();
        delay(120000u);
        led_off();
        delay(120000u);
    }
}
