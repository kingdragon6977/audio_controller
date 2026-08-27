#include "stm32f10x.h"
#include "i2s_rx.h"
#include "diagnostics.h"
#include "uart.h"

static uint16_t rx_buffer[I2S_RX_SAMPLES];
static uint32_t dma_error_flags;
static i2s_rx_debug_t debug_state;
static uint8_t capture_debug_latched;

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

static int wait_for_ws_falling_edge(void)
{
    uint32_t timeout;

    /* Philips I2S uses WS low for the left channel. Waiting for a complete
     * high-to-low transition gives the receiver a repeatable left-frame
     * boundary before SPI2 is enabled. */
    timeout = 200000u;
    while (((GPIOB->IDR & GPIO_Pin_12) == 0u) && timeout--)
        __asm__("nop");
    if (timeout == 0u)
        return 0;

    timeout = 200000u;
    while (((GPIOB->IDR & GPIO_Pin_12) != 0u) && timeout--)
        __asm__("nop");

    return (timeout != 0u) ? 1 : 0;
}

int i2s_rx_start_capture(void)
{
    DMA_InitTypeDef dma;
    I2S_InitTypeDef i2s;
    volatile uint16_t dummy_dr;
    volatile uint16_t dummy_sr;

    dma_error_flags = 0u;
    capture_debug_latched = 0u;
    debug_state.sr_before = SPI2->SR;
    debug_state.sr_after_enable = 0u;
    debug_state.sr_after_capture = 0u;
    debug_state.dr_after_capture = 0u;
    debug_state.dma_isr_after_capture = 0u;
    debug_state.dma_cndtr_after_capture = 0u;
    debug_state.ws_sync_ok = 0u;
    debug_state.ws_level_at_enable = 0u;

    if (!diagnostics_i2s2_safe())
        return 0;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    I2S_Cmd(SPI2, DISABLE);
    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_GL4 | DMA1_FLAG_TC4 |
                  DMA1_FLAG_HT4 | DMA1_FLAG_TE4);

    dummy_dr = SPI2->DR;
    dummy_sr = SPI2->SR;
    (void)dummy_dr;
    (void)dummy_sr;

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

    i2s.I2S_Mode = I2S_Mode_SlaveRx;
    i2s.I2S_Standard = I2S_Standard_Phillips;
    i2s.I2S_DataFormat = I2S_DataFormat_16b;
    i2s.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
    i2s.I2S_AudioFreq = I2S_AudioFreq_48k;
    i2s.I2S_CPOL = I2S_CPOL_Low;
    I2S_Init(SPI2, &i2s);

    /* Arm DMA first. No requests can occur while I2S is disabled. */
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, ENABLE);
    DMA_Cmd(DMA1_Channel4, ENABLE);

    /* Start on a known WS falling edge so even/odd DMA slots are repeatable
     * across reset, power-cycle, and CLI reboot tests. */
    if (!wait_for_ws_falling_edge())
    {
        SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
        DMA_Cmd(DMA1_Channel4, DISABLE);
        return 0;
    }

    debug_state.ws_sync_ok = 1u;
    debug_state.ws_level_at_enable =
        (GPIOB->IDR & GPIO_Pin_12) ? 1u : 0u;

    I2S_Cmd(SPI2, ENABLE);
    debug_state.sr_after_enable = SPI2->SR;

    return 1;
}

int i2s_rx_capture_complete(void)
{
    if (!DMA_GetFlagStatus(DMA1_FLAG_TC4))
        return 0;

    if (!capture_debug_latched)
    {
        debug_state.sr_after_capture = SPI2->SR;
        debug_state.dr_after_capture = SPI2->DR;
        debug_state.dma_isr_after_capture = DMA1->ISR;
        debug_state.dma_cndtr_after_capture = DMA1_Channel4->CNDTR;
        capture_debug_latched = 1u;
    }

    return 1;
}

void i2s_rx_print_analysis(void)
{
    unsigned int i;
    unsigned int pair_count = 0u;
    int32_t min_a = 32767;
    int32_t max_a = -32768;
    int32_t min_b = 32767;
    int32_t max_b = -32768;
    int32_t peak_a = 0;
    int32_t peak_b = 0;
    int64_t sum_a = 0;
    int64_t sum_b = 0;

    /* Discard the first captured stereo pair from statistics. Even with a
     * synchronized WS edge, this keeps startup latency from contaminating the
     * measurements. All remaining pairs preserve even/odd slot phase. */
    for (i = 2u; i + 1u < I2S_RX_SAMPLES; i += 2u)
    {
        int32_t a = (int16_t)rx_buffer[i];
        int32_t b = (int16_t)rx_buffer[i + 1u];
        int32_t abs_a = (a < 0) ? -a : a;
        int32_t abs_b = (b < 0) ? -b : b;

        if (a < min_a) min_a = a;
        if (a > max_a) max_a = a;
        if (b < min_b) min_b = b;
        if (b > max_b) max_b = b;
        if (abs_a > peak_a) peak_a = abs_a;
        if (abs_b > peak_b) peak_b = abs_b;
        sum_a += a;
        sum_b += b;
        pair_count++;
    }

    uart2_print("\r\nI2S FRAME-ALIGNED CHANNEL ANALYSIS\r\n");
    uart2_print("  WS sync     = ");
    uart2_print(debug_state.ws_sync_ok ? "PASS (falling edge before enable)\r\n"
                                       : "FAIL\r\n");
    uart2_print("  WS at enable= ");
    uart2_print(debug_state.ws_level_at_enable ? "HIGH\r\n" : "LOW\r\n");
    uart2_print("  Slot phase   = even stream A, odd stream B\r\n");
    uart2_print("  Statistics exclude first captured A/B pair\r\n");

    uart2_print("  STREAM A MIN/MAX = ");
    print_s32_local(min_a);
    uart2_print(" / ");
    print_s32_local(max_a);
    uart2_print("\r\n");
    uart2_print("  STREAM A MEAN    = ");
    print_s32_local(pair_count ? (int32_t)(sum_a / pair_count) : 0);
    uart2_print("\r\n");
    uart2_print("  STREAM A PEAK    = ");
    print_s32_local(peak_a);
    uart2_print("\r\n");

    uart2_print("  STREAM B MIN/MAX = ");
    print_s32_local(min_b);
    uart2_print(" / ");
    print_s32_local(max_b);
    uart2_print("\r\n");
    uart2_print("  STREAM B MEAN    = ");
    print_s32_local(pair_count ? (int32_t)(sum_b / pair_count) : 0);
    uart2_print("\r\n");
    uart2_print("  STREAM B PEAK    = ");
    print_s32_local(peak_b);
    uart2_print("\r\n");

    uart2_print("  FIRST 12 RAW FRAMES:\r\n");
    for (i = 0u; i < 12u; ++i)
    {
        unsigned int base = 2u * i;
        uart2_print("    F");
        if (i < 10u) uart2_putc('0');
        print_s32_local((int32_t)i);
        uart2_print(" A=0x");
        print_hex16_local(rx_buffer[base]);
        uart2_print(" (");
        print_s32_local((int16_t)rx_buffer[base]);
        uart2_print(") B=0x");
        print_hex16_local(rx_buffer[base + 1u]);
        uart2_print(" (");
        print_s32_local((int16_t)rx_buffer[base + 1u]);
        uart2_print(")\r\n");
    }
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
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    I2S_Cmd(SPI2, DISABLE);
    DMA_Cmd(DMA1_Channel4, DISABLE);
}
