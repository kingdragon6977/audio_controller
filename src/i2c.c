#include "stm32f10x.h"
#include "i2c.h"

#define I2C2_TIMEOUT 100000u
#define CODEC_I2C I2C1
#define CODEC_I2C_RCC RCC_APB1Periph_I2C1
#define CODEC_I2C_SCL GPIO_Pin_6
#define CODEC_I2C_SDA GPIO_Pin_7

/* Temporary diagnostic configuration:
 * use I2C1 on PB6/PB7 instead of I2C2 on PB10/PB11. */
static void i2c2_gpio_delay(void)
{
    volatile uint32_t n = 80u;
    while (n--)
        __asm__("nop");
}

static void i2c2_bus_recover(void)
{
    GPIO_InitTypeDef gpio;
    unsigned int i;

    I2C_Cmd(CODEC_I2C, DISABLE);

    gpio.GPIO_Pin = CODEC_I2C_SCL | CODEC_I2C_SDA;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);

    GPIO_SetBits(GPIOB, CODEC_I2C_SCL | CODEC_I2C_SDA);
    i2c2_gpio_delay();

    for (i = 0; i < 9u; ++i)
    {
        GPIO_ResetBits(GPIOB, CODEC_I2C_SCL);
        i2c2_gpio_delay();
        GPIO_SetBits(GPIOB, CODEC_I2C_SCL);
        i2c2_gpio_delay();
    }

    GPIO_ResetBits(GPIOB, CODEC_I2C_SDA);
    i2c2_gpio_delay();
    GPIO_SetBits(GPIOB, CODEC_I2C_SCL);
    i2c2_gpio_delay();
    GPIO_SetBits(GPIOB, CODEC_I2C_SDA);
    i2c2_gpio_delay();
}

static int i2c2_bus_idle(void)
{
    return ((GPIOB->IDR & (CODEC_I2C_SCL | CODEC_I2C_SDA)) ==
            (CODEC_I2C_SCL | CODEC_I2C_SDA));
}

static int i2c2_wait_flag(uint32_t flag)
{
    uint32_t timeout = I2C2_TIMEOUT;

    while ((CODEC_I2C->SR1 & flag) == 0u)
    {
        if (--timeout == 0u)
            return 0;
    }

    return 1;
}

void i2c2_init(void)
{
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef i2c;

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO,
        ENABLE);
    RCC_APB1PeriphClockCmd(CODEC_I2C_RCC, ENABLE);

    i2c2_bus_recover();

    I2C_DeInit(CODEC_I2C);
    I2C_Cmd(CODEC_I2C, DISABLE);
    CODEC_I2C->CR1 |= I2C_CR1_SWRST;
    CODEC_I2C->CR1 &= ~I2C_CR1_SWRST;

    /* PB6 = I2C1_SCL, PB7 = I2C1_SDA. */
    gpio.GPIO_Pin = CODEC_I2C_SCL | CODEC_I2C_SDA;
    gpio.GPIO_Mode = GPIO_Mode_AF_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    i2c.I2C_Mode = I2C_Mode_I2C;
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1 = 0x00;
    i2c.I2C_Ack = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    i2c.I2C_ClockSpeed = 100000u;

    I2C_Init(CODEC_I2C, &i2c);
    I2C_Cmd(CODEC_I2C, ENABLE);
}

int i2c2_probe(uint8_t address)
{
    uint32_t timeout;

    if (!i2c2_bus_idle())
    {
        I2C_GenerateSTOP(CODEC_I2C, ENABLE);
        return 0;
    }

    CODEC_I2C->SR1 = 0u;
    (void)CODEC_I2C->SR2;

    I2C_GenerateSTART(CODEC_I2C, ENABLE);
    if (!i2c2_wait_flag(I2C_SR1_SB))
        goto fail;

    I2C_Send7bitAddress(CODEC_I2C, (uint8_t)(address << 1), I2C_Direction_Transmitter);

    timeout = I2C2_TIMEOUT;
    while (timeout--)
    {
        uint32_t sr1 = CODEC_I2C->SR1;

        if (sr1 & I2C_SR1_ADDR)
        {
            (void)CODEC_I2C->SR1;
            (void)CODEC_I2C->SR2;
            I2C_GenerateSTOP(CODEC_I2C, ENABLE);
            return 1;
        }

        if (sr1 & I2C_SR1_AF)
        {
            I2C_ClearFlag(CODEC_I2C, I2C_FLAG_AF);
            I2C_GenerateSTOP(CODEC_I2C, ENABLE);
            return 0;
        }

        if (sr1 & (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR))
            goto fail;
    }

fail:
    I2C_GenerateSTOP(CODEC_I2C, ENABLE);
    CODEC_I2C->SR1 = 0u;
    return 0;
}

int i2c2_write(uint8_t address, uint8_t reg, uint8_t data)
{
    uint32_t timeout;

    if (!i2c2_bus_idle())
        return 0;

    I2C_GenerateSTART(CODEC_I2C, ENABLE);
    if (!i2c2_wait_flag(I2C_SR1_SB))
        goto fail;

    I2C_Send7bitAddress(CODEC_I2C, (uint8_t)(address << 1), I2C_Direction_Transmitter);
    if (!i2c2_wait_flag(I2C_SR1_ADDR))
        goto fail;
    (void)CODEC_I2C->SR1;
    (void)CODEC_I2C->SR2;

    I2C_SendData(CODEC_I2C, reg);
    if (!i2c2_wait_flag(I2C_SR1_TXE))
        goto fail;

    I2C_SendData(CODEC_I2C, data);
    if (!i2c2_wait_flag(I2C_SR1_BTF))
        goto fail;

    I2C_GenerateSTOP(CODEC_I2C, ENABLE);
    return 1;

fail:
    timeout = I2C2_TIMEOUT;
    I2C_GenerateSTOP(CODEC_I2C, ENABLE);
    while ((CODEC_I2C->CR1 & I2C_CR1_STOP) && --timeout)
        ;
    CODEC_I2C->SR1 = 0u;
    return 0;
}

int i2c2_read(uint8_t address, uint8_t reg, uint8_t *data)
{
    uint32_t timeout;

    if (data == 0 || !i2c2_bus_idle())
        return 0;

    I2C_GenerateSTART(CODEC_I2C, ENABLE);
    if (!i2c2_wait_flag(I2C_SR1_SB))
        goto fail;

    I2C_Send7bitAddress(CODEC_I2C, (uint8_t)(address << 1), I2C_Direction_Transmitter);
    if (!i2c2_wait_flag(I2C_SR1_ADDR))
        goto fail;
    (void)CODEC_I2C->SR1;
    (void)CODEC_I2C->SR2;

    I2C_SendData(CODEC_I2C, reg);
    if (!i2c2_wait_flag(I2C_SR1_BTF))
        goto fail;

    I2C_GenerateSTART(CODEC_I2C, ENABLE);
    if (!i2c2_wait_flag(I2C_SR1_SB))
        goto fail;

    I2C_Send7bitAddress(CODEC_I2C, (uint8_t)(address << 1), I2C_Direction_Receiver);
    if (!i2c2_wait_flag(I2C_SR1_ADDR))
        goto fail;

    I2C_AcknowledgeConfig(CODEC_I2C, DISABLE);
    (void)CODEC_I2C->SR1;
    (void)CODEC_I2C->SR2;
    I2C_GenerateSTOP(CODEC_I2C, ENABLE);

    timeout = I2C2_TIMEOUT;
    while (((CODEC_I2C->SR1 & I2C_SR1_RXNE) == 0u) && timeout--)
        ;
    if (timeout == 0u)
        goto fail_no_stop;

    *data = I2C_ReceiveData(CODEC_I2C);
    I2C_AcknowledgeConfig(CODEC_I2C, ENABLE);
    return 1;

fail:
    I2C_GenerateSTOP(CODEC_I2C, ENABLE);
fail_no_stop:
    I2C_AcknowledgeConfig(CODEC_I2C, ENABLE);
    CODEC_I2C->SR1 = 0u;
    return 0;
}
