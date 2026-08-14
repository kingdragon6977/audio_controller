#include "stm32f10x.h"
#include "board.h"

static uint8_t led_state = 0;

void board_init(void)
{
    GPIO_InitTypeDef gpio;

    /* PB2 LED clock */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOB,
        ENABLE);

    /* PB2 LED - active high: PB2 HIGH = LED ON */
    gpio.GPIO_Pin   = GPIO_Pin_2;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;

    GPIO_Init(GPIOB, &gpio);

    /* Start with LED off */
    GPIO_ResetBits(GPIOB, GPIO_Pin_2);
    led_state = 0;
}

void led_on(void)
{
    GPIO_SetBits(GPIOB, GPIO_Pin_2);
    led_state = 1;
}

void led_off(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_2);
    led_state = 0;
}

void led_toggle(void)
{
    if (led_state)
        led_off();
    else
        led_on();
}
