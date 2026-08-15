#include "stm32f10x.h"
#include "i2c.h"

#define I2C2_TIMEOUT 100000u

static int i2c2_wait_flag(uint32_t flag)
{
    uint32_t timeout = I2C2_TIMEOUT;

    while ((I2C2->SR1 & flag) == 0u)
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

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);

    /* PB10 = I2C2_SCL, PB11 = I2C2_SDA.
     * I2C requires external pull-ups; do not enable push-pull here.
     */
    gpio.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    gpio.GPIO_Mode = GPIO_Mode_AF_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    I2C_DeInit(I2C2);

    i2c.I2C_Mode = I2C_Mode_I2C;
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1 = 0x00;
    i2c.I2C_Ack = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    i2c.I2C_ClockSpeed = 100000u;

    I2C_Init(I2C2, &i2c);
    I2C_Cmd(I2C2, ENABLE);
}

int i2c2_probe(uint8_t address)
{
    uint32_t timeout;

    /* Clear any stale error/STOP state before starting. */
    (void)I2C2->SR1;
    (void)I2C2->SR2;

    I2C_GenerateSTART(I2C2, ENABLE);
    if (!i2c2_wait_flag(I2C_SR1_SB))
        goto fail;

    I2C_Send7bitAddress(I2C2, (uint8_t)(address << 1), I2C_Direction_Transmitter);

    timeout = I2C2_TIMEOUT;
    while (timeout--)
    {
        uint32_t sr1 = I2C2->SR1;

        if (sr1 & I2C_SR1_ADDR)
        {
            (void)I2C2->SR1;
            (void)I2C2->SR2;
            I2C_GenerateSTOP(I2C2, ENABLE);
            return 1;
        }

        if (sr1 & I2C_SR1_AF)
        {
            I2C_ClearFlag(I2C2, I2C_FLAG_AF);
            I2C_GenerateSTOP(I2C2, ENABLE);
            return 0;
        }
    }

fail:
    I2C_GenerateSTOP(I2C2, ENABLE);
    return 0;
}

int i2c2_write(uint8_t address, uint8_t reg, uint8_t data)
{
    uint32_t timeout;

    I2C_GenerateSTART(I2C2, ENABLE);
    if (!i2c2_wait_flag(I2C_SR1_SB))
        goto fail;

    I2C_Send7bitAddress(I2C2, (uint8_t)(address << 1), I2C_Direction_Transmitter);
    if (!i2c2_wait_flag(I2C_SR1_ADDR))
        goto fail;
    (void)I2C2->SR1;
    (void)I2C2->SR2;

    I2C_SendData(I2C2, reg);
    if (!i2c2_wait_flag(I2C_SR1_TXE))
        goto fail;

    I2C_SendData(I2C2, data);
    if (!i2c2_wait_flag(I2C_SR1_BTF))
        goto fail;

    I2C_GenerateSTOP(I2C2, ENABLE);
    return 1;

fail:
    timeout = I2C2_TIMEOUT;
    I2C_GenerateSTOP(I2C2, ENABLE);
    while ((I2C2->CR1 & I2C_CR1_STOP) && --timeout)
        ;
    I2C_ClearFlag(I2C2, I2C_FLAG_AF);
    return 0;
}

int i2c2_read(uint8_t address, uint8_t reg, uint8_t *data)
{
    uint32_t timeout;

    if (data == 0)
        return 0;

    /* Write the register address. */
    I2C_GenerateSTART(I2C2, ENABLE);
    if (!i2c2_wait_flag(I2C_SR1_SB))
        goto fail;

    I2C_Send7bitAddress(I2C2, (uint8_t)(address << 1), I2C_Direction_Transmitter);
    if (!i2c2_wait_flag(I2C_SR1_ADDR))
        goto fail;
    (void)I2C2->SR1;
    (void)I2C2->SR2;

    I2C_SendData(I2C2, reg);
    if (!i2c2_wait_flag(I2C_SR1_BTF))
        goto fail;

    /* Repeated START, then one-byte receive. */
    I2C_GenerateSTART(I2C2, ENABLE);
    if (!i2c2_wait_flag(I2C_SR1_SB))
        goto fail;

    I2C_Send7bitAddress(I2C2, (uint8_t)(address << 1), I2C_Direction_Receiver);
    if (!i2c2_wait_flag(I2C_SR1_ADDR))
        goto fail;

    I2C_AcknowledgeConfig(I2C2, DISABLE);
    (void)I2C2->SR1;
    (void)I2C2->SR2;
    I2C_GenerateSTOP(I2C2, ENABLE);

    timeout = I2C2_TIMEOUT;
    while (((I2C2->SR1 & I2C_SR1_RXNE) == 0u) && timeout--)
        ;
    if (timeout == 0u)
        goto fail_no_stop;

    *data = I2C_ReceiveData(I2C2);
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    return 1;

fail:
    I2C_GenerateSTOP(I2C2, ENABLE);
fail_no_stop:
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    I2C_ClearFlag(I2C2, I2C_FLAG_AF);
    return 0;
}
