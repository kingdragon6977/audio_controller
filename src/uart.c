#include "stm32f10x.h"
#include "uart.h"


void uart4_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef us;


    /*
     * Enable clocks:
     * GPIOC = APB2
     * UART4 = APB1
     */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOC,
        ENABLE
    );

    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_UART4,
        ENABLE
    );


    /*
     * PC10 UART4 TX
     */
    gpio.GPIO_Pin =
        GPIO_Pin_10;

    gpio.GPIO_Mode =
        GPIO_Mode_AF_PP;

    gpio.GPIO_Speed =
        GPIO_Speed_50MHz;

    GPIO_Init(
        GPIOC,
        &gpio
    );


    /*
     * PC11 UART4 RX
     */
    gpio.GPIO_Pin =
        GPIO_Pin_11;

    gpio.GPIO_Mode =
        GPIO_Mode_IN_FLOATING;

    GPIO_Init(
        GPIOC,
        &gpio
    );


    us.USART_BaudRate = 115200;
    us.USART_WordLength = USART_WordLength_8b;
    us.USART_StopBits = USART_StopBits_1;
    us.USART_Parity = USART_Parity_No;
    us.USART_HardwareFlowControl =
        USART_HardwareFlowControl_None;

    us.USART_Mode =
        USART_Mode_Tx |
        USART_Mode_Rx;


    USART_Init(
        UART4,
        &us
    );


    USART_Cmd(
        UART4,
        ENABLE
    );
}



void uart4_putc(char c)
{
    while(!(UART4->SR & USART_SR_TXE));

    UART4->DR = c;
}


void uart4_print(const char *s)
{
    while(*s)
    {
        uart4_putc(*s++);
    }
}
