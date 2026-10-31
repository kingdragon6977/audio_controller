#pragma once
#include "../clock_stubs/stm32f10x.h"
typedef struct { uint32_t SR,DR,BRR,CR1,CR2,CR3; } USART_TypeDef;
typedef struct { uint32_t USART_BaudRate,USART_WordLength,USART_StopBits,USART_Parity,USART_HardwareFlowControl,USART_Mode; } USART_InitTypeDef;
extern USART_TypeDef usart1,usart2;
USART_TypeDef *uart_test_usart2(void);
#define USART1 (&usart1)
#define USART2 uart_test_usart2()
#define GPIO_Pin_2 (1u<<2)
#define GPIO_Pin_3 (1u<<3)
#define GPIO_Pin_9 (1u<<9)
#define GPIO_Pin_10 (1u<<10)
#define RCC_APB1Periph_USART2 (1u<<17)
#define RCC_APB2Periph_USART1 (1u<<14)
#define USART_WordLength_8b 0u
#define USART_StopBits_1 0u
#define USART_Parity_No 0u
#define USART_HardwareFlowControl_None 0u
#define USART_Mode_Tx 8u
#define USART_Mode_Rx 4u
#define USART_SR_TXE (1u<<7)
#define USART_SR_RXNE (1u<<5)
#define USART_SR_ORE (1u<<3)
#define USART_SR_FE (1u<<1)
#define USART_SR_NE (1u<<2)
static inline void USART_Init(USART_TypeDef *u,USART_InitTypeDef *s) { (void)u;(void)s; }
static inline void USART_Cmd(USART_TypeDef *u,unsigned enable) { (void)u;(void)enable; }
