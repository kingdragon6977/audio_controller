#include "stm32f10x.h"
#include "i2s.h"


void i2s2_init(void)
{
    GPIO_InitTypeDef gpio;

    /*
     * Clocks
     */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOB |
        RCC_APB2Periph_AFIO,
        ENABLE
    );

    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_SPI2,
        ENABLE
    );


    /*
     * I2S2 pins:
     *
     * PB12 = WS
     * PB13 = CK
     * PB15 = SD (input)
     *
     * TLV320 is master, STM32 is slave receiver
     */

    gpio.GPIO_Pin =
        GPIO_Pin_12 |
        GPIO_Pin_13 |
        GPIO_Pin_15;

    gpio.GPIO_Mode =
        GPIO_Mode_IN_FLOATING;

    gpio.GPIO_Speed =
        GPIO_Speed_50MHz;

    GPIO_Init(GPIOB, &gpio);


    /*
     * Reset SPI2
     */
    SPI_I2S_DeInit(SPI2);


    /*
     * I2S slave receive
     */
    I2S_InitTypeDef i2s;

    i2s.I2S_Mode =
        I2S_Mode_SlaveRx;

    i2s.I2S_Standard =
        I2S_Standard_Phillips;

    i2s.I2S_DataFormat =
        I2S_DataFormat_16b;

    i2s.I2S_MCLKOutput =
        I2S_MCLKOutput_Disable;

    i2s.I2S_AudioFreq =
        I2S_AudioFreq_16k;

    i2s.I2S_CPOL =
        I2S_CPOL_Low;


    I2S_Init(
        SPI2,
        &i2s
    );


    I2S_Cmd(
        SPI2,
        ENABLE
    );
}
