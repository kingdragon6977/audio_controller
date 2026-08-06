#include "stm32f10x.h"
#include "board.h"

static uint8_t led_state = 0;

void board_init(void)
{
    GPIO_InitTypeDef gpio;

    /* GPIOA clock */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA,
        ENABLE);

    /* PA8 LED */
    gpio.GPIO_Pin   = GPIO_Pin_8;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;

    GPIO_Init(GPIOA,&gpio);

    GPIO_ResetBits(GPIOA,GPIO_Pin_8);
}

void led_on(void)
{
    GPIO_SetBits(GPIOA,GPIO_Pin_8);
    led_state = 1;
}

void led_off(void)
{
    GPIO_ResetBits(GPIOA,GPIO_Pin_8);
    led_state = 0;
}

void led_toggle(void)
{
    if(led_state)
        led_off();
    else
        led_on();
}
