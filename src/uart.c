#include "stm32f10x.h"
#include "uart.h"

/*
 * USART2: primary/debug CLI UART
 * PA2 = TX, PA3 = RX
 * 115200 8N1
 */
void uart2_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef us;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_2;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

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

int uart2_available(void)
{
    return (USART2->SR & USART_SR_RXNE) != 0;
}

char uart2_getc(void)
{
    return (char)(USART2->DR & 0xFF);
}

void uart2_putc(char c)
{
    while (!(USART2->SR & USART_SR_TXE))
        ;

    USART2->DR = (uint16_t)c;
}

void uart2_print(const char *s)
{
    while (*s)
        uart2_putc(*s++);
}

/*
 * USART1: ESP-01 high-speed PCM transport
 * PA9  = TX -> ESP GPIO3/RX
 * PA10 = RX <- ESP GPIO1/TX
 * 1000000 8N1
 *
 * 24 kHz mono PCM16 is 48 kB/s. A 1 Mbaud 8N1 UART carries about
 * 100 kB/s, leaving comfortable framing/headroom while improving
 * signal margin versus the previous 2 Mbaud bring-up setting.
 */
void esp_uart_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef us;

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1,
        ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    us.USART_BaudRate = 1000000;
    us.USART_WordLength = USART_WordLength_8b;
    us.USART_StopBits = USART_StopBits_1;
    us.USART_Parity = USART_Parity_No;
    us.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    us.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART1, &us);
    USART_Cmd(USART1, ENABLE);
}

int esp_uart_available(void)
{
    return (USART1->SR & USART_SR_RXNE) != 0;
}

char esp_uart_getc(void)
{
    return (char)(USART1->DR & 0xFF);
}

void esp_uart_putc(char c)
{
    while (!(USART1->SR & USART_SR_TXE))
        ;

    USART1->DR = (uint16_t)c;
}

void esp_uart_write(const uint8_t *data, uint32_t length)
{
    while (length--)
        esp_uart_putc((char)*data++);
}

void esp_uart_print(const char *s)
{
    while (*s)
        esp_uart_putc(*s++);
}
