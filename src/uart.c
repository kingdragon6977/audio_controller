#include "stm32f10x.h"
#include "uart.h"

void uart4_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef us;

    /* Enable clocks: GPIOA = APB2, USART2 = APB1 */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA,
        ENABLE
    );

    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_USART2,
        ENABLE
    );

    /* PA2 = USART2 TX */
    gpio.GPIO_Pin = GPIO_Pin_2;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* PA3 = USART2 RX */
    gpio.GPIO_Pin = GPIO_Pin_3;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    us.USART_BaudRate = 115200;
    us.USART_WordLength = USART_WordLength_8b;
    us.USART_StopBits = USART_StopBits_1;
    us.USART_Parity = USART_Parity_No;
    us.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    us.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART2, &us);
    USART_Cmd(USART2, ENABLE);
}

void uart4_putc(char c)
{
    while (!(USART2->SR & USART_SR_TXE))
        ;

    USART2->DR = c;
}

void uart4_print(const char *s)
{
    while (*s)
        uart4_putc(*s++);
}
