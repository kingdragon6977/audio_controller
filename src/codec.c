#include "stm32f10x.h"
#include "codec.h"
#include "i2c.h"
#include "uart.h"
#include "audio_clock.h"
#include <stdio.h>

/* TLV320ADC3101 Page-0 clock / serial-interface registers. */
#define CODEC_REG_CLKMUX       0x04u
#define CODEC_REG_PLLPR        0x05u
#define CODEC_REG_PLLJ         0x06u
#define CODEC_REG_PLLD_MSB     0x07u
#define CODEC_REG_PLLD_LSB     0x08u
#define CODEC_REG_NADC         0x12u
#define CODEC_REG_MADC         0x13u
#define CODEC_REG_AOSR         0x14u
#define CODEC_REG_IADC         0x15u
#define CODEC_REG_IFACE        0x1Bu
#define CODEC_REG_IFACE2       0x1Du
#define CODEC_REG_BCLK_DIV     0x1Eu
#define CODEC_REG_ADC_FLAG     0x24u
#define CODEC_REG_DOUT         0x35u
#define CODEC_REG_ADC_PRB      0x3Du
#define CODEC_REG_ADC_POWER    0x51u
#define CODEC_REG_ADC_MUTE     0x52u

/* Page-1 analog input routing/gain registers. */
#define CODEC_REG_PAGE         0x00u
#define CODEC_REG_MICBIAS      0x33u
#define CODEC_REG_IN1L_ROUTE   0x34u
#define CODEC_REG_IN1R_ROUTE   0x37u
#define CODEC_REG_RIGHT_X_ROUTE 0x39u
#define CODEC_REG_LEFT_PGA     0x3Bu
#define CODEC_REG_RIGHT_PGA    0x3Cu
#define CODEC_REG_PGA_FLAGS    0x3Eu

/* TLV320ADC3101 RESET is active low on STM32 PB14. */
#define CODEC_RESET_PORT      GPIOB
#define CODEC_RESET_PIN       GPIO_Pin_14

/*
 * PA8 12-MHz MCLK / 48-kHz / 16-bit I2S master configuration.
 *
 * PLL: 12 MHz * 8.192 = 98.304 MHz
 *   P=1, R=1, J=8, D=1920
 * ADC: 98.304 MHz / 8 / 2 / 128 = 48 kHz
 * BCLK source: ADC_CLK = 98.304 MHz / 8 = 12.288 MHz
 * BCLK: 12.288 MHz / 8 = 1.536 MHz = 32 * 48 kHz
 *
 * Register 0x1B = 0x0C selects I2S, 16-bit and ADC master mode.
 * Register 0x1D = 0x02 selects ADC_CLK as the BCLK divider source.
 * Register 0x1E = 0x88 selects BCLK divider N=8.
 * Register 0x35 = 0x12 routes DOUT to the primary codec interface.
 *
 * Normal analog routing uses physical IN1L(P) on the left PGA only.
 * The right analog input paths remain disconnected while both ADC engines
 * stay powered so the already-proven stereo I2S framing remains unchanged.
 */
static const uint8_t audio_profile[][2] = {
    { CODEC_REG_PAGE,       0x01u },

    /* Physical IN1L(P), pin 8 -> left PGA, single-ended, 0 dB. */
    { CODEC_REG_IN1L_ROUTE, 0xFCu },
    { CODEC_REG_LEFT_PGA,   0x00u },

    /* Unused right analog channel: disconnect both normal and alternate
     * routes. Keep right PGA at 0 dB while its input is disconnected. */
    { CODEC_REG_IN1R_ROUTE,    0xFFu },
    { CODEC_REG_RIGHT_X_ROUTE, 0x3Fu },
    { CODEC_REG_RIGHT_PGA,     0x00u },

    /* Return to page 0 for clock / digital interface configuration. */
    { CODEC_REG_PAGE,       0x00u },
    { CODEC_REG_ADC_MUTE,   0x88u }, /* Mute ADCs while clocks/power start */
    { CODEC_REG_ADC_POWER,  0x00u }, /* ADCs off during clock programming */

    { CODEC_REG_CLKMUX,   0x03u }, /* MCLK -> PLL, PLL -> CODEC_CLK */
    { CODEC_REG_PLLPR,    0x91u }, /* PLL on, P=1, R=1 */
    { CODEC_REG_PLLJ,     0x08u }, /* J=8 */
    { CODEC_REG_PLLD_MSB, 0x07u }, /* D=1920 = 0x0780 */
    { CODEC_REG_PLLD_LSB, 0x80u },

    { CODEC_REG_NADC,     0x88u }, /* NADC=8, enabled */
    { CODEC_REG_MADC,     0x82u }, /* MADC=2, enabled */
    { CODEC_REG_AOSR,     0x80u }, /* AOSR=128 */
    { CODEC_REG_IADC,     0x80u }, /* IADC=128 */

    { CODEC_REG_IFACE,    0x0Cu }, /* I2S, 16-bit, ADC master */
    { CODEC_REG_IFACE2,   0x02u }, /* BCLK divider input = ADC_CLK */
    { CODEC_REG_BCLK_DIV, 0x88u }, /* BCLK divider N=8 -> 1.536 MHz */
    { CODEC_REG_DOUT,     0x12u }, /* primary DOUT, bus keeper disabled */

    { CODEC_REG_ADC_POWER, 0xC2u }, /* power up both ADC channels */
    { CODEC_REG_ADC_MUTE,  0x00u }  /* unmute, 0 dB digital gain */
};

static void codec_delay(uint32_t count)
{
    while (count--)
        __asm__("nop");
}

static int profile_verified;

int codec_profile_ready(void)
{
    return profile_verified && audio_clock_ready();
}

static int verify_clock_profile(void)
{
    static const uint8_t expected[][2] = {
        {0x04,0x03}, {0x05,0x91}, {0x06,0x08}, {0x07,0x07}, {0x08,0x80},
        {0x12,0x88}, {0x13,0x82}, {0x14,0x80}, {0x15,0x80},
        {0x1b,0x0c}, {0x1d,0x02}, {0x1e,0x88}, {0x35,0x12}, {0x51,0xc2}, {0x52,0x00}
    };
    unsigned int i;
    uint8_t value;
    char buf[110];
    if (!i2c1_write(TLV320ADC3101_ADDR, CODEC_REG_PAGE, 0u)) return 0;
    for (i=0; i<sizeof(expected)/sizeof(expected[0]); ++i) {
        if (!i2c1_read(TLV320ADC3101_ADDR, expected[i][0], &value)) {
            uart2_print("CODEC CLOCK READBACK: FAIL - I2C read failed.\r\n");
            return 0;
        }
        if (value != expected[i][1]) {
            sprintf(buf, "CODEC CLOCK READBACK: FAIL reg=0x%02X actual=0x%02X expected=0x%02X\r\n",
                    expected[i][0], value, expected[i][1]);
            uart2_print(buf);
            return 0;
        }
    }
    uart2_print("CODEC CLOCK READBACK: PASS - calculated PLL 98.304 MHz, ADC 48 kHz, BCLK 1.536 MHz.\r\n");
    return 1;
}

void codec_reset(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    gpio.GPIO_Pin   = CODEC_RESET_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(CODEC_RESET_PORT, &gpio);

    profile_verified = 0;
    audio_clock_invalidate_rate();
    if (!audio_clock_ready()) {
        GPIO_ResetBits(CODEC_RESET_PORT, CODEC_RESET_PIN);
        uart2_print("CODEC RESET: HELD - reference clock register check failed.\r\n");
        return;
    }
    GPIO_SetBits(CODEC_RESET_PORT, CODEC_RESET_PIN);
    codec_delay(720000u);

    GPIO_ResetBits(CODEC_RESET_PORT, CODEC_RESET_PIN);
    codec_delay(72000u);

    GPIO_SetBits(CODEC_RESET_PORT, CODEC_RESET_PIN);
    codec_delay(720000u);
}

int codec_apply_audio_profile(void)
{
    unsigned int i;
    profile_verified = 0;
    audio_clock_invalidate_rate();
    if (!audio_clock_ready()) goto fail;

    for (i = 0; i < sizeof(audio_profile) / sizeof(audio_profile[0]); ++i)
    {
        if (!i2c1_write(TLV320ADC3101_ADDR,
                        audio_profile[i][0],
                        audio_profile[i][1]))
            goto fail;
        if (audio_profile[i][0] == CODEC_REG_PLLD_LSB)
            codec_delay(720000u); /* Settling after the adjacent D-register pair. */
    }

    profile_verified = verify_clock_profile();
    if (profile_verified) return 1;
fail:
    profile_verified = 0;
    audio_clock_invalidate_rate();
    GPIO_ResetBits(CODEC_RESET_PORT, CODEC_RESET_PIN);
    uart2_print("CODEC PROFILE: FAIL - hardware reset held; capture blocked.\r\n");
    return 0;
}

static void print_hex8(uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    uart2_putc(hex[(value >> 4) & 0x0Fu]);
    uart2_putc(hex[value & 0x0Fu]);
}

static void codec_dump_registers(const char *title,
                                  const uint8_t *regs,
                                  unsigned int count)
{
    unsigned int i;
    uint8_t value;

    uart2_print(title);

    for (i = 0; i < count; ++i)
    {
        if (i2c1_read(TLV320ADC3101_ADDR, regs[i], &value))
        {
            uart2_print("  0x");
            print_hex8(regs[i]);
            uart2_print(" = 0x");
            print_hex8(value);
            uart2_print("\r\n");
        }
        else
        {
            uart2_print("  0x");
            print_hex8(regs[i]);
            uart2_print(" = READ ERROR\r\n");
        }
    }
}

void codec_dump_profile(void)
{
    static const uint8_t page0_regs[] = {
        0x00u,
        CODEC_REG_CLKMUX,
        CODEC_REG_PLLPR,
        CODEC_REG_PLLJ,
        CODEC_REG_PLLD_MSB,
        CODEC_REG_PLLD_LSB,
        CODEC_REG_NADC,
        CODEC_REG_MADC,
        CODEC_REG_AOSR,
        CODEC_REG_IADC,
        CODEC_REG_IFACE,
        0x1Cu,
        CODEC_REG_IFACE2,
        CODEC_REG_BCLK_DIV,
        0x1Fu,
        0x20u,
        0x21u,
        CODEC_REG_ADC_FLAG,
        CODEC_REG_ADC_PRB,
        CODEC_REG_DOUT,
        CODEC_REG_ADC_POWER,
        CODEC_REG_ADC_MUTE
    };
    static const uint8_t page1_regs[] = {
        CODEC_REG_MICBIAS,
        CODEC_REG_IN1L_ROUTE,
        CODEC_REG_IN1R_ROUTE,
        CODEC_REG_RIGHT_X_ROUTE,
        CODEC_REG_LEFT_PGA,
        CODEC_REG_RIGHT_PGA,
        CODEC_REG_PGA_FLAGS
    };

    if (!i2c1_write(TLV320ADC3101_ADDR, CODEC_REG_PAGE, 0x00u))
    {
        uart2_print("\r\nTLV320ADC3101 DIAGNOSTIC DUMP: ERROR selecting Page 0\r\n");
        return;
    }

    uart2_print("\r\nTLV320ADC3101 Page-0 clock/interface/ADC dump\r\n");
    codec_dump_registers("", page0_regs,
                         sizeof(page0_regs) / sizeof(page0_regs[0]));

    if (!i2c1_write(TLV320ADC3101_ADDR, CODEC_REG_PAGE, 0x01u))
    {
        uart2_print("TLV320ADC3101 DIAGNOSTIC DUMP: ERROR selecting Page 1\r\n");
        (void)i2c1_write(TLV320ADC3101_ADDR, CODEC_REG_PAGE, 0x00u);
        return;
    }

    uart2_print("\r\nTLV320ADC3101 Page-1 analog-input dump\r\n");
    codec_dump_registers("", page1_regs,
                         sizeof(page1_regs) / sizeof(page1_regs[0]));

    (void)i2c1_write(TLV320ADC3101_ADDR, CODEC_REG_PAGE, 0x00u);
}
