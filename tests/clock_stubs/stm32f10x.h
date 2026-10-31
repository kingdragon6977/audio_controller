#pragma once
#include <stdint.h>
#include <string.h>
typedef struct { uint32_t CRH, ODR, IDR; } GPIO_TypeDef;
typedef struct { unsigned GPIO_Pin,GPIO_Mode,GPIO_Speed; } GPIO_InitTypeDef;
typedef struct { uint32_t CR,CFGR,APB2ENR,APB1ENR; } RCC_TypeDef;
typedef struct { uint32_t MAPR,EXTICR[4]; } AFIO_TypeDef;
typedef struct { uint32_t CTRL; } SysTick_Type;
typedef struct { uint32_t CR1,CR2,SMCR,DIER,PSC,ARR,CCR1,RCR,CCMR1,CCMR2,CCER,BDTR,EGR,SR,CNT; } TIM_TypeDef;
typedef struct { uint32_t SYSCLK_Frequency,HCLK_Frequency,PCLK2_Frequency,PCLK1_Frequency; } RCC_ClocksTypeDef;
extern GPIO_TypeDef ga,gb;
extern RCC_TypeDef rcc;
extern AFIO_TypeDef afio;
extern SysTick_Type systick;
extern TIM_TypeDef tim;
extern RCC_ClocksTypeDef clocks;
GPIO_TypeDef *clock_test_gpio(void);
#define GPIOA (&ga)
#define GPIOB clock_test_gpio()
#define RCC (&rcc)
#define AFIO (&afio)
#define SysTick (&systick)
#define TIM1 (&tim)
#define ENABLE 1u
#define DISABLE 0u
#define GPIO_Pin_8 (1u<<8)
#define GPIO_Pin_12 (1u<<12)
#define GPIO_Pin_14 (1u<<14)
#define GPIO_Mode_AF_PP 0xbu
#define GPIO_Mode_IN_FLOATING 4u
#define GPIO_Mode_Out_PP 2u
#define GPIO_Speed_50MHz 3u
#define GPIO_Speed_2MHz 2u
#define RCC_APB2Periph_GPIOA 4u
#define RCC_APB2Periph_GPIOB 8u
#define RCC_APB2Periph_AFIO 1u
#define RCC_APB2Periph_TIM1 (1u<<11)
#define RCC_CR_HSERDY (1u<<17)
#define RCC_CR_PLLRDY (1u<<25)
#define RCC_CFGR_SWS 12u
#define RCC_CFGR_SWS_PLL 8u
#define RCC_CFGR_PLLSRC (1u<<16)
#define RCC_CFGR_MCO (7u<<24)
#define RCC_CFGR_PPRE2 (7u<<11)
#define GPIO_FullRemap_TIM1 0xc0u
#define TIM_CR1_CEN 1u
#define TIM_CR1_ARPE 128u
#define TIM_CCER_CC1E 1u
#define TIM_BDTR_MOE (1u<<15)
#define TIM_EGR_UG 1u
#define SysTick_CTRL_ENABLE_Msk 1u
static inline void RCC_GetClocksFreq(RCC_ClocksTypeDef *c) { *c=clocks; }
static inline void RCC_APB2PeriphClockCmd(unsigned p,unsigned e) { if(e)rcc.APB2ENR|=p;else rcc.APB2ENR&=~p; }
static inline void RCC_APB2PeriphResetCmd(unsigned p,unsigned e) { (void)p;if(e)memset(&tim,0,sizeof(tim)); }
static inline void GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *g) { if(g->GPIO_Pin==GPIO_Pin_8)p->CRH=(p->CRH&~15u)|g->GPIO_Mode; }
static inline void GPIO_ResetBits(GPIO_TypeDef *p,unsigned b) { p->ODR&=~b; }
static inline void GPIO_SetBits(GPIO_TypeDef *p,unsigned b) { p->ODR|=b; }

typedef struct { uint32_t IMR,EMR,PR,RTSR,FTSR; } EXTI_TypeDef;
typedef struct { uint32_t ISER[2],pending,priority; } NVIC_TypeDef;
typedef struct { uint32_t I2SCFGR; } SPI_TypeDef;
typedef struct { uint32_t CCR; } DMA_Channel_TypeDef;
extern EXTI_TypeDef exti;
extern NVIC_TypeDef nvic;
extern SPI_TypeDef spi;
extern DMA_Channel_TypeDef dma;
extern TIM_TypeDef timer2;
extern unsigned primask;
TIM_TypeDef *frequency_test_timer(void);
#define TIM2 frequency_test_timer()
#define EXTI (&exti)
#define NVIC (&nvic)
#define SPI2 (&spi)
#define DMA1_Channel4 (&dma)
#define SPI_I2SCFGR_I2SE (1u<<10)
#define RCC_APB1Periph_TIM2 1u
#define EXTI15_10_IRQn 40u
static inline unsigned __get_PRIMASK(void) { return primask; }
static inline unsigned NVIC_GetPendingIRQ(unsigned irq) { (void)irq;return nvic.pending; }
static inline unsigned NVIC_GetPriority(unsigned irq) { (void)irq;return nvic.priority; }
static inline void NVIC_SetPriority(unsigned irq,unsigned p) { (void)irq;nvic.priority=p; }
static inline void NVIC_ClearPendingIRQ(unsigned irq) { (void)irq;nvic.pending=0; }
static inline void NVIC_EnableIRQ(unsigned irq) { nvic.ISER[irq/32]|=1u<<(irq%32); }
static inline void NVIC_DisableIRQ(unsigned irq) { nvic.ISER[irq/32]&=~(1u<<(irq%32)); }
static inline void RCC_APB1PeriphClockCmd(unsigned p,unsigned e) { if(e)rcc.APB1ENR|=p;else rcc.APB1ENR&=~p; }
static inline void RCC_APB1PeriphResetCmd(unsigned p,unsigned e) { (void)p;if(e)memset(&timer2,0,sizeof(timer2)); }
