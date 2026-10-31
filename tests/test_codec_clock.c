#include <assert.h>
#include <stdio.h>
#include "stm32f10x.h"
#include "codec.h"
GPIO_TypeDef ga,gb;
RCC_TypeDef rcc;
AFIO_TypeDef afio;
SysTick_Type systick;
TIM_TypeDef tim;
SPI_TypeDef spi;
DMA_Channel_TypeDef dma;
RCC_ClocksTypeDef clocks;
GPIO_TypeDef *clock_test_gpio(void) { return &gb; }
static uint8_t regs[2][256], page, last_reg;
static int clock_ok=1, corrupt, writes, probes, nack_probe;
int audio_clock_check_wclk(int verbose) { (void)verbose;return 1; }
int audio_frequency_measure(uint32_t *hz,uint32_t *edges,uint32_t *gate) {
 assert(!codec_profile_ready());assert(regs[0][0x52]==0x88);
 assert(regs[0][0x05] == (probes==2 ? 0x11 : 0x91));
 assert(regs[0][0x04] == (probes==2 ? 0 : 3));
 assert(regs[0][0x12] == (probes==2 ? 0x81 : 0x88));
 *hz=probes==0 ? 48000u : 46875u;*edges=*hz/20;*gate=50000;
 ++probes;return 1;
}
int audio_frequency_measure_dout(uint32_t *hz,uint32_t *edges,uint32_t *gate) {
 assert(!codec_profile_ready());assert(regs[0][0x51]==0 && regs[0][0x52]==0x88);
 assert(regs[0][0x05]==0x11 && regs[0][0x04]==0);
 assert(regs[0][0x19]==0 && regs[0][0x1a]==0x80 && regs[0][0x35]==0x16);
 *hz=93750;*edges=1875;*gate=20000;return 1;
}
int audio_clock_ready(void) { return clock_ok; }
void audio_clock_invalidate_rate(void) {}
void uart2_print(const char *s) { (void)s; }
void uart2_putc(char c) { (void)c; }
int i2c1_write(uint8_t address,uint8_t reg,uint8_t data) {
    assert(address==0x18); ++writes;
    if(nack_probe && probes==1) return 0;
    if (reg==0) { page=data; last_reg=0; return 1; }
    if (page==0 && reg==8) assert(last_reg==7); /* required adjacent D pair */
    if(page==0 && reg==0x35 && (data==0x14 || data==0x15))
      gb.IDR=(gb.IDR & ~(1u<<15)) | ((uint32_t)(data&1u)<<15);
    regs[page][reg]=data;last_reg=reg; return 1;
}
int i2c1_read(uint8_t address,uint8_t reg,uint8_t *data) {
    assert(address==0x18);*data=regs[page][reg];
    if(corrupt && page==0 && reg==7) *data^=1;
    return 1;
}
int i2c1_probe(uint8_t address) { (void)address; return 1; }
int main(void) {
    assert(codec_apply_audio_profile());
    assert(codec_profile_ready());
    assert(regs[0][6]==8 && regs[0][7]==7 && regs[0][8]==0x80);
    assert(regs[0][0x12]==0x88 && regs[0][0x13]==0x82 && regs[0][0x14]==128);
    codec_clock_test();assert(probes==3);assert(codec_profile_ready());
    assert(regs[0][0x04]==3 && regs[0][0x07]==7 && regs[0][0x08]==0x80);
    assert(regs[0][0x12]==0x88 && regs[0][0x52]==0);
    probes=0;nack_probe=1;gb.ODR=GPIO_Pin_14;
    codec_clock_test();assert(probes==1);assert(!codec_profile_ready());
    assert(!(gb.ODR & GPIO_Pin_14));nack_probe=0;
    codec_mclk_test();assert(codec_profile_ready());assert(regs[0][0x35]==0x12);
    assert(regs[0][0x05]==0x91 && regs[0][0x52]==0);
    dma.CCR=1;gb.ODR=GPIO_Pin_14;codec_mclk_test();assert(!codec_profile_ready());
    assert(!(gb.ODR & GPIO_Pin_14));dma.CCR=0;
    corrupt=1; gb.ODR=GPIO_Pin_14;
    assert(!codec_apply_audio_profile());assert(!codec_profile_ready());
    assert((gb.ODR & GPIO_Pin_14)==0); /* mismatch holds codec reset */
    corrupt=0;clock_ok=0;writes=0;
    assert(!codec_apply_audio_profile());assert(writes==0);
    codec_reset();assert((gb.ODR & GPIO_Pin_14)==0);
    puts("codec 12-MHz PLL readback and failure-gate tests PASS");
}
