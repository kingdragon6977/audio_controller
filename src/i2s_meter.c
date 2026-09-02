#include <stdint.h>
#include "i2s_meter.h"
#include "i2s_rx.h"
#include "uart.h"

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

static void print_s32(int32_t value)
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

static void print_u32(uint32_t value)
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

/*
 * Return peak level in whole dBFS without floating point.
 * Each loop applies one -1 dB amplitude step using
 * 10^(-1/20) ~= 0.89125094, represented as Q16 58409/65536.
 */
static int peak_dbfs(int32_t peak)
{
    uint32_t threshold = 32767u;
    unsigned int db;

    if (peak <= 0)
        return -99;

    if ((uint32_t)peak >= 32767u)
        return 0;

    for (db = 1u; db <= 96u; ++db)
    {
        threshold = (uint32_t)(((uint64_t)threshold * 58409u + 32768u) >> 16);
        if (threshold == 0u || (uint32_t)peak >= threshold)
            return -(int)db;
    }

    return -99;
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

static void accumulate_block(meter_channel_t *left,
                             meter_channel_t *right)
{
    const uint16_t *samples = i2s_rx_buffer();
    i2s_rx_debug_t debug;
    unsigned int i;

    i2s_rx_get_debug(&debug);

    /* Skip the first captured pair exactly like the forensic report. */
    for (i = 2u; i + 1u < I2S_RX_SAMPLES; i += 2u)
    {
        int32_t even = (int16_t)samples[i];
        int32_t odd = (int16_t)samples[i + 1u];

        if (debug.ws_level_at_enable)
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

static void print_channel(const char *name, const meter_channel_t *channel)
{
    int32_t mean = 0;
    uint64_t mean_square = 0u;
    uint64_t dc_square = 0u;
    uint32_t rms = 0u;
    int dbfs = -99;

    if (channel->samples != 0u)
    {
        mean = (int32_t)(channel->sum / (int64_t)channel->samples);
        mean_square = channel->sumsq / channel->samples;
        dc_square = (uint64_t)((int64_t)mean * (int64_t)mean);

        if (mean_square > dc_square)
            rms = isqrt_u64(mean_square - dc_square);

        dbfs = peak_dbfs(channel->peak);
    }

    uart2_print("  ");
    uart2_print(name);
    uart2_print(" MIN/MAX  = ");
    print_s32(channel->min);
    uart2_print(" / ");
    print_s32(channel->max);

    uart2_print("\r\n  ");
    uart2_print(name);
    uart2_print(" MEAN     = ");
    print_s32(mean);

    uart2_print("\r\n  ");
    uart2_print(name);
    uart2_print(" RMS(AC)  = ");
    print_u32(rms);

    uart2_print("\r\n  ");
    uart2_print(name);
    uart2_print(" PEAK     = ");
    print_s32(channel->peak);
    uart2_print(" (~");
    print_s32(dbfs);
    uart2_print(" dBFS)");

    uart2_print("\r\n  ");
    uart2_print(name);
    uart2_print(" CLIPS    = ");
    print_u32(channel->clips);
    uart2_print("\r\n");
}

int i2s_meter_run(unsigned int seconds)
{
    meter_channel_t left;
    meter_channel_t right;
    uint32_t target_blocks;
    uint32_t completed = 0u;
    uint32_t block;

    if (seconds < 1u || seconds > 10u)
        return 0;

    /* 127 analyzed stereo frames per 256-slot DMA block. */
    target_blocks = ((uint32_t)seconds * 48000u + 126u) / 127u;

    meter_init(&left);
    meter_init(&right);

    uart2_print("Starting long frame-aligned I2S audio meter...\r\n");

    for (block = 0u; block < target_blocks; ++block)
    {
        uint32_t timeout = 3000000u;

        if (!i2s_rx_start_capture())
            break;

        while (!i2s_rx_capture_complete() && timeout != 0u)
        {
            timeout--;
            __asm__("nop");
        }

        if (timeout == 0u || i2s_rx_error_flags() || !i2s_rx_capture_complete())
        {
            i2s_rx_stop();
            break;
        }

        accumulate_block(&left, &right);
        completed++;
        i2s_rx_stop();
    }

    uart2_print("\r\nI2S LONG AUDIO METER\r\n");
    uart2_print("  Requested     = ");
    print_u32(seconds);
    uart2_print(" s\r\n  Target blocks = ");
    print_u32(target_blocks);
    uart2_print("\r\n  Completed     = ");
    print_u32(completed);
    uart2_print(" blocks\r\n  Frames/ch     = ");
    print_u32(completed * 127u);
    uart2_print("\r\n  Channel map follows WS at every DMA handoff\r\n");

    if (completed == 0u)
    {
        uart2_print("  RESULT        = FAIL (no complete blocks)\r\n");
        return 0;
    }

    print_channel("LEFT ", &left);
    print_channel("RIGHT", &right);

    if (completed != target_blocks)
    {
        uart2_print("  RESULT        = PARTIAL (capture stopped early)\r\n");
        return 0;
    }

    uart2_print("  RESULT        = PASS\r\n");
    return 1;
}
