#include "stm32f10x.h"
#include "uart.h"


void uart_init(void)
{

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_USART1,
        ENABLE);


    GPIO_InitTypeDef gpio;


    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA,&gpio);



    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA,&gpio);



    USART_InitTypeDef us;


    us.USART_BaudRate = 115200;
    us.USART_WordLength = USART_WordLength_8b;
    us.USART_StopBits = USART_StopBits_1;
    us.USART_Parity = USART_Parity_No;
    us.USART_HardwareFlowControl =
        USART_HardwareFlowControl_None;

    us.USART_Mode =
        USART_Mode_Tx |
        USART_Mode_Rx;


    USART_Init(USART1,&us);

    USART_Cmd(USART1,ENABLE);
}



void uart_putc(char c)
{
    while(!(USART1->SR & USART_SR_TXE));

    USART1->DR=c;
}



void uart_print(const char *s)
{
    while(*s)
    {
        uart_putc(*s++);
    }
}
