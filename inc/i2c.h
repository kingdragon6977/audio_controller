#ifndef I2C_H
#define I2C_H

#include <stdint.h>

void i2c1_init(void);

int i2c1_probe(uint8_t address);

int i2c1_write(
    uint8_t address,
    uint8_t reg,
    uint8_t data);

int i2c1_read(
    uint8_t address,
    uint8_t reg,
    uint8_t *data);

#endif /* I2C_H */
