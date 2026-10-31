#include <assert.h>
#include <stdio.h>
#include "stm32f10x.h"
#include "audio_clock.h"
GPIO_TypeDef ga,gb;
RCC_TypeDef rcc;
AFIO_TypeDef afio;
SysTick_Type systick;
TIM_TypeDef tim;
SPI_TypeDef spi;
DMA_Channel_TypeDef dma;
static unsigned pad_rate=12000u;
int audio_frequency_measure_pa8(uint32_t *hz,uint32_t *edges,uint32_t *gate) {
 assert(tim.ARR==5999 && tim.CCR1==3000 && tim.CCER==1);
 assert(!(gb.ODR & GPIO_Pin_14));
 *hz=pad_rate;*edges=pad_rate/20;*gate=50000;return pad_rate!=0;
}
RCC_ClocksTypeDef clocks;
static uint32_t us, rate=48000u;
static int stalled_tick;
int audio_frequency_measure(uint32_t *hz,uint32_t *edges,uint32_t *gate) { *hz=stalled_tick ? 0u : rate; *gate=50000; *edges=*hz/20; return !stalled_tick && systick.CTRL; }
uint32_t board_millis(void) { if(!stalled_tick) ++us; return us/1000u; }
GPIO_TypeDef *clock_test_gpio(void) {
    gb.IDR = (((uint64_t)us*rate*2u/1000000u)&1u) ? GPIO_Pin_12 : 0u;
    return &gb;
}
void uart2_print(const char *s) { (void)s; }
static void reset(void) {
    memset(&ga,0,sizeof(ga)); memset(&tim,0,sizeof(tim));
    ga.CRH=0x44444444u; rcc.CR=RCC_CR_HSERDY|RCC_CR_PLLRDY;
    rcc.CFGR=RCC_CFGR_SWS_PLL|RCC_CFGR_PLLSRC; rcc.APB2ENR=0;
    afio.MAPR=0; systick.CTRL=1;
    clocks=(RCC_ClocksTypeDef){72000000u,72000000u,72000000u,36000000u};
    us=0; rate=48000u; stalled_tick=0;
}
int main(void) {
    reset(); clocks.PCLK2_Frequency=36000000;
    assert(!audio_clock_init()); assert(ga.CRH==0x44444444u);
    reset(); afio.MAPR=GPIO_FullRemap_TIM1;
    assert(!audio_clock_init()); assert(ga.CRH==0x44444444u);
    reset(); assert(audio_clock_init()); assert(audio_clock_ready());
    assert(ga.CRH==0x4444444bu); /* PA9/10 UART config preserved */
    assert(audio_clock_check_wclk(1)); assert(audio_clock_rate_valid());
    assert(audio_clock_wclk_hz()>=47520 && audio_clock_wclk_hz()<=48480);
    rate=17200; assert(!audio_clock_check_wclk(1)); assert(!audio_clock_rate_valid());
    rate=0; assert(!audio_clock_check_wclk(1));
    rate=48000; assert(audio_clock_check_wclk(1));
    tim.CCR1=2; assert(!audio_clock_ready()); assert(!audio_clock_check_wclk(1));
    tim.CCR1=3; stalled_tick=1; assert(!audio_clock_check_wclk(1));
    reset(); assert(audio_clock_init()); systick.CTRL=0; assert(!audio_clock_check_wclk(1));
    reset();assert(audio_clock_init());assert(audio_clock_test_output());assert(audio_clock_ready());
    pad_rate=0;assert(!audio_clock_test_output());assert(audio_clock_ready());
    pad_rate=11000;assert(!audio_clock_test_output());assert(audio_clock_ready());
    gb.ODR=GPIO_Pin_14;assert(!audio_clock_test_output());assert(tim.ARR==5);
    gb.ODR=0;dma.CCR=1;assert(!audio_clock_test_output());assert(tim.ARR==5);dma.CCR=0;
    puts("PA8 clock and WCLK preflight tests PASS");
}
