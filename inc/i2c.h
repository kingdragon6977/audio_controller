#ifndef I2C_H
#define I2C_H

#include <stdint.h>

void i2c2_init(void);

void i2c2_write(
    uint8_t reg,
    uint8_t data
);

uint8_t i2c2_read(
    uint8_t reg
);

#endif
