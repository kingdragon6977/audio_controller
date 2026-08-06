#include "stm32f10x.h"
#include "codec.h"


#define TLV_ADDR 0x18


void i2c2_write(
    uint8_t reg,
    uint8_t data)
{

    while(I2C_GetFlagStatus(
        I2C2,
        I2C_FLAG_BUSY));


    I2C_GenerateSTART(
        I2C2,
        ENABLE);


    while(!I2C_CheckEvent(
        I2C2,
        I2C_EVENT_MASTER_MODE_SELECT));


    I2C_Send7bitAddress(
        I2C2,
        TLV_ADDR<<1,
        I2C_Direction_Transmitter);



    while(!I2C_CheckEvent(
        I2C2,
        I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));


    I2C_SendData(
        I2C2,
        reg);


    while(!I2C_CheckEvent(
        I2C2,
        I2C_EVENT_MASTER_BYTE_TRANSMITTED));


    I2C_SendData(
        I2C2,
        data);



    while(!I2C_CheckEvent(
        I2C2,
        I2C_EVENT_MASTER_BYTE_TRANSMITTED));


    I2C_GenerateSTOP(
        I2C2,
        ENABLE);

}



void codec_init(void)
{

    /*
       Software reset
       Page 0 register 1
    */

    i2c2_write(
        0x00,
        0x00);


    i2c2_write(
        0x01,
        0x01);



    for(volatile int i=0;i<100000;i++);



    /*
       Select page 0
    */

    i2c2_write(
        0x00,
        0x00);



    /*
       Power up ADC
       register 81
    */

    i2c2_write(
        0x51,
        0x80);



    /*
       ADC input PGA gain
       register 82
    */

    i2c2_write(
        0x52,
        0x00);



    /*
       I2S output format
       register 27

       I2S
       16 bit
       slave/master later
    */

    i2c2_write(
        0x1B,
        0x00);



    /*
       Enable ADC channel
       register 82
    */

    i2c2_write(
        0x52,
        0x00);


}
