#include "stm32f10x.h"
#include "board.h"
#include "uart.h"
#include "i2c.h"
#include "cli.h"
#include "codec.h"

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

static void print_i2c_status(void)
{
    uart2_print("\r\nI2C1 STATUS:\r\n");
    uart2_print("  GPIOB->CRL = 0x");
    uart_hex32(GPIOB->CRL);
    uart2_print("\r\n");
    uart2_print("  GPIOB->IDR = 0x");
    uart_hex32(GPIOB->IDR);
    uart2_print("\r\n");
    uart2_print("  GPIOB->ODR = 0x");
    uart_hex32(GPIOB->ODR);
    uart2_print("\r\n");
    uart2_print("  I2C1->CR1  = 0x");
    uart_hex32(I2C1->CR1);
    uart2_print("\r\n");
    uart2_print("  I2C1->CR2  = 0x");
    uart_hex32(I2C1->CR2);
    uart2_print("\r\n");
    uart2_print("  I2C1->CCR  = 0x");
    uart_hex32(I2C1->CCR);
    uart2_print("\r\n");
    uart2_print("  I2C1->TRISE= 0x");
    uart_hex32(I2C1->TRISE);
    uart2_print("\r\n");
    uart2_print("  I2C1->SR1  = 0x");
    uart_hex32(I2C1->SR1);
    uart2_print("\r\n");
    uart2_print("  I2C1->SR2  = 0x");
    uart_hex32(I2C1->SR2);
    uart2_print("\r\n");
}

static int codec_probe(void)
{
    int result;

    uart2_print("\r\nTLV320ADC3101:\r\n");
    uart2_print("  I2C1: PB6=SCL PB7=SDA\r\n");
    uart2_print("  Address: 0x18 (7-bit)\r\n");
    uart2_print("  Probing... ");

    result = i2c1_probe(TLV320ADC3101_ADDR);

    if (result)
        uart2_print("ACK - codec responded\r\n");
    else
        uart2_print("NO ACK / ERROR\r\n");

    return result;
}

int main(void)
{
    int codec_present;

    board_init();
    uart2_init();
    cli_init();

    uart2_print("\r\n========================================\r\n");
    uart2_print(" audio_controller - RCT6 bring-up\r\n");
    uart2_print("========================================\r\n");
    uart2_print("USART2: PA2=TX PA3=RX 115200 8N1\r\n");
    uart2_print("System clock: 72 MHz target\r\n");

    print_clock_status();

    uart2_print("\r\nInitializing I2C1...\r\n");
    i2c1_init();
    uart2_print("I2C1 initialization returned.\r\n");
    print_i2c_status();

    /*
     * The codec may be disconnected during bring-up. A missing ACK must
     * return cleanly; do not attempt codec register writes unless the device
     * actually responds.
     */
    codec_present = codec_probe();
    print_i2c_status();

    if (codec_present)
    {
        uart2_print("Applying AV6301 codec profile...\r\n");
        if (codec_apply_av6301_profile())
            uart2_print("Codec profile applied.\r\n");
        else
            uart2_print("Codec profile write failed.\r\n");
    }
    else
    {
        uart2_print("Codec profile NOT applied because no ACK was received.\r\n");
    }

    delay(500000u);

    uart2_print("\r\nBring-up complete. CLI ready.\r\n");
    uart2_print("> ");

    while (1)
    {
        cli_task();

        led_on();
        delay(120000u);
        led_off();
        delay(120000u);
    }
}
