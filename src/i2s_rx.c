#include "stm32f10x.h"
#include "i2s_rx.h"
#include "diagnostics.h"
#include "uart.h"

static uint16_t rx_buffer[I2S_RX_SAMPLES];
static uint32_t dma_error_flags;
static i2s_rx_debug_t debug_state;
static uint8_t capture_debug_latched;

#define I2S_METER_BLOCKS 375u

typedef struct
{
    int32_t min;
    int32_t max;
    int32_t peak;
    int64_t sum;
    uint64_t sumsq;
    uint32_t samples;
    uint32_t clips;
} meter_channel_t;

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

static void print_u32_local(uint32_t value)
{
    char buf[12];
    unsigned int n = 0u;

    do
    {
        buf[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);

    while (n != 0u)
        uart2_putc(buf[--n]);
}

static uint32_t isqrt_u64(uint64_t value)
{
    uint64_t bit = (uint64_t)1u << 62;
    uint64_t result = 0u;

    while (bit > value)
        bit >>= 2;

    while (bit != 0u)
    {
        if (value >= result + bit)
        {
            value -= result + bit;
            result = (result >> 1) + bit;
        }
        else
        {
            result >>= 1;
        }
        bit >>= 2;
    }

    return (uint32_t)result;
}

static int peak_dbfs_approx(int32_t peak)
{
    static const uint16_t threshold[] = {
        32767u, 29204u, 26028u, 23198u, 20675u, 18426u, 16422u,
        14636u, 13045u, 11627u, 10362u, 9235u, 8231u, 7336u,
        6539u, 5828u, 5194u, 4630u, 4127u, 3678u, 3278u
    };
    unsigned int i;

    if (peak <= 0)
        return -99;

    for (i = 0u; i < sizeof(threshold) / sizeof(threshold[0]); ++i)
    {
        if ((uint32_t)peak >= threshold[i])
            return -(int)i;
    }

    return -21;
}

static void meter_init(meter_channel_t *channel)
{
    channel->min = 32767;
    channel->max = -32768;
    channel->peak = 0;
    channel->sum = 0;
    channel->sumsq = 0u;
    channel->samples = 0u;
    channel->clips = 0u;
}

static void meter_add(meter_channel_t *channel, int32_t sample)
{
    int32_t magnitude = (sample < 0) ? -sample : sample;

    if (sample < channel->min) channel->min = sample;
    if (sample > channel->max) channel->max = sample;
    if (magnitude > channel->peak) channel->peak = magnitude;
    channel->sum += sample;
    channel->sumsq += (uint64_t)((int64_t)sample * (int64_t)sample);
    channel->samples++;
    if (sample == 32767 || sample == -32768)
        channel->clips++;
}

static void meter_print_channel(const char *name, const meter_channel_t *channel)
{
    int32_t mean = 0;
    uint64_t mean_square = 0u;
    uint64_t dc_square = 0u;
    uint32_t rms = 0u;
    int dbfs;

    if (channel->samples != 0u)
    {
        mean = (int32_t)(channel->sum / (int64_t)channel->samples);
        mean_square = channel->sumsq / channel->samples;
        dc_square = (uint64_t)((int64_t)mean * (int64_t)mean);
        if (mean_square > dc_square)
            rms = isqrt_u64(mean_square - dc_square);
    }

    dbfs = peak_dbfs_approx(channel->peak);

    uart2_print("  ");
    uart2_print(name);
    uart2_print(" MIN/MAX  = ");
    print_s32_local(channel->min);
    uart2_print(" / ");
    print_s32_local(channel->max);
    uart2_print("\r\n  ");
    uart2_print(name);
    uart2_print(" MEAN     = ");
    print_s32_local(mean);
    uart2_print("\r\n  ");
    uart2_print(name);
    uart2_print(" RMS(AC)  = ");
    print_u32_local(rms);
    uart2_print("\r\n  ");
    uart2_print(name);
    uart2_print(" PEAK     = ");
    print_s32_local(channel->peak);
    uart2_print(" (~");
    print_s32_local(dbfs);
    uart2_print(" dBFS)\r\n  ");
    uart2_print(name);
    uart2_print(" CLIPS    = ");
    print_u32_local(channel->clips);
    uart2_print("\r\n");
}

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

            if (timeout == 0u)
            {
                I2S_Cmd(SPI2, DISABLE);
                return 0;
            }
            timeout--;
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
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
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

    if (!sync_receiver_before_dma())
        return 0;

    debug_state.sr_after_enable = SPI2->SR;
    debug_state.ws_sync_ok = 1u;
    debug_state.ws_level_at_enable =
        (GPIOB->IDR & GPIO_Pin_12) ? 1u : 0u;

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

static void meter_accumulate_current(meter_channel_t *left,
                                     meter_channel_t *right)
{
    unsigned int i;
    int ws_high = debug_state.ws_level_at_enable ? 1 : 0;

    for (i = 2u; i + 1u < I2S_RX_SAMPLES; i += 2u)
    {
        int32_t even = (int16_t)rx_buffer[i];
        int32_t odd = (int16_t)rx_buffer[i + 1u];

        if (ws_high)
        {
            meter_add(right, even);
            meter_add(left, odd);
        }
        else
        {
            meter_add(left, even);
            meter_add(right, odd);
        }
    }
}

static void run_one_second_meter(void)
{
    meter_channel_t left;
    meter_channel_t right;
    unsigned int block;
    unsigned int completed = 1u;

    meter_init(&left);
    meter_init(&right);
    meter_accumulate_current(&left, &right);

    for (block = 1u; block < I2S_METER_BLOCKS; ++block)
    {
        uint32_t timeout = 3000000u;

        i2s_rx_stop();
        if (!i2s_rx_start_capture())
            break;

        while (!i2s_rx_capture_complete() && timeout != 0u)
        {
            timeout--;
            __asm__("nop");
        }

        if (timeout == 0u || i2s_rx_error_flags() || !i2s_rx_capture_complete())
            break;

        meter_accumulate_current(&left, &right);
        completed++;
    }

    uart2_print("\r\nI2S ~1 SECOND AUDIO METER\r\n");
    uart2_print("  Target blocks = 375 (about 48,000 stereo frames)\r\n");
    uart2_print("  Completed     = ");
    print_u32_local(completed);
    uart2_print(" blocks\r\n");
    uart2_print("  Channel map follows WS level captured at each DMA handoff\r\n");
    meter_print_channel("LEFT ", &left);
    meter_print_channel("RIGHT", &right);
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
    uart2_print(debug_state.ws_sync_ok ? "PASS (CHSIDE-aligned DMA handoff)\r\n"
                                       : "FAIL\r\n");
    uart2_print("  WS at DMA   = ");
    uart2_print(debug_state.ws_level_at_enable ? "HIGH\r\n" : "LOW\r\n");
    if (debug_state.ws_level_at_enable)
        uart2_print("  Slot mapping = even RIGHT, odd LEFT\r\n");
    else
        uart2_print("  Slot mapping = even LEFT, odd RIGHT\r\n");
    uart2_print("  Statistics exclude first captured stereo pair\r\n");

    uart2_print(debug_state.ws_level_at_enable ?
                "  RIGHT MIN/MAX = " : "  LEFT MIN/MAX  = ");
    print_s32_local(min_a);
    uart2_print(" / ");
    print_s32_local(max_a);
    uart2_print(debug_state.ws_level_at_enable ?
                "\r\n  RIGHT MEAN    = " : "\r\n  LEFT MEAN     = ");
    print_s32_local(pair_count ? (int32_t)(sum_a / pair_count) : 0);
    uart2_print(debug_state.ws_level_at_enable ?
                "\r\n  RIGHT PEAK    = " : "\r\n  LEFT PEAK     = ");
    print_s32_local(peak_a);
    uart2_print(debug_state.ws_level_at_enable ?
                "\r\n  RIGHT FS/2    = " : "\r\n  LEFT FS/2     = ");
    print_s32_local(pair_count ? (int32_t)(alternating_a / pair_count) : 0);
    uart2_print("\r\n");

    uart2_print(debug_state.ws_level_at_enable ?
                "  LEFT MIN/MAX  = " : "  RIGHT MIN/MAX = ");
    print_s32_local(min_b);
    uart2_print(" / ");
    print_s32_local(max_b);
    uart2_print(debug_state.ws_level_at_enable ?
                "\r\n  LEFT MEAN     = " : "\r\n  RIGHT MEAN    = ");
    print_s32_local(pair_count ? (int32_t)(sum_b / pair_count) : 0);
    uart2_print(debug_state.ws_level_at_enable ?
                "\r\n  LEFT PEAK     = " : "\r\n  RIGHT PEAK    = ");
    print_s32_local(peak_b);
    uart2_print(debug_state.ws_level_at_enable ?
                "\r\n  LEFT FS/2     = " : "\r\n  RIGHT FS/2    = ");
    print_s32_local(pair_count ? (int32_t)(alternating_b / pair_count) : 0);
    uart2_print("\r\n");

    uart2_print("  FIRST 12 RAW FRAMES (even/odd raw slot order):\r\n");
    for (i = 0u; i < 12u; ++i)
    {
        unsigned int base = 2u * i;
        uart2_print("    F");
        if (i < 10u) uart2_putc('0');
        print_s32_local((int32_t)i);
        uart2_print(" E=0x");
        print_hex16_local(rx_buffer[base]);
        uart2_print(" (");
        print_s32_local((int16_t)rx_buffer[base]);
        uart2_print(") O=0x");
        print_hex16_local(rx_buffer[base + 1u]);
        uart2_print(" (");
        print_s32_local((int16_t)rx_buffer[base + 1u]);
        uart2_print(")\r\n");
    }

    run_one_second_meter();
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
    DMA_Cmd(DMA1_Channel4, DISABLE);
    I2S_Cmd(SPI2, DISABLE);
    clear_spi2_rx_state();
}
