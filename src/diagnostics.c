#include "stm32f10x.h"
#include "diagnostics.h"
#include "uart.h"

#define DBGMCU_IDCODE_ADDR 0xE0042000u
#define UID_BASE_ADDR      0x1FFFF7E8u
#define FLASH_SIZE_ADDR    0x1FFFF7E0u

static void hex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    int shift;

    for (shift = 28; shift >= 0; shift -= 4)
        uart2_putc(hex[(value >> shift) & 0x0Fu]);
}

static void dec32(uint32_t value)
{
    char buf[11];
    unsigned int i = 0;

    if (value == 0u)
    {
        uart2_putc('0');
        return;
    }

    while (value && i < sizeof(buf))
    {
        buf[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (i)
        uart2_putc(buf[--i]);
}

static uint32_t ahb_div(uint32_t cfgr)
{
    static const uint8_t divs[] = {1,1,1,1,1,1,1,1,2,4,8,16,64,128,256,512};
    return divs[(cfgr >> 4) & 0x0Fu];
}

static uint32_t apb_div(uint32_t cfgr, unsigned int shift)
{
    static const uint8_t divs[] = {1,1,1,1,1,1,1,1,2,4,8,16,16,16,16,16};
    return divs[(cfgr >> shift) & 0x07u];
}

static uint32_t pll_multiplier(uint32_t cfgr)
{
    uint32_t bits = (cfgr >> 18) & 0x0Fu;

    /* 0000..1101 map to x2..x15; 1110 and 1111 are x16. */
    if (bits >= 14u)
        return 16u;
    return bits + 2u;
}

static uint32_t sysclk_hz(uint32_t cfgr)
{
    uint32_t sw = cfgr & RCC_CFGR_SWS;
    uint32_t pll_input;

    if (sw == RCC_CFGR_SWS_HSI)
        return HSI_VALUE;

    if (sw == RCC_CFGR_SWS_HSE)
        return HSE_VALUE;

    if (cfgr & RCC_CFGR_PLLSRC)
    {
        pll_input = HSE_VALUE;
        if (cfgr & RCC_CFGR_PLLXTPRE)
            pll_input /= 2u;
    }
    else
    {
        pll_input = HSI_VALUE / 2u;
    }

    return pll_input * pll_multiplier(cfgr);
}

static void print_gpio_pin(const char *name, GPIO_TypeDef *port, unsigned int pin)
{
    uint32_t config;
    uint32_t mode;
    uint32_t cnf;
    uint32_t level;

    if (pin < 8u)
        config = (port->CRL >> (pin * 4u)) & 0x0Fu;
    else
        config = (port->CRH >> ((pin - 8u) * 4u)) & 0x0Fu;

    mode = config & 0x03u;
    cnf = (config >> 2) & 0x03u;
    level = (port->IDR >> pin) & 1u;

    uart2_print("  ");
    uart2_print(name);
    uart2_print(" cfg=0x");
    hex32(config);
    uart2_print(" ");

    if (mode == 0u)
    {
        if (cnf == 0u)
            uart2_print("ANALOG");
        else if (cnf == 1u)
            uart2_print("IN_FLOATING");
        else
            uart2_print("IN_PULL");
    }
    else
    {
        uart2_print("OUT_");
        if (mode == 1u)
            uart2_print("10M");
        else if (mode == 2u)
            uart2_print("2M");
        else
            uart2_print("50M");

        if (cnf == 0u)
            uart2_print("_PP");
        else if (cnf == 1u)
            uart2_print("_OD");
        else if (cnf == 2u)
            uart2_print("_AFPP");
        else
            uart2_print("_AFOD");
    }

    uart2_print(" level=");
    uart2_putc(level ? '1' : '0');
    uart2_print("\r\n");
}

void diagnostics_print_mcu(void)
{
    volatile uint32_t *uid = (volatile uint32_t *)UID_BASE_ADDR;
    volatile uint16_t *flash_size = (volatile uint16_t *)FLASH_SIZE_ADDR;
    volatile uint32_t *idcode = (volatile uint32_t *)DBGMCU_IDCODE_ADDR;

    uart2_print("\r\nMCU IDENTITY:\r\n");
    uart2_print("  DBGMCU_IDCODE = 0x");
    hex32(*idcode);
    uart2_print("\r\n");
    uart2_print("  REV_ID        = 0x");
    hex32((*idcode >> 16) & 0xFFFFu);
    uart2_print("\r\n");
    uart2_print("  DEV_ID        = 0x");
    hex32(*idcode & 0x0FFFu);
    uart2_print("\r\n");
    uart2_print("  FLASH_SIZE_KB = ");
    dec32(*flash_size);
    uart2_print("\r\n");
    uart2_print("  UID           = ");
    hex32(uid[0]); uart2_putc('-');
    hex32(uid[1]); uart2_putc('-');
    hex32(uid[2]); uart2_print("\r\n");
}

void diagnostics_print_clock(void)
{
    uint32_t cr = RCC->CR;
    uint32_t cfgr = RCC->CFGR;
    uint32_t sys = sysclk_hz(cfgr);
    uint32_t hclk = sys / ahb_div(cfgr);
    uint32_t pclk1 = hclk / apb_div(cfgr, 8u);
    uint32_t pclk2 = hclk / apb_div(cfgr, 11u);

    uart2_print("\r\nCLOCK TREE:\r\n");
    uart2_print("  RCC->CR     = 0x"); hex32(cr); uart2_print("\r\n");
    uart2_print("  RCC->CFGR   = 0x"); hex32(cfgr); uart2_print("\r\n");
    uart2_print("  HSI READY   = "); uart2_print((cr & RCC_CR_HSIRDY) ? "YES\r\n" : "NO\r\n");
    uart2_print("  HSE READY   = "); uart2_print((cr & RCC_CR_HSERDY) ? "YES\r\n" : "NO\r\n");
    uart2_print("  PLL READY   = "); uart2_print((cr & RCC_CR_PLLRDY) ? "YES\r\n" : "NO\r\n");
    uart2_print("  SYSCLK      = "); dec32(sys); uart2_print(" Hz\r\n");
    uart2_print("  HCLK        = "); dec32(hclk); uart2_print(" Hz\r\n");
    uart2_print("  PCLK1       = "); dec32(pclk1); uart2_print(" Hz\r\n");
    uart2_print("  PCLK2       = "); dec32(pclk2); uart2_print(" Hz\r\n");
    uart2_print("  PLL INPUT   = ");
    if (cfgr & RCC_CFGR_PLLSRC)
        uart2_print((cfgr & RCC_CFGR_PLLXTPRE) ? "HSE/2\r\n" : "HSE\r\n");
    else
        uart2_print("HSI/2\r\n");
    uart2_print("  PLL MULT    = x"); dec32(pll_multiplier(cfgr)); uart2_print("\r\n");
    uart2_print("  AHB DIV     = "); dec32(ahb_div(cfgr)); uart2_print("\r\n");
    uart2_print("  APB1 DIV    = "); dec32(apb_div(cfgr, 8u)); uart2_print("\r\n");
    uart2_print("  APB2 DIV    = "); dec32(apb_div(cfgr, 11u)); uart2_print("\r\n");
    uart2_print("  RCC_APB1ENR = 0x"); hex32(RCC->APB1ENR); uart2_print("\r\n");
    uart2_print("  RCC_APB2ENR = 0x"); hex32(RCC->APB2ENR); uart2_print("\r\n");
}

void diagnostics_print_audio_pins(void)
{
    uart2_print("\r\nAUDIO / BUS GPIO:\r\n");
    uart2_print("  I2C1: PB6=SCL PB7=SDA\r\n");
    print_gpio_pin("PB6 ", GPIOB, 6u);
    print_gpio_pin("PB7 ", GPIOB, 7u);
    uart2_print("  CODEC I2S: PB12=WCLK PB13=BCLK PB15=DOUT\r\n");
    print_gpio_pin("PB12", GPIOB, 12u);
    print_gpio_pin("PB13", GPIOB, 13u);
    print_gpio_pin("PB15", GPIOB, 15u);
    print_gpio_pin("PB14", GPIOB, 14u);
    uart2_print("  GPIOB->CRL = 0x"); hex32(GPIOB->CRL); uart2_print("\r\n");
    uart2_print("  GPIOB->CRH = 0x"); hex32(GPIOB->CRH); uart2_print("\r\n");
    uart2_print("  GPIOB->IDR = 0x"); hex32(GPIOB->IDR); uart2_print("\r\n");
    uart2_print("  GPIOB->ODR = 0x"); hex32(GPIOB->ODR); uart2_print("\r\n");
}

void diagnostics_print_i2c1(void)
{
    uart2_print("\r\nI2C1 HARDWARE STATE:\r\n");
    uart2_print("  APB1 CLOCK = ");
    uart2_print((RCC->APB1ENR & RCC_APB1Periph_I2C1) ? "ON\r\n" : "OFF\r\n");
    uart2_print("  CR1        = 0x"); hex32(I2C1->CR1); uart2_print("\r\n");
    uart2_print("  CR2        = 0x"); hex32(I2C1->CR2); uart2_print("\r\n");
    uart2_print("  OAR1       = 0x"); hex32(I2C1->OAR1); uart2_print("\r\n");
    uart2_print("  CCR        = 0x"); hex32(I2C1->CCR); uart2_print("\r\n");
    uart2_print("  TRISE      = 0x"); hex32(I2C1->TRISE); uart2_print("\r\n");
    uart2_print("  SR1        = 0x"); hex32(I2C1->SR1); uart2_print("\r\n");
    uart2_print("  SR2        = 0x"); hex32(I2C1->SR2); uart2_print("\r\n");
}

void diagnostics_print_i2s2(void)
{
    uart2_print("\r\nSTM32 SPI2/I2S HARDWARE STATE:\r\n");
    uart2_print("  APB1 CLOCK = ");
    uart2_print((RCC->APB1ENR & RCC_APB1Periph_SPI2) ? "ON\r\n" : "OFF\r\n");
    uart2_print("  CR1        = 0x"); hex32(SPI2->CR1); uart2_print("\r\n");
    uart2_print("  CR2        = 0x"); hex32(SPI2->CR2); uart2_print("\r\n");
    uart2_print("  SR         = 0x"); hex32(SPI2->SR); uart2_print("\r\n");
    uart2_print("  I2SCFGR    = 0x"); hex32(SPI2->I2SCFGR); uart2_print("\r\n");
    uart2_print("  I2SPR      = 0x"); hex32(SPI2->I2SPR); uart2_print("\r\n");
}

int diagnostics_i2c1_safe(void)
{
    uint32_t pb6 = (GPIOB->CRL >> 24) & 0x0Fu;
    uint32_t pb7 = (GPIOB->CRL >> 28) & 0x0Fu;
    uint32_t levels = GPIOB->IDR & (GPIO_Pin_6 | GPIO_Pin_7);

    /* AF open-drain only: never accept push-pull output or pull-up mode. */
    if (pb6 != 0x0Fu || pb7 != 0x0Fu)
        return 0;

    /* Both I2C lines must be released high before we generate START. */
    if (levels != (GPIO_Pin_6 | GPIO_Pin_7))
        return 0;

    return 1;
}

int diagnostics_i2s2_safe(void)
{
    uint32_t pb12 = (GPIOB->CRH >> 16) & 0x0Fu;
    uint32_t pb13 = (GPIOB->CRH >> 20) & 0x0Fu;
    uint32_t pb15 = (GPIOB->CRH >> 28) & 0x0Fu;

    /* Codec drives these pins; STM32 must not drive them as outputs. */
    if ((pb12 & 0x03u) != 0u || (pb13 & 0x03u) != 0u || (pb15 & 0x03u) != 0u)
        return 0;

    return 1;
}
