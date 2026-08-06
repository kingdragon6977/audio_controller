#include "stm32f10x.h"
#include "i2c.h"


void i2c2_init(void)
{
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef i2c;


    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOB |
        RCC_APB2Periph_AFIO,
        ENABLE);


    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_I2C2,
        ENABLE);


    /*
       PB10 = I2C2_SCL
       PB11 = I2C2_SDA
    */

    gpio.GPIO_Pin =
        GPIO_Pin_10 |
        GPIO_Pin_11;

    gpio.GPIO_Mode =
        GPIO_Mode_AF_OD;

    gpio.GPIO_Speed =
        GPIO_Speed_50MHz;

    GPIO_Init(
        GPIOB,
        &gpio);


    I2C_DeInit(I2C2);


    i2c.I2C_Mode =
        I2C_Mode_I2C;

    i2c.I2C_DutyCycle =
        I2C_DutyCycle_2;

    i2c.I2C_OwnAddress1 =
        0x00;

    i2c.I2C_Ack =
        I2C_Ack_Enable;

    i2c.I2C_AcknowledgedAddress =
        I2C_AcknowledgedAddress_7bit;

    i2c.I2C_ClockSpeed =
        100000;


    I2C_Init(
        I2C2,
        &i2c);


    I2C_Cmd(
        I2C2,
        ENABLE);
}
