#include "stm32f10x.h"
#include "i2c.h"

#define I2C2_TIMEOUT 100000u

/* PB10/PB11 are deliberately recovered as ordinary GPIO before the I2C
 * peripheral is enabled.  This prevents a stale I2C START/BUSY state from
 * leaving SDA low after reset and gives us a deterministic electrical bus
 * check before talking to the codec. */
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

    /* I2C2 must be disabled while the pins are used for manual recovery. */
    I2C_Cmd(I2C2, DISABLE);

    gpio.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);

    /* Release both lines.  External pull-ups should take them high. */
    GPIO_SetBits(GPIOB, GPIO_Pin_10 | GPIO_Pin_11);
    i2c2_gpio_delay();

    /* If a slave was interrupted mid-byte, up to nine clocks lets it finish
     * the byte and release SDA.  Because these are open-drain outputs, the
     * GPIO high state only releases the line; it never drives it high. */
    for (i = 0; i < 9u; ++i)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_10);
        i2c2_gpio_delay();
        GPIO_SetBits(GPIOB, GPIO_Pin_10);
        i2c2_gpio_delay();
    }

    /* Generate a STOP while SCL is released high: SDA low -> high. */
    GPIO_ResetBits(GPIOB, GPIO_Pin_11);
    i2c2_gpio_delay();
    GPIO_SetBits(GPIOB, GPIO_Pin_10);
    i2c2_gpio_delay();
    GPIO_SetBits(GPIOB, GPIO_Pin_11);
    i2c2_gpio_delay();
}

static int i2c2_bus_idle(void)
{
    return ((GPIOB->IDR & (GPIO_Pin_10 | GPIO_Pin_11)) ==
            (GPIO_Pin_10 | GPIO_Pin_11));
}

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

    /* Reset/recover the physical bus before handing PB10/PB11 to the I2C
     * alternate function.  This is especially important after a debugger
     * reset or a slave reset in the middle of an I2C transaction. */
    i2c2_bus_recover();

    /* Put the peripheral into a known reset state and clear stale status. */
    I2C_DeInit(I2C2);
    I2C_Cmd(I2C2, DISABLE);
    I2C2->CR1 |= I2C_CR1_SWRST;
    I2C2->CR1 &= ~I2C_CR1_SWRST;

    /* PB10 = I2C2_SCL, PB11 = I2C2_SDA.  Open drain + external pull-ups. */
    gpio.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    gpio.GPIO_Mode = GPIO_Mode_AF_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

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

    /* Never start a transaction while the physical bus is low. */
    if (!i2c2_bus_idle())
    {
        I2C_GenerateSTOP(I2C2, ENABLE);
        return 0;
    }

    /* Clear stale status before starting. */
    I2C2->SR1 = 0u;
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

        if (sr1 & (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR))
            goto fail;
    }

fail:
    I2C_GenerateSTOP(I2C2, ENABLE);
    I2C2->SR1 = 0u;
    return 0;
}

int i2c2_write(uint8_t address, uint8_t reg, uint8_t data)
{
    uint32_t timeout;

    if (!i2c2_bus_idle())
        return 0;

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
    I2C2->SR1 = 0u;
    return 0;
}

int i2c2_read(uint8_t address, uint8_t reg, uint8_t *data)
{
    uint32_t timeout;

    if (data == 0 || !i2c2_bus_idle())
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
    I2C2->SR1 = 0u;
    return 0;
}
