#pragma once
#include <stdint.h>
typedef struct { uint32_t ODR, IDR; } GPIO_TypeDef;
typedef struct { unsigned GPIO_Pin, GPIO_Mode, GPIO_Speed; } GPIO_InitTypeDef;
extern GPIO_TypeDef mock_gpioa, mock_gpiob;
#define GPIOA (&mock_gpioa)
#define GPIOB (&mock_gpiob)
#define GPIO_Pin_0 1u
#define GPIO_Pin_2 4u
#define GPIO_Pin_14 (1u << 14)
#define RCC_APB2Periph_GPIOA 1u
#define RCC_APB2Periph_GPIOB 2u
#define GPIO_Mode_Out_PP 1u
#define GPIO_Speed_2MHz 2u
#define GPIO_Speed_50MHz 50u
#define ENABLE 1u
extern uint32_t SystemCoreClock;
extern uint32_t systick_reload;
static inline void SystemCoreClockUpdate(void) {}
static inline unsigned SysTick_Config(uint32_t ticks) { systick_reload=ticks; return 0; }
static inline void RCC_APB2PeriphClockCmd(unsigned p, unsigned e) { (void)p; (void)e; }
static inline void GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *g) { (void)p; (void)g; }
static inline void GPIO_SetBits(GPIO_TypeDef *p, unsigned bits) { p->ODR |= bits; p->IDR |= bits; }
static inline void GPIO_ResetBits(GPIO_TypeDef *p, unsigned bits) { p->ODR &= ~bits; p->IDR &= ~bits; }
