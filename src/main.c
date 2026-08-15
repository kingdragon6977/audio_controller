#include "stm32f10x.h"
#include "board.h"
#include "uart.h"
#include "cli.h"

/*
 * Clock diagnostic:
 *
 * PA8 is the STM32F1 MCO pin.  We deliberately select the raw HSE here,
 * rather than SYSCLK or PLL/2, so the external 8 MHz resonator can be
 * measured directly with the logic analyzer.
 *
 * The normal SystemInit() already attempts to start HSE for the 72 MHz PLL.
 * We explicitly check HSERDY as well so this test is useful even if the PLL
 * configuration ever falls back to HSI.
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

    /* MCO = raw HSE. No PLL division or SYSCLK ambiguity. */
    RCC->CFGR &= ~RCC_CFGR_MCO;
    RCC->CFGR |= RCC_MCO_HSE;

    return ((RCC->CR & RCC_CR_HSERDY) != 0u) ? 1u : 0u;
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
    hse_ok = clock_test_mco_hse();

    /*
     * LED is only a status indicator; PA8 remains the measurement output.
     * HSE good  = slow heartbeat.
     * HSE bad   = fast heartbeat.
     */
    while (1)
    {
        led_on();
        delay(hse_ok ? 500000u : 150000u);
        led_off();
        delay(hse_ok ? 500000u : 150000u);
    }
}
