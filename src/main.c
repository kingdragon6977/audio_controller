#include "stm32f10x.h"
#include "uart.h"
#include "cli.h"

static void delay(uint32_t d)
{
    while(d--)
        __asm__("nop");
}

int main(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA,
        ENABLE);

    gpio.GPIO_Pin=GPIO_Pin_8;
    gpio.GPIO_Mode=GPIO_Mode_Out_PP;
    gpio.GPIO_Speed=GPIO_Speed_2MHz;

    GPIO_Init(GPIOA,&gpio);

    uart4_init();

    cli_init();

    uart4_print("\r\n");
    uart4_print("---------------------------------\r\n");
    uart4_print(" Directional Mic Controller\r\n");
    uart4_print(" STM32F103VFT6\r\n");
    uart4_print(" CLI Ready\r\n");
    uart4_print("---------------------------------\r\n");
    uart4_print("> ");

    while(1)
    {
        GPIO_SetBits(GPIOA,GPIO_Pin_8);
        delay(100000);

        GPIO_ResetBits(GPIOA,GPIO_Pin_8);
        delay(100000);

        cli_task();
    }
}
