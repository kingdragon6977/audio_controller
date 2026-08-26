#include "stm32f10x.h"
#include "i2s_rx.h"
#include "diagnostics.h"

static uint16_t rx_buffer[I2S_RX_SAMPLES];
static uint32_t dma_error_flags;
static i2s_rx_debug_t debug_state;

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
