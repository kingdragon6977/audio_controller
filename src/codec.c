#include "stm32f10x.h"
#include "codec.h"
#include "i2c.h"
#include "uart.h"

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
#define CODEC_REG_LEFT_PGA     0x3Bu
#define CODEC_REG_RIGHT_PGA    0x3Cu

/* TLV320ADC3101 RESET is active low on STM32 PB14. */
#define CODEC_RESET_PORT      GPIOB
#define CODEC_RESET_PIN       GPIO_Pin_14

/*
 * Known-good 16-MHz MCLK / 48-kHz / 16-bit I2S master configuration.
 *
 * PLL: 16 MHz * 6.144 = 98.304 MHz
 *   P=1, R=1, J=6, D=1440
 * ADC: 98.304 MHz / 8 / 2 / 128 = 48 kHz
 * BCLK: ADC_CLK / 8 = 12.288 MHz / 8 = 1.536 MHz
 *
 * IMPORTANT: the recovered AV6301 traffic uses:
 *   P0:0x1D = 0x06 -> BDIV_CLKIN = ADC_CLK, BCLK/WCLK stay active
 *                    when the codec is otherwise powered down
 *   P0:0x1E = 0x88 -> BCLK divider enabled, N = 8
 *
 * Register 0x1B = 0x0C selects I2S, 16-bit and ADC master mode.
 * Register 0x35 = 0x12 selects primary DOUT and disables its bus keeper.
 *
 * The original AV6301 traffic was captured with the project's logic analyzer.
 * This profile preserves the recovered clock/interface settings and adds the
 * missing explicit IN1L(P) pin-8 analog input path for the isolated TLV320.
 */
static const uint8_t av6301_profile[][2] = {
    { CODEC_REG_PAGE, 0x00u },
    { CODEC_REG_PAGE,       0x01u },
    { CODEC_REG_IN1L_ROUTE, 0xFCu },
    { CODEC_REG_LEFT_PGA,   0x00u },
    { CODEC_REG_PAGE,       0x00u },
    { CODEC_REG_ADC_MUTE,   0x88u },
    { CODEC_REG_ADC_POWER,  0x00u },
    { CODEC_REG_CLKMUX,     0x03u },
    { CODEC_REG_PLLPR,      0x91u },
    { CODEC_REG_PLLJ,       0x06u },
    { CODEC_REG_PLLD_MSB,   0x05u },
    { CODEC_REG_PLLD_LSB,   0xA0u },
    { CODEC_REG_NADC,       0x88u },
    { CODEC_REG_MADC,       0x82u },
    { CODEC_REG_AOSR,       0x80u },
    { CODEC_REG_IADC,       0x80u },
    { CODEC_REG_IFACE,      0x0Cu },
    { CODEC_REG_IFACE2,     0x06u },
    { CODEC_REG_BCLK_DIV,   0x88u },
    { CODEC_REG_DOUT,       0x12u },
    { CODEC_REG_ADC_POWER,  0xC2u },
    { CODEC_REG_ADC_MUTE,   0x00u }
};

static void codec_delay(uint32_t count)
{
    while (count--)
        __asm__("nop");
}

void codec_reset(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    gpio.GPIO_Pin   = CODEC_RESET_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(CODEC_RESET_PORT, &gpio);

    GPIO_SetBits(CODEC_RESET_PORT, CODEC_RESET_PIN);
    codec_delay(720000u);
    GPIO_ResetBits(CODEC_RESET_PORT, CODEC_RESET_PIN);
    codec_delay(72000u);
    GPIO_SetBits(CODEC_RESET_PORT, CODEC_RESET_PIN);
    codec_delay(720000u);
}

static void print_hex8(uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    uart2_putc(hex[(value >> 4) & 0x0Fu]);
    uart2_putc(hex[value & 0x0Fu]);
}

int codec_apply_av6301_profile(void)
{
    unsigned int i;
    uint8_t actual;

    for (i = 0; i < sizeof(av6301_profile) / sizeof(av6301_profile[0]); ++i)
    {
        uint8_t reg = av6301_profile[i][0];
        uint8_t expected = av6301_profile[i][1];

        if (!i2c1_write(TLV320ADC3101_ADDR, reg, expected))
        {
            uart2_print("Codec profile I2C WRITE FAIL at reg 0x");
            print_hex8(reg);
            uart2_print(" expected 0x");
            print_hex8(expected);
            uart2_print("\r\n");
            return 0;
        }

        /* Verify every profile write immediately. This distinguishes an I2C
         * transaction that completed electrically from a codec register that
         * actually accepted the value. Page 0/1 writes are included so the
         * first failing register is unambiguous. */
        if (!i2c1_read(TLV320ADC3101_ADDR, reg, &actual))
        {
            uart2_print("Codec profile READBACK FAIL at reg 0x");
            print_hex8(reg);
            uart2_print(" after writing 0x");
            print_hex8(expected);
            uart2_print("\r\n");
            return 0;
        }

        if (actual != expected)
        {
            uart2_print("Codec profile VERIFY FAIL at reg 0x");
            print_hex8(reg);
            uart2_print(" wrote 0x");
            print_hex8(expected);
            uart2_print(" read 0x");
            print_hex8(actual);
            uart2_print("\r\n");
            return 0;
        }
    }

    return 1;
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
        0x00u, CODEC_REG_CLKMUX, CODEC_REG_PLLPR, CODEC_REG_PLLJ,
        CODEC_REG_PLLD_MSB, CODEC_REG_PLLD_LSB, CODEC_REG_NADC,
        CODEC_REG_MADC, CODEC_REG_AOSR, CODEC_REG_IADC, CODEC_REG_IFACE,
        0x1Cu, CODEC_REG_IFACE2, CODEC_REG_BCLK_DIV, 0x1Fu, 0x20u,
        0x21u, CODEC_REG_ADC_FLAG, CODEC_REG_ADC_PRB, CODEC_REG_DOUT,
        CODEC_REG_ADC_POWER, CODEC_REG_ADC_MUTE
    };
    static const uint8_t page1_regs[] = {
        CODEC_REG_MICBIAS, CODEC_REG_IN1L_ROUTE, CODEC_REG_IN1R_ROUTE,
        CODEC_REG_LEFT_PGA, CODEC_REG_RIGHT_PGA
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
