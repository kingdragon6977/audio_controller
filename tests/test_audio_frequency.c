#include <assert.h>
#include <stdio.h>
#include "stm32f10x.h"
#include "audio_frequency.h"
GPIO_TypeDef ga,gb;
RCC_TypeDef rcc;
AFIO_TypeDef afio;
SysTick_Type systick;
TIM_TypeDef tim,timer2;
RCC_ClocksTypeDef clocks;
EXTI_TypeDef exti;
NVIC_TypeDef nvic;
SPI_TypeDef spi;
DMA_Channel_TypeDef dma;
unsigned primask;
static uint32_t rate, fraction;
static int stopped, overflow;
void EXTI15_10_IRQHandler(void);
void EXTI9_5_IRQHandler(void);
GPIO_TypeDef *clock_test_gpio(void) { return &gb; }
TIM_TypeDef *frequency_test_timer(void) {
    if ((timer2.CR1 & 1u) && !stopped) {
        timer2.CNT += 10u;
        if(overflow)timer2.SR=1;
        if (((exti.IMR & ((1u<<12)|(1u<<15))) && (nvic.ISER[1] & (1u<<8))) ||
            ((exti.IMR & (1u<<8)) && (nvic.ISER[0] & (1u<<23)))) {
            fraction += rate * 10u;
            while(fraction >= 1000000u) {
                fraction -= 1000000u;
                if (exti.IMR & (1u<<8)) {exti.PR=1u<<8;EXTI9_5_IRQHandler();}
                else {exti.PR=(exti.IMR & (1u<<15)) ? (1u<<15) : (1u<<12);EXTI15_10_IRQHandler();}
                exti.PR=0;
            }
        }
    }
    return &timer2;
}
static void reset(void) {
 memset(&rcc,0,sizeof(rcc)); memset(&exti,0,sizeof(exti));
 memset(&nvic,0,sizeof(nvic));memset(&afio,0,sizeof(afio));
 memset(&timer2,0,sizeof(timer2)); memset(&gb,0,sizeof(gb));memset(&ga,0,sizeof(ga));memset(&tim,0,sizeof(tim));
 spi.I2SCFGR=0;dma.CCR=0;primask=0;fraction=0;rate=48000;stopped=overflow=0;
 clocks.HCLK_Frequency=72000000;clocks.PCLK1_Frequency=36000000;
 afio.EXTICR[3]=0xABC0;exti.RTSR=2;exti.FTSR=4;nvic.priority=7;
}
int main(void) {
 uint32_t hz,edges,gate;
 reset();assert(audio_frequency_measure(&hz,&edges,&gate));
 assert(hz>=47900 && hz<=48100);assert(edges>=2399 && edges<=2402);
 assert(gate>=50000 && gate<=50100);
 assert(!rcc.APB1ENR && !rcc.APB2ENR && !exti.IMR && !nvic.ISER[1]);
 assert(afio.EXTICR[3]==0xABC0 && exti.RTSR==2 && exti.FTSR==4 && nvic.priority==7);
 reset();rate=17100;assert(audio_frequency_measure(&hz,&edges,&gate));assert(hz>17000 && hz<17200);
 reset();rate=100000;assert(!audio_frequency_measure(&hz,&edges,&gate));assert(!hz && edges>=3000);
 reset();rate=0;assert(!audio_frequency_measure(&hz,&edges,&gate));assert(!hz);
 reset();stopped=1;assert(!audio_frequency_measure(&hz,&edges,&gate));assert(!rcc.APB1ENR);
 reset();overflow=1;assert(!audio_frequency_measure(&hz,&edges,&gate));assert(!hz);
 reset();rcc.APB1ENR=1;assert(!audio_frequency_measure(&hz,&edges,&gate));assert(rcc.APB1ENR==1);
 reset();exti.IMR=1u<<13;assert(!audio_frequency_measure(&hz,&edges,&gate));
 reset();nvic.ISER[1]=1u<<8;assert(!audio_frequency_measure(&hz,&edges,&gate));
 reset();primask=1;assert(!audio_frequency_measure(&hz,&edges,&gate));
 reset();gb.CRH=1u<<16;assert(!audio_frequency_measure(&hz,&edges,&gate));
 reset();spi.I2SCFGR=SPI_I2SCFGR_I2SE;assert(!audio_frequency_measure(&hz,&edges,&gate));
 reset();dma.CCR=1;assert(!audio_frequency_measure(&hz,&edges,&gate));
 reset();ga.CRH=11;tim.ARR=5999;tim.CCR1=3000;tim.CR1=1;tim.CCER=1;tim.BDTR=TIM_BDTR_MOE;
 rate=12000;afio.EXTICR[2]=0xabc1;
 assert(audio_frequency_measure_pa8(&hz,&edges,&gate));assert(hz>=11950 && hz<=12050);
 assert(afio.EXTICR[2]==0xabc1 && !nvic.ISER[0] && !exti.IMR);
 gb.IDR=GPIO_Pin_14;assert(!audio_frequency_measure_pa8(&hz,&edges,&gate));
 gb.IDR=0;tim.ARR=5;assert(!audio_frequency_measure_pa8(&hz,&edges,&gate));
 tim.ARR=5999;exti.IMR=1u<<7;assert(!audio_frequency_measure_pa8(&hz,&edges,&gate));
 reset();rate=93750;afio.EXTICR[3]=0xabc0;
 assert(audio_frequency_measure_dout(&hz,&edges,&gate));assert(gate>=20000 && gate<=20100);
 assert(hz>93500 && hz<94000);assert(afio.EXTICR[3]==0xabc0 && !exti.IMR);
 gb.CRH=1u<<28;assert(!audio_frequency_measure_dout(&hz,&edges,&gate));
 puts("EXTI/TIM2 frequency measurement tests PASS");
}
