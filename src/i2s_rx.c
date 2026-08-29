#include "stm32f10x.h"
#include "i2s_rx.h"
#include "diagnostics.h"
#include "uart.h"

static uint16_t rx_buffer[I2S_RX_SAMPLES];
static uint32_t dma_error_flags;
static i2s_rx_debug_t debug_state;
static uint8_t capture_debug_latched;
static uint8_t receiver_locked;

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

static void drain_spi2_rx(void)
{
    volatile uint16_t value;

    if ((SPI2->SR & SPI_SR_RXNE) != 0u)
    {
        value = SPI2->DR;
        (void)value;
    }

    /* Clear OVR, if present, using the STM32F1-required DR then SR sequence. */
    if ((SPI2->SR & SPI_SR_OVR) != 0u)
    {
        value = SPI2->DR;
        value = SPI2->SR;
        (void)value;
    }
}

static int wait_for_ws_falling_edge(void)
{
    uint32_t timeout;

    timeout = 400000u;
    while ((GPIOB->IDR & GPIO_Pin_12) == 0u)
    {
        if (timeout == 0u)
            return 0;
        timeout--;
    }

    timeout = 400000u;
    while ((GPIOB->IDR & GPIO_Pin_12) != 0u)
    {
        if (timeout == 0u)
            return 0;
        timeout--;
    }

    return 1;
}

static int lock_receiver_to_frame(void)
{
    I2S_InitTypeDef i2s;

    I2S_Cmd(SPI2, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);

    i2s.I2S_Mode = I2S_Mode_SlaveRx;
    i2s.I2S_Standard = I2S_Standard_Phillips;
    i2s.I2S_DataFormat = I2S_DataFormat_16b;
    i2s.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
    i2s.I2S_AudioFreq = I2S_AudioFreq_48k;
    i2s.I2S_CPOL = I2S_CPOL_Low;
    I2S_Init(SPI2, &i2s);

    /*
     * The TLV320 clocks run continuously. Enabling an STM32 I2S slave in the
     * middle of a slot can leave its internal 16-bit shifter permanently
     * offset until the peripheral is disabled again. Wait for a known WCLK
     * falling edge while SPI2 is disabled, then enable immediately at that
     * frame boundary. Once locked, do NOT disable SPI2 between captures.
     */
    if (!wait_for_ws_falling_edge())
        return 0;

    I2S_Cmd(SPI2, ENABLE);
    receiver_locked = 1u;
    return 1;
}

int i2s_rx_start_capture(void)
{
    DMA_InitTypeDef dma;
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

    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_GL4 | DMA1_FLAG_TC4 |
                  DMA1_FLAG_HT4 | DMA1_FLAG_TE4);

    if (!receiver_locked)
    {
        dummy_dr = SPI2->DR;
        dummy_sr = SPI2->SR;
        (void)dummy_dr;
        (void)dummy_sr;

        if (!lock_receiver_to_frame())
            return 0;
    }

    debug_state.sr_after_enable = SPI2->SR;
    debug_state.ws_sync_ok = 1u;
    debug_state.ws_level_at_enable =
        (GPIOB->IDR & GPIO_Pin_12) ? 1u : 0u;

    /* The receiver remains live between captures and may have RXNE/OVR set
     * while DMA is idle. Clearing those flags does not disturb the serial
     * shifter/frame lock. */
    drain_spi2_rx();

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

    DMA_ClearFlag(DMA1_FLAG_GL4 | DMA1_FLAG_TC4 |
                  DMA1_FLAG_HT4 | DMA1_FLAG_TE4);
    DMA_SetCurrDataCounter(DMA1_Channel4, I2S_RX_SAMPLES);
    DMA_Cmd(DMA1_Channel4, ENABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, ENABLE);

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
    int64_t alternating_a = 0;
    int64_t alternating_b = 0;

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

        if ((pair_count & 1u) == 0u)
        {
            alternating_a += a;
            alternating_b += b;
        }
        else
        {
            alternating_a -= a;
            alternating_b -= b;
        }

        pair_count++;
    }

    uart2_print("\r\nI2S FRAME-ALIGNED CHANNEL ANALYSIS\r\n");
    uart2_print("  WS sync     = ");
    uart2_print(debug_state.ws_sync_ok ? "PASS (receiver held live between captures)\r\n"
                                       : "FAIL\r\n");
    uart2_print("  WS at DMA   = ");
    uart2_print(debug_state.ws_level_at_enable ? "HIGH\r\n" : "LOW\r\n");
    uart2_print("  Slot phase   = even stream A, odd stream B\r\n");
    uart2_print("  Statistics exclude first captured A/B pair\r\n");

    uart2_print("  STREAM A MIN/MAX = ");
    print_s32_local(min_a);
    uart2_print(" / ");
    print_s32_local(max_a);
    uart2_print("\r\n  STREAM A MEAN    = ");
    print_s32_local(pair_count ? (int32_t)(sum_a / pair_count) : 0);
    uart2_print("\r\n  STREAM A PEAK    = ");
    print_s32_local(peak_a);
    uart2_print("\r\n  STREAM A FS/2    = ");
    print_s32_local(pair_count ? (int32_t)(alternating_a / pair_count) : 0);
    uart2_print("\r\n");

    uart2_print("  STREAM B MIN/MAX = ");
    print_s32_local(min_b);
    uart2_print(" / ");
    print_s32_local(max_b);
    uart2_print("\r\n  STREAM B MEAN    = ");
    print_s32_local(pair_count ? (int32_t)(sum_b / pair_count) : 0);
    uart2_print("\r\n  STREAM B PEAK    = ");
    print_s32_local(peak_b);
    uart2_print("\r\n  STREAM B FS/2    = ");
    print_s32_local(pair_count ? (int32_t)(alternating_b / pair_count) : 0);
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
    /* Stop only the capture transport. Keep SPI2/I2S enabled so the slave
     * receiver never loses BCLK/WCLK phase between CLI captures. */
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    DMA_Cmd(DMA1_Channel4, DISABLE);
    drain_spi2_rx();
}
