#include "stm32f10x.h"
#include "audio.h"
#include "i2s.h"


void dma_i2s_init(void)
{

    DMA_InitTypeDef dma;


    RCC_AHBPeriphClockCmd(
        RCC_AHBPeriph_DMA1,
        ENABLE);



    DMA_DeInit(DMA1_Channel4);



    dma.DMA_PeripheralBaseAddr =
        (uint32_t)&SPI2->DR;


    


    dma.DMA_DIR =
        DMA_DIR_PeripheralSRC;


    dma.DMA_BufferSize =
        512;


    dma.DMA_PeripheralInc =
        DMA_PeripheralInc_Disable;


    dma.DMA_MemoryInc =
        DMA_MemoryInc_Enable;


    dma.DMA_PeripheralDataSize =
        DMA_PeripheralDataSize_HalfWord;


    dma.DMA_MemoryDataSize =
        DMA_MemoryDataSize_HalfWord;


    dma.DMA_Mode =
        DMA_Mode_Circular;


    dma.DMA_Priority =
        DMA_Priority_High;


    dma.DMA_M2M =
        DMA_M2M_Disable;


    DMA_Init(
        DMA1_Channel4,
        &dma);



    DMA_ITConfig(
        DMA1_Channel4,
        DMA_IT_TC,
        ENABLE);



    NVIC_EnableIRQ(
        DMA1_Channel4_IRQn);



    DMA_Cmd(
        DMA1_Channel4,
        ENABLE);



    SPI_I2S_DMACmd(
        SPI2,
        SPI_I2S_DMAReq_Rx,
        ENABLE);

}



void DMA1_Channel4_IRQHandler(void)
{

    if(DMA_GetITStatus(
        DMA1_IT_TC4))
    {

        DMA_ClearITPendingBit(
            DMA1_IT_TC4);

audio_blocks++;
    }

}
