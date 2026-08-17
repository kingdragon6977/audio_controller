#include "stm32f10x.h"
#include "codec.h"
#include "i2c.h"
#include "uart.h"

/*
 * TLV320ADC3101 Page 0 register definitions used by the AV6301 capture.
 *
 * The STM32 Logic Analyzer captured this exact sequence on the AV6301 I2C
 * bus, all to 7-bit address 0x18:
 *
 *   0x13 <- 0x84
 *   0x1B <- 0x0C
 *   0x14 <- 0x80
 *   0x1D <- 0x06
 *   0x1E <- 0x88
 *
 * These values decode as:
 *   MADC = 4, AOSR = 128
 *   I2S, 16-bit, BCLK/WCLK outputs, DOUT not 3-stated
 *   BDIV clock input = ADC_CLK, BCLK divider enabled, N = 8
 *
 * Do not silently add other codec writes here. The captured sequence is the
 * known-good reference from the original AV6301 and should be reproduced
 * before we infer any additional ADC power/input settings.
 */
#define CODEC_REG_MADC        0x13u
#define CODEC_REG_AOSR        0x14u
#define CODEC_REG_IFACE       0x1Bu
#define CODEC_REG_IFACE2      0x1Du
#define CODEC_REG_BCLK_DIV    0x1Eu

static const uint8_t av6301_profile[][2] = {
    { CODEC_REG_MADC,     0x84u },
    { CODEC_REG_IFACE,    0x0Cu },
    { CODEC_REG_AOSR,     0x80u },
    { CODEC_REG_IFACE2,   0x06u },
    { CODEC_REG_BCLK_DIV, 0x88u }
};

static void print_hex8(uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    uart2_putc(hex[(value >> 4) & 0x0Fu]);
    uart2_putc(hex[value & 0x0Fu]);
}

int codec_apply_av6301_profile(void)
{
    unsigned int i;

    /* All captured registers are on Page 0. */
    if (!i2c2_write(TLV320ADC3101_ADDR, 0x00u, 0x00u))
        return 0;

    for (i = 0; i < sizeof(av6301_profile) / sizeof(av6301_profile[0]); ++i)
    {
        if (!i2c2_write(TLV320ADC3101_ADDR,
                        av6301_profile[i][0],
                        av6301_profile[i][1]))
            return 0;
    }

    return 1;
}

void codec_dump_profile(void)
{
    static const uint8_t regs[] = {
        0x00u, /* page */
        CODEC_REG_MADC,
        CODEC_REG_IFACE,
        CODEC_REG_AOSR,
        CODEC_REG_IFACE2,
        CODEC_REG_BCLK_DIV,
        0x12u, /* NADC - important for determining actual sample rate */
        0x51u, /* ADC digital power */
        0x52u  /* ADC digital mute/gain */
    };
    unsigned int i;
    uint8_t value;

    uart2_print("\r\nTLV320ADC3101 Page-0 register dump\r\n");
    uart2_print("Expected captured AV6301 values:\r\n");
    uart2_print("  13=84  1B=0C  14=80  1D=06  1E=88\r\n");

    if (!i2c2_write(TLV320ADC3101_ADDR, 0x00u, 0x00u))
    {
        uart2_print("  ERROR: cannot select Page 0\r\n");
        return;
    }

    for (i = 0; i < sizeof(regs) / sizeof(regs[0]); ++i)
    {
        if (i2c2_read(TLV320ADC3101_ADDR, regs[i], &value))
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
