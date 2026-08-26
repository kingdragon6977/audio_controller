#include "stm32f10x.h"
#include "i2c.h"

#define I2C1_TIMEOUT 100000u

static int i2c1_wait_flag(uint32_t flag)
{
    uint32_t timeout = I2C1_TIMEOUT;

    while ((I2C1->SR1 & flag) == 0u)
    {
        if (I2C1->SR1 & I2C_SR1_AF)
            return 0;

        if (--timeout == 0u)
            return 0;
    }

    return 1;
}

static int i2c1_wait_tx_complete(void)
{
    uint32_t timeout = I2C1_TIMEOUT;

    while (timeout--)
    {
        uint32_t sr1 = I2C1->SR1;

        if (sr1 & I2C_SR1_AF)
            return 0;

        if (sr1 & I2C_SR1_BTF)
            return 1;
    }

    return 0;
}

void i2c1_init(void)
{
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef i2c;

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO,
        ENABLE);

    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_I2C1,
        ENABLE);

    gpio.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode  = GPIO_Mode_AF_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    I2C_DeInit(I2C1);

    i2c.I2C_Mode = I2C_Mode_I2C;
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1 = 0x00u;
    i2c.I2C_Ack = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    i2c.I2C_ClockSpeed = 100000u;

    I2C_Init(I2C1, &i2c);
    I2C_Cmd(I2C1, ENABLE);
    I2C_AcknowledgeConfig(I2C1, ENABLE);
}

int i2c1_probe(uint8_t address)
{
    uint32_t timeout;
    uint32_t sr1;

    (void)I2C1->SR1;
    (void)I2C1->SR2;

    if (I2C1->SR2 & I2C_SR2_BUSY)
    {
        I2C_GenerateSTOP(I2C1, ENABLE);

        timeout = I2C1_TIMEOUT;
        while ((I2C1->SR2 & I2C_SR2_BUSY) && --timeout)
            __asm__("nop");

        if (I2C1->SR2 & I2C_SR2_BUSY)
        {
            I2C_SoftwareResetCmd(I2C1, ENABLE);
            I2C_SoftwareResetCmd(I2C1, DISABLE);
            I2C_Cmd(I2C1, ENABLE);
            I2C_AcknowledgeConfig(I2C1, ENABLE);
            return 0;
        }
    }

    I2C_GenerateSTART(I2C1, ENABLE);

    if (!i2c1_wait_flag(I2C_SR1_SB))
    {
        I2C_GenerateSTOP(I2C1, ENABLE);
        I2C_ClearFlag(I2C1, I2C_FLAG_AF);
        return 0;
    }

    I2C_Send7bitAddress(
        I2C1,
        (uint8_t)(address << 1),
        I2C_Direction_Transmitter);

    timeout = I2C1_TIMEOUT;
    while (timeout--)
    {
        sr1 = I2C1->SR1;

        if (sr1 & I2C_SR1_ADDR)
        {
            (void)I2C1->SR1;
            (void)I2C1->SR2;
            I2C_GenerateSTOP(I2C1, ENABLE);
            return 1;
        }

        if (sr1 & I2C_SR1_AF)
        {
            I2C_ClearFlag(I2C1, I2C_FLAG_AF);
            I2C_GenerateSTOP(I2C1, ENABLE);
            return 0;
        }
    }

    I2C_GenerateSTOP(I2C1, ENABLE);
    return 0;
}

int i2c1_write(
    uint8_t address,
    uint8_t reg,
    uint8_t data)
{
    uint32_t timeout;

    /* Clear a stale acknowledge-failure flag before starting. */
    I2C_ClearFlag(I2C1, I2C_FLAG_AF);

    I2C_GenerateSTART(I2C1, ENABLE);

    if (!i2c1_wait_flag(I2C_SR1_SB))
        goto fail;

    I2C_Send7bitAddress(
        I2C1,
        (uint8_t)(address << 1),
        I2C_Direction_Transmitter);

    /* ADDR must be observed before sending the register byte. */
    if (!i2c1_wait_flag(I2C_SR1_ADDR))
        goto fail;

    (void)I2C1->SR1;
    (void)I2C1->SR2;

    I2C_SendData(I2C1, reg);

    /* TXE alone is not an ACK. Require BTF and reject AF. */
    if (!i2c1_wait_tx_complete())
        goto fail;

    I2C_SendData(I2C1, data);

    /* This is the important fix: a NACK cannot silently look successful. */
    if (!i2c1_wait_tx_complete())
        goto fail;

    I2C_GenerateSTOP(I2C1, ENABLE);

    timeout = I2C1_TIMEOUT;
    while ((I2C1->CR1 & I2C_CR1_STOP) && --timeout)
        __asm__("nop");

    I2C_ClearFlag(I2C1, I2C_FLAG_AF);
    return 1;

fail:
    I2C_GenerateSTOP(I2C1, ENABLE);
    I2C_ClearFlag(I2C1, I2C_FLAG_AF);
    return 0;
}

int i2c1_read(
    uint8_t address,
    uint8_t reg,
    uint8_t *data)
{
    uint32_t timeout;

    if (data == 0)
        return 0;

    I2C_ClearFlag(I2C1, I2C_FLAG_AF);
    I2C_GenerateSTART(I2C1, ENABLE);

    if (!i2c1_wait_flag(I2C_SR1_SB))
        goto fail;

    I2C_Send7bitAddress(
        I2C1,
        (uint8_t)(address << 1),
        I2C_Direction_Transmitter);

    if (!i2c1_wait_flag(I2C_SR1_ADDR))
        goto fail;

    (void)I2C1->SR1;
    (void)I2C1->SR2;

    I2C_SendData(I2C1, reg);

    if (!i2c1_wait_tx_complete())
        goto fail;

    I2C_GenerateSTART(I2C1, ENABLE);

    if (!i2c1_wait_flag(I2C_SR1_SB))
        goto fail;

    I2C_Send7bitAddress(
        I2C1,
        (uint8_t)(address << 1),
        I2C_Direction_Receiver);

    if (!i2c1_wait_flag(I2C_SR1_ADDR))
        goto fail;

    I2C_AcknowledgeConfig(I2C1, DISABLE);

    (void)I2C1->SR1;
    (void)I2C1->SR2;

    I2C_GenerateSTOP(I2C1, ENABLE);

    timeout = I2C1_TIMEOUT;
    while (((I2C1->SR1 & I2C_SR1_RXNE) == 0u) && timeout--)
        __asm__("nop");

    if ((I2C1->SR1 & I2C_SR1_RXNE) == 0u)
        goto fail;

    *data = I2C_ReceiveData(I2C1);
    I2C_AcknowledgeConfig(I2C1, ENABLE);
    I2C_ClearFlag(I2C1, I2C_FLAG_AF);
    return 1;

fail:
    I2C_GenerateSTOP(I2C1, ENABLE);
    I2C_AcknowledgeConfig(I2C1, ENABLE);
    I2C_ClearFlag(I2C1, I2C_FLAG_AF);
    return 0;
}
