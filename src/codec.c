#include "stm32f10x.h"
#include "codec.h"
#include "i2c.h"
#include "uart.h"

#define CODEC_REG_MADC        0x13u
#define CODEC_REG_AOSR        0x14u
#define CODEC_REG_IFACE       0x1Bu
#define CODEC_REG_IFACE2      0x1Du
#define CODEC_REG_BCLK_DIV    0x1Eu

/* TLV320ADC3101 RESET is active low on STM32 PB14. */
#define CODEC_RESET_PORT      GPIOB
#define CODEC_RESET_PIN       GPIO_Pin_14

static const uint8_t av6301_profile[][2] = {
    { CODEC_REG_MADC,     0x84u },
    { CODEC_REG_IFACE,    0x0Cu },
    { CODEC_REG_AOSR,     0x80u },
    { CODEC_REG_IFACE2,   0x06u },
    { CODEC_REG_BCLK_DIV, 0x88u }
};

static void codec_delay(uint32_t count)
{
    while (count--)
        __asm__("nop");
}

/*
 * TLV320ADC3101 hardware reset.
 * TI requires RESET low for at least 10 ns after the codec supplies are
 * valid. We use millisecond-scale margins for bring-up reliability.
 */
void codec_reset(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    gpio.GPIO_Pin   = CODEC_RESET_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(CODEC_RESET_PORT, &gpio);

    /* Allow already-powered codec supplies to settle. */
    GPIO_SetBits(CODEC_RESET_PORT, CODEC_RESET_PIN);
    codec_delay(720000u);

    /* Active-low reset pulse. */
    GPIO_ResetBits(CODEC_RESET_PORT, CODEC_RESET_PIN);
    codec_delay(72000u);

    /* Release reset and allow codec startup. */
    GPIO_SetBits(CODEC_RESET_PORT, CODEC_RESET_PIN);
    codec_delay(720000u);
}

int codec_apply_av6301_profile(void)
{
    unsigned int i;

    if (!i2c1_write(TLV320ADC3101_ADDR, 0x00u, 0x00u))
        return 0;

    for (i = 0; i < sizeof(av6301_profile) / sizeof(av6301_profile[0]); ++i)
    {
        if (!i2c1_write(TLV320ADC3101_ADDR,
                        av6301_profile[i][0],
                        av6301_profile[i][1]))
            return 0;
    }

    return 1;
}

static void print_hex8(uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    uart2_putc(hex[(value >> 4) & 0x0Fu]);
    uart2_putc(hex[value & 0x0Fu]);
}

void codec_dump_profile(void)
{
    static const uint8_t regs[] = {
        0x00u, CODEC_REG_MADC, CODEC_REG_IFACE, CODEC_REG_AOSR,
        CODEC_REG_IFACE2, CODEC_REG_BCLK_DIV, 0x12u, 0x51u, 0x52u
    };
    unsigned int i;
    uint8_t value;

    uart2_print("\r\nTLV320ADC3101 Page-0 register dump\r\n");
    uart2_print("Expected captured AV6301 values:\r\n");
    uart2_print("  13=84  1B=0C  14=80  1D=06  1E=88\r\n");

    if (!i2c1_write(TLV320ADC3101_ADDR, 0x00u, 0x00u))
    {
        uart2_print("  ERROR: cannot select Page 0\r\n");
        return;
    }

    for (i = 0; i < sizeof(regs) / sizeof(regs[0]); ++i)
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
