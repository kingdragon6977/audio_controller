#include "stm32f10x.h"
#include "i2s_rx.h"
#include "diagnostics.h"
#include "uart.h"

static uint16_t rx_buffer[I2S_RX_SAMPLES];
static uint32_t dma_error_flags;
static i2s_rx_debug_t debug_state;

static void print_hex16_local(uint16_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    uart2_putc(hex[(value >> 12) & 0x0Fu]);
    uart2_putc(hex[(value >> 8) & 0x0Fu]);
    uart2_putc(hex[(value >> 4) & 0x0Fu]);
    uart2_putc(hex[value & 0x0Fu]);
}

static void print_s32_local(int32_t value)
{
    uint32_t magnitude;
    char buf[12];
    unsigned int n = 0u;

    if (value < 0)
    {
        uart2_putc('-');
        magnitude = (uint32_t)(-(value + 1)) + 1u;
    }
    else
    {
        magnitude = (uint32_t)value;
    }

    do
    {
        buf[n++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
    } while (magnitude != 0u);

    while (n != 0u)
        uart2_putc(buf[--n]);
}

static void print_channel_stats(void)
{
    unsigned int i;
    int32_t min_l = 32767;
    int32_t max_l = -32768;
    int32_t min_r = 32767;
    int32_t max_r = -32768;
    int32_t peak_l = 0;
    int32_t peak_r = 0;
    int64_t sum_l = 0;
    int64_t sum_r = 0;

    /* DMA slot 0,2,4... and 1,3,5... are kept separate. Do not assume
     * left/right naming until the codec's WS phase is correlated with the
     * captured stream; report both streams independently first. */
    for (i = 0u; i + 1u < I2S_RX_SAMPLES; i += 2u)
    {
        int32_t left = (int16_t)rx_buffer[i];
        int32_t right = (int16_t)rx_buffer[i + 1u];
        int32_t abs_left = (left < 0) ? -left : left;
        int32_t abs_right = (right < 0) ? -right : right;

        if (left < min_l) min_l = left;
        if (left > max_l) max_l = left;
        if (right < min_r) min_r = right;
        if (right > max_r) max_r = right;
        if (abs_left > peak_l) peak_l = abs_left;
        if (abs_right > peak_r) peak_r = abs_right;
        sum_l += left;
        sum_r += right;
    }

    uart2_print("\r\nI2S CHANNEL ANALYSIS (RAW DMA SLOT STREAM)\r\n");
    uart2_print("  Even slots = stream A; odd slots = stream B\r\n");
    uart2_print("  Channel naming intentionally not assumed yet.\r\n");

    uart2_print("  STREAM A MIN/MAX = ");
    print_s32_local(min_l);
    uart2_print(" / ");
    print_s32_local(max_l);
    uart2_print("\r\n");
    uart2_print("  STREAM A MEAN    = ");
    print_s32_local((int32_t)(sum_l / (I2S_RX_SAMPLES / 2u)));
    uart2_print("\r\n");
    uart2_print("  STREAM A PEAK    = ");
    print_s32_local(peak_l);
    uart2_print("\r\n");

    uart2_print("  STREAM B MIN/MAX = ");
    print_s32_local(min_r);
    uart2_print(" / ");
    print_s32_local(max_r);
    uart2_print("\r\n");
    uart2_print("  STREAM B MEAN    = ");
    print_s32_local((int32_t)(sum_r / (I2S_RX_SAMPLES / 2u)));
    uart2_print("\r\n");
    uart2_print("  STREAM B PEAK    = ");
    print_s32_local(peak_r);
    uart2_print("\r\n");

    uart2_print("  STREAM A FIRST 16: ");
    for (i = 0u; i < 16u && (2u * i) < I2S_RX_SAMPLES; ++i)
    {
        uart2_print("0x");
        print_hex16_local(rx_buffer[2u * i]);
        if (i != 15u) uart2_print(" ");
    }
    uart2_print("\r\n");

    uart2_print("  STREAM B FIRST 16: ");
    for (i = 0u; i < 16u && (2u * i + 1u) < I2S_RX_SAMPLES; ++i)
    {
        uart2_print("0x");
        print_hex16_local(rx_buffer[2u * i + 1u]);
        if (i != 15u) uart2_print(" ");
    }
    uart2_print("\r\n");
}

int i2s_rx_start_capture(void)
{
    DMA_InitTypeDef dma;
    I2S_InitTypeDef i2s;
    volatile uint16_t dummy_dr;
    volatile uint16_t dummy_sr;

    dma_error_flags = 0u;
    debug_state.sr_before = SPI2->SR;
    debug_state.sr_after_enable = 0u;
    debug_state.sr_after_capture = 0u;
    debug_state.dr_after_capture = 0u;
    debug_state.dma_isr_after_capture = 0u;
    debug_state.dma_cndtr_after_capture = 0u;

    /* Never activate the MCU I2S receiver unless its external-master pins
     * have already passed the GPIO safety gate. */
    if (!diagnostics_i2s2_safe())
        return 0;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    /* Keep the receiver disabled while DMA is prepared. */
    I2S_Cmd(SPI2, DISABLE);
    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_GL4 | DMA1_FLAG_TC4 |
                  DMA1_FLAG_HT4 | DMA1_FLAG_TE4);

    /* Clear a stale RXNE/OVR condition before the new capture. On STM32F1,
     * an overrun is cleared by reading DR followed by SR. Do this while I2S
     * is disabled so an old status bit cannot contaminate the new capture. */
    dummy_dr = SPI2->DR;
    dummy_sr = SPI2->SR;
    (void)dummy_dr;
    (void)dummy_sr;

    /* SPI2 RX is DMA1 Channel 4 on STM32F1. One DMA item is one 16-bit I2S
     * slot. Normal mode gives us a deterministic finite capture. */
    dma.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)rx_buffer;
    dma.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma.DMA_BufferSize = I2S_RX_SAMPLES;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma.DMA_Mode = DMA_Mode_Normal;
    dma.DMA_Priority = DMA_Priority_VeryHigh;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel4, &dma);

    /* STM32 is a Philips-I2S slave receiver. The codec supplies WS and BCLK.
     * AudioFreq is informational for slave operation; no MCU clock is driven. */
    i2s.I2S_Mode = I2S_Mode_SlaveRx;
    i2s.I2S_Standard = I2S_Standard_Phillips;
    i2s.I2S_DataFormat = I2S_DataFormat_16b;
    i2s.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
    i2s.I2S_AudioFreq = I2S_AudioFreq_48k;
    i2s.I2S_CPOL = I2S_CPOL_Low;
    I2S_Init(SPI2, &i2s);

    /* DMA must be ready before I2S is allowed to observe the live clock. */
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, ENABLE);
    DMA_Cmd(DMA1_Channel4, ENABLE);
    I2S_Cmd(SPI2, ENABLE);
    debug_state.sr_after_enable = SPI2->SR;

    return 1;
}

int i2s_rx_capture_complete(void)
{
    if (DMA_GetFlagStatus(DMA1_FLAG_TC4))
    {
        debug_state.sr_after_capture = SPI2->SR;
        debug_state.dr_after_capture = SPI2->DR;
        debug_state.dma_isr_after_capture = DMA1->ISR;
        debug_state.dma_cndtr_after_capture = DMA1_Channel4->CNDTR;
        print_channel_stats();
        return 1;
    }
    return 0;
}

uint32_t i2s_rx_error_flags(void)
{
    if (DMA_GetFlagStatus(DMA1_FLAG_TE4))
        dma_error_flags |= DMA1_FLAG_TE4;
    return dma_error_flags;
}

const uint16_t *i2s_rx_buffer(void)
{
    return rx_buffer;
}

void i2s_rx_get_debug(i2s_rx_debug_t *debug)
{
    if (debug)
        *debug = debug_state;
}

void i2s_rx_stop(void)
{
    /* Disable DMA request before stopping the peripheral. */
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    I2S_Cmd(SPI2, DISABLE);
    DMA_Cmd(DMA1_Channel4, DISABLE);
}
