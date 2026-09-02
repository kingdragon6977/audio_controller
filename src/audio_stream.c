#include "stm32f10x.h"
#include "audio_stream.h"
#include "diagnostics.h"
#include "i2s_rx.h"
#include "uart.h"

#define STREAM_DMA_SLOTS       256u
#define STREAM_HALF_SLOTS      128u
#define STREAM_PACKET_SAMPLES   64u
#define STREAM_SAMPLE_RATE   24000u
#define ESP_CTRL_READY         0xF0u
#define ESP_CTRL_STOP          0xF1u

static uint16_t stream_dma_buffer[STREAM_DMA_SLOTS];
static int16_t packet_samples[STREAM_PACKET_SAMPLES];
static uint32_t packet_fill;
static uint16_t packet_sequence;
static uint32_t packet_count;
static uint32_t dma_error_count;
static uint8_t running;
static uint8_t esp_ready;
static uint8_t ws_high_at_dma;
static uint8_t start_message_printed;

static void clear_spi2_rx_state(void)
{
    volatile uint16_t value;

    if ((SPI2->SR & SPI_SR_RXNE) != 0u)
    {
        value = SPI2->DR;
        (void)value;
    }

    if ((SPI2->SR & SPI_SR_OVR) != 0u)
    {
        value = SPI2->DR;
        value = SPI2->SR;
        (void)value;
    }
}

static int wait_for_ws_falling_edge(void)
{
    uint32_t timeout = 400000u;

    while ((GPIOB->IDR & GPIO_Pin_12) == 0u)
    {
        if (timeout-- == 0u)
            return 0;
    }

    timeout = 400000u;
    while ((GPIOB->IDR & GPIO_Pin_12) != 0u)
    {
        if (timeout-- == 0u)
            return 0;
    }

    return 1;
}

static int sync_receiver_before_dma(void)
{
    uint32_t timeout;
    uint16_t sr;
    volatile uint16_t value;
    unsigned int words_seen = 0u;

    I2S_Cmd(SPI2, DISABLE);
    clear_spi2_rx_state();

    if (!wait_for_ws_falling_edge())
        return 0;

    I2S_Cmd(SPI2, ENABLE);

    while (words_seen < 4u)
    {
        timeout = 400000u;
        while ((SPI2->SR & SPI_SR_RXNE) == 0u)
        {
            if ((SPI2->SR & SPI_SR_OVR) != 0u)
            {
                clear_spi2_rx_state();
                I2S_Cmd(SPI2, DISABLE);
                return 0;
            }

            if (timeout-- == 0u)
            {
                I2S_Cmd(SPI2, DISABLE);
                return 0;
            }
        }

        sr = SPI2->SR;
        value = SPI2->DR;
        (void)value;
        words_seen++;

        if ((sr & SPI_SR_OVR) != 0u)
        {
            clear_spi2_rx_state();
            I2S_Cmd(SPI2, DISABLE);
            return 0;
        }

        if ((sr & I2S_FLAG_CHSIDE) != 0u)
            return 1;
    }

    I2S_Cmd(SPI2, DISABLE);
    return 0;
}

static void send_packet(void)
{
    uint8_t header[10];

    header[0] = 0xA5u;
    header[1] = 0x5Au;
    header[2] = 0x01u;
    header[3] = 0x01u;
    header[4] = (uint8_t)(packet_sequence & 0xFFu);
    header[5] = (uint8_t)(packet_sequence >> 8);
    header[6] = (uint8_t)(STREAM_PACKET_SAMPLES & 0xFFu);
    header[7] = (uint8_t)(STREAM_PACKET_SAMPLES >> 8);
    header[8] = (uint8_t)(STREAM_SAMPLE_RATE & 0xFFu);
    header[9] = (uint8_t)(STREAM_SAMPLE_RATE >> 8);

    esp_uart_write(header, sizeof(header));
    esp_uart_write((const uint8_t *)packet_samples, sizeof(packet_samples));

    packet_sequence++;
    packet_count++;
    packet_fill = 0u;
}

static void process_half(unsigned int first_slot)
{
    unsigned int frame;

    for (frame = 0u; frame < STREAM_HALF_SLOTS / 2u; frame += 2u)
    {
        unsigned int base = first_slot + frame * 2u;
        unsigned int left_index = base + (ws_high_at_dma ? 1u : 0u);

        packet_samples[packet_fill++] = (int16_t)stream_dma_buffer[left_index];

        if (packet_fill == STREAM_PACKET_SAMPLES)
            send_packet();
    }
}

int audio_stream_start(void)
{
    DMA_InitTypeDef dma;
    I2S_InitTypeDef i2s;
    volatile uint16_t dummy_dr;
    volatile uint16_t dummy_sr;

    if (running)
        return 1;

    if (!diagnostics_i2s2_safe())
        return 0;

    i2s_rx_stop();

    packet_fill = 0u;
    packet_sequence = 0u;
    packet_count = 0u;
    dma_error_count = 0u;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    I2S_Cmd(SPI2, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_GL4 | DMA1_FLAG_TC4 |
                  DMA1_FLAG_HT4 | DMA1_FLAG_TE4);

    dummy_dr = SPI2->DR;
    dummy_sr = SPI2->SR;
    (void)dummy_dr;
    (void)dummy_sr;

    dma.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)stream_dma_buffer;
    dma.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma.DMA_BufferSize = STREAM_DMA_SLOTS;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma.DMA_Mode = DMA_Mode_Circular;
    dma.DMA_Priority = DMA_Priority_VeryHigh;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel4, &dma);

    i2s.I2S_Mode = I2S_Mode_SlaveRx;
    i2s.I2S_Standard = I2S_Standard_Phillips;
    i2s.I2S_DataFormat = I2S_DataFormat_16b;
    i2s.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
    i2s.I2S_AudioFreq = I2S_AudioFreq_48k;
    i2s.I2S_CPOL = I2S_CPOL_Low;
    I2S_Init(SPI2, &i2s);

    if (!sync_receiver_before_dma())
        return 0;

    ws_high_at_dma = (GPIOB->IDR & GPIO_Pin_12) ? 1u : 0u;

    DMA_ClearFlag(DMA1_FLAG_GL4 | DMA1_FLAG_TC4 |
                  DMA1_FLAG_HT4 | DMA1_FLAG_TE4);
    DMA_SetCurrDataCounter(DMA1_Channel4, STREAM_DMA_SLOTS);
    DMA_Cmd(DMA1_Channel4, ENABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, ENABLE);

    running = 1u;
    return 1;
}

void audio_stream_stop(void)
{
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    DMA_Cmd(DMA1_Channel4, DISABLE);
    I2S_Cmd(SPI2, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_GL4 | DMA1_FLAG_TC4 |
                  DMA1_FLAG_HT4 | DMA1_FLAG_TE4);
    clear_spi2_rx_state();
    running = 0u;
    packet_fill = 0u;
}

void audio_stream_task(void)
{
    while (esp_uart_available())
    {
        uint8_t control = (uint8_t)esp_uart_getc();

        if (control == ESP_CTRL_READY)
        {
            esp_ready = 1u;
            start_message_printed = 0u;
            uart2_print("ESP-01: Wi-Fi/UDP ready; enabling PCM stream.\r\n");
        }
        else if (control == ESP_CTRL_STOP)
        {
            esp_ready = 0u;
            audio_stream_stop();
            uart2_print("ESP-01: stream stopped by receiver.\r\n");
        }
    }

    /* A diagnostic capture/meter may temporarily take DMA1 CH4 away. */
    if (running && (DMA1_Channel4->CCR & 0x0001u) == 0u)
        running = 0u;

    if (!running)
    {
        if (esp_ready)
        {
            if (audio_stream_start())
            {
                if (!start_message_printed)
                {
                    uart2_print("AUDIO STREAM: 24 kHz mono PCM16 -> USART1 2 Mbaud -> ESP-01.\r\n");
                    start_message_printed = 1u;
                }
            }
        }
        return;
    }

    if (DMA_GetFlagStatus(DMA1_FLAG_TE4))
    {
        dma_error_count++;
        audio_stream_stop();
        uart2_print("AUDIO STREAM: DMA error; will retry while ESP remains ready.\r\n");
        return;
    }

    if (DMA_GetFlagStatus(DMA1_FLAG_HT4))
    {
        DMA_ClearFlag(DMA1_FLAG_HT4);
        process_half(0u);
    }

    if (DMA_GetFlagStatus(DMA1_FLAG_TC4))
    {
        DMA_ClearFlag(DMA1_FLAG_TC4);
        process_half(STREAM_HALF_SLOTS);
    }
}

int audio_stream_running(void)
{
    return running ? 1 : 0;
}

uint32_t audio_stream_packets(void)
{
    return packet_count;
}

uint32_t audio_stream_dma_errors(void)
{
    return dma_error_count;
}
