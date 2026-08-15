#include "stm32f10x.h"
#include "board.h"
#include "uart.h"

/*
 * Clock diagnostic for the STM32F103RCT6 board.
 *
 * PA8 is the STM32F1 MCO pin.  The test deliberately selects the raw HSE
 * (the external 8 MHz ceramic resonator) so the signal can be measured with
 * the logic analyzer without any PLL/divider ambiguity.
 *
 * USART2 (PA2/PA3) reports the RCC state so we can distinguish:
 *   1. HSE/resonator not starting,
 *   2. HSE running but MCO not configured,
 *   3. MCO running but PA0/analyzer wiring being wrong.
 */
static uint8_t clock_test_mco_hse(void)
{
    GPIO_InitTypeDef gpio;
    uint32_t timeout = 1000000u;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* Make sure HSE is enabled before selecting it on MCO. */
    RCC->CR |= RCC_CR_HSEON;
    while (((RCC->CR & RCC_CR_HSERDY) == 0u) && --timeout)
        ;

    gpio.GPIO_Pin   = GPIO_Pin_8;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* MCO = raw HSE. */
    RCC->CFGR &= ~RCC_CFGR_MCO;
    RCC->CFGR |= RCC_MCO_HSE;

    return ((RCC->CR & RCC_CR_HSERDY) != 0u) ? 1u : 0u;
}

static void uart_hex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    int shift;

    for (shift = 28; shift >= 0; shift -= 4)
        uart2_putc(hex[(value >> shift) & 0x0Fu]);
}

static void print_clock_status(uint8_t hse_ok)
{
    uart2_print("\r\n=== STM32F103RCT6 CLOCK / MCO DIAGNOSTIC ===\r\n");
    uart2_print("PA8: MCO = HSE (raw external clock)\r\n");
    uart2_print("PA2: USART2 TX / PA3: USART2 RX\r\n");

    uart2_print("RCC->CR   = 0x");
    uart_hex32(RCC->CR);
    uart2_print("\r\n");

    uart2_print("RCC->CFGR = 0x");
    uart_hex32(RCC->CFGR);
    uart2_print("\r\n");

    uart2_print("GPIOA->CRH = 0x");
    uart_hex32(GPIOA->CRH);
    uart2_print("\r\n");

    uart2_print("GPIOA->ODR = 0x");
    uart_hex32(GPIOA->ODR);
    uart2_print("\r\n");

    uart2_print("HSI ready: ");
    uart2_print((RCC->CR & RCC_CR_HSIRDY) ? "YES\r\n" : "NO\r\n");
    uart2_print("HSE ready: ");
    uart2_print(hse_ok ? "YES\r\n" : "NO\r\n");
    uart2_print("PLL ready: ");
    uart2_print((RCC->CR & RCC_CR_PLLRDY) ? "YES\r\n" : "NO\r\n");

    uart2_print("MCO field: 0x");
    uart_hex32(RCC->CFGR & RCC_CFGR_MCO);
    uart2_print(" (expected HSE selection)\r\n");

    uart2_print("Connect PA8 -> PA0 and GND -> GND.\r\n");
    uart2_print("Expected PA8/PA0 frequency: approximately 8 MHz.\r\n");
    uart2_print("==============================================\r\n");
}

static void delay(uint32_t d)
{
    while (d--)
        __asm__("nop");
}

int main(void)
{
    uint8_t hse_ok;

    board_init();
    uart2_init();

    uart2_print("\r\nBOOT: audio_controller RCT6 clock test\r\n");

    hse_ok = clock_test_mco_hse();
    print_clock_status(hse_ok);

    /*
     * LED is only a status indicator; PA8 remains the measurement output.
     * HSE good = slow heartbeat.
     * HSE bad  = fast heartbeat.
     */
    while (1)
    {
        led_on();
        delay(hse_ok ? 500000u : 150000u);
        led_off();
        delay(hse_ok ? 500000u : 150000u);
    }
}
