#include "stm32f10x.h"
#include "board.h"
#include "uart.h"
#include "cli.h"

/* Temporary HSE/MCO diagnostic: PA8 outputs the external HSE directly. */
static void clock_test_mco_hse(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    gpio.GPIO_Pin   = GPIO_Pin_8;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* MCO = HSE. With the installed 8 MHz resonator this should be ~8 MHz. */
    RCC->CFGR &= ~RCC_CFGR_MCO;
    RCC->CFGR |= RCC_MCO_HSE;
}

static void delay(uint32_t d)
{
    while (d--)
        __asm__("nop");
}

int main(void)
{
    board_init();

    /* TEMPORARY CLOCK TEST: PA8 (MCO) = raw HSE. */
    clock_test_mco_hse();

    while (1)
    {
        led_on();
        delay(100000);
        led_off();
        delay(100000);
    }
}
