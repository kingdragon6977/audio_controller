#include "stm32f10x.h"
#include "i2c.h"

#define I2C1_TIMEOUT 100000u

static int i2c1_wait_flag(uint32_t flag)
{
    uint32_t timeout = I2C1_TIMEOUT;

    while ((I2C1->SR1 & flag) == 0u)
    {
        if (--timeout == 0u)
            return 0;
    }

    return 1;
}

void i2c1_init(void)
{
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef i2c;

    /*
     * Enable GPIOB and AFIO clocks.
     */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO,
        ENABLE);

    /*
     * Enable I2C1 peripheral clock.
     */
    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_I2C1,
        ENABLE);

    /*
     * I2C1 pin assignment:
     *
     * PB6 = SCL
     * PB7 = SDA
     *
     * I2C requires open-drain outputs.
     * External pull-ups are required.
     */
    gpio.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode  = GPIO_Mode_AF_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(GPIOB, &gpio);

    /*
     * Reset I2C1 before configuring it.
     */
    I2C_DeInit(I2C1);

    /*
     * Configure I2C1 for:
     *
     * 100 kHz
     * 7-bit addressing
     * ACK enabled
     * Standard duty cycle
     */
    i2c.I2C_Mode = I2C_Mode_I2C;
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1 = 0x00u;
    i2c.I2C_Ack = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    i2c.I2C_ClockSpeed = 100000u;

    I2C_Init(I2C1, &i2c);

    /*
     * Enable peripheral.
     */
    I2C_Cmd(I2C1, ENABLE);

    /*
     * Make sure ACK is enabled after initialization.
     */
    I2C_AcknowledgeConfig(I2C1, ENABLE);
}

int i2c1_probe(uint8_t address)
{
    uint32_t timeout;
    uint32_t sr1;

    /*
     * Clear any stale status flags by reading the status registers.
     */
    (void)I2C1->SR1;
    (void)I2C1->SR2;

    /*
     * If the peripheral thinks it is still in master mode,
     * reset it before starting a new transaction.
     */
    if (I2C1->SR2 & I2C_SR2_BUSY)
    {
        I2C_GenerateSTOP(I2C1, ENABLE);

        timeout = I2C1_TIMEOUT;

        while ((I2C1->SR2 & I2C_SR2_BUSY) && --timeout)
        {
            __asm__("nop");
        }

        if (I2C1->SR2 & I2C_SR2_BUSY)
        {
            I2C_SoftwareResetCmd(I2C1, ENABLE);
            I2C_SoftwareResetCmd(I2C1, DISABLE);

            I2C_Cmd(I2C1, ENABLE);
            I2C_AcknowledgeConfig(I2C1, ENABLE);

            return 0;
        }
    }

    /*
     * Generate START.
     */
    I2C_GenerateSTART(I2C1, ENABLE);

    if (!i2c1_wait_flag(I2C_SR1_SB))
    {
        I2C_GenerateSTOP(I2C1, ENABLE);
        return 0;
    }

    /*
     * Send address in 7-bit form.
     * STM32 StdPeriph library expects the shifted address.
     */
    I2C_Send7bitAddress(
        I2C1,
        (uint8_t)(address << 1),
        I2C_Direction_Transmitter);

    /*
     * Wait for either ADDR or AF.
     */
    timeout = I2C1_TIMEOUT;

    while (timeout--)
    {
        sr1 = I2C1->SR1;

        /*
         * ACK received.
         */
        if (sr1 & I2C_SR1_ADDR)
        {
            /*
             * Clear ADDR by reading SR1 then SR2.
             */
            (void)I2C1->SR1;
            (void)I2C1->SR2;

            I2C_GenerateSTOP(I2C1, ENABLE);

            return 1;
        }

        /*
         * NACK received.
         */
        if (sr1 & I2C_SR1_AF)
        {
            I2C_ClearFlag(I2C1, I2C_FLAG_AF);
            I2C_GenerateSTOP(I2C1, ENABLE);

            return 0;
        }
    }

    /*
     * Timeout.
     */
    I2C_GenerateSTOP(I2C1, ENABLE);

    return 0;
}

int i2c1_write(
    uint8_t address,
    uint8_t reg,
    uint8_t data)
{
    uint32_t timeout;

    /*
     * START.
     */
    I2C_GenerateSTART(I2C1, ENABLE);

    if (!i2c1_wait_flag(I2C_SR1_SB))
        return 0;

    /*
     * Address + write.
     */
    I2C_Send7bitAddress(
        I2C1,
        (uint8_t)(address << 1),
        I2C_Direction_Transmitter);

    if (!i2c1_wait_flag(I2C_SR1_ADDR))
        goto fail;

    /*
     * Clear ADDR.
     */
    (void)I2C1->SR1;
    (void)I2C1->SR2;

    /*
     * Register address.
     */
    I2C_SendData(I2C1, reg);

    if (!i2c1_wait_flag(I2C_SR1_TXE))
        goto fail;

    /*
     * Data.
     */
    I2C_SendData(I2C1, data);

    if (!i2c1_wait_flag(I2C_SR1_BTF))
        goto fail;

    /*
     * STOP.
     */
    I2C_GenerateSTOP(I2C1, ENABLE);

    /*
     * Wait briefly for STOP to clear.
     */
    timeout = I2C1_TIMEOUT;

    while ((I2C1->CR1 & I2C_CR1_STOP) && --timeout)
    {
        __asm__("nop");
    }

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

    /*
     * START.
     */
    I2C_GenerateSTART(I2C1, ENABLE);

    if (!i2c1_wait_flag(I2C_SR1_SB))
        return 0;

    /*
     * Address + write.
     */
    I2C_Send7bitAddress(
        I2C1,
        (uint8_t)(address << 1),
        I2C_Direction_Transmitter);

    if (!i2c1_wait_flag(I2C_SR1_ADDR))
        goto fail;

    /*
     * Clear ADDR.
     */
    (void)I2C1->SR1;
    (void)I2C1->SR2;

    /*
     * Register.
     */
    I2C_SendData(I2C1, reg);

    if (!i2c1_wait_flag(I2C_SR1_BTF))
        goto fail;

    /*
     * Repeated START.
     */
    I2C_GenerateSTART(I2C1, ENABLE);

    if (!i2c1_wait_flag(I2C_SR1_SB))
        goto fail;

    /*
     * Address + read.
     */
    I2C_Send7bitAddress(
        I2C1,
        (uint8_t)(address << 1),
        I2C_Direction_Receiver);

    if (!i2c1_wait_flag(I2C_SR1_ADDR))
        goto fail;

    /*
     * One-byte receive:
     *
     * Disable ACK before clearing ADDR.
     */
    I2C_AcknowledgeConfig(I2C1, DISABLE);

    /*
     * Clear ADDR.
     */
    (void)I2C1->SR1;
    (void)I2C1->SR2;

    /*
     * Generate STOP before receiving the final byte.
     */
    I2C_GenerateSTOP(I2C1, ENABLE);

    /*
     * Wait for RXNE.
     */
    timeout = I2C1_TIMEOUT;

    while (((I2C1->SR1 & I2C_SR1_RXNE) == 0u) && timeout--)
    {
        __asm__("nop");
    }

    if ((I2C1->SR1 & I2C_SR1_RXNE) == 0u)
        goto fail;

    *data = I2C_ReceiveData(I2C1);

    /*
     * Restore ACK.
     */
    I2C_AcknowledgeConfig(I2C1, ENABLE);

    return 1;

fail:

    I2C_GenerateSTOP(I2C1, ENABLE);
    I2C_AcknowledgeConfig(I2C1, ENABLE);
    I2C_ClearFlag(I2C1, I2C_FLAG_AF);

    return 0;
}
