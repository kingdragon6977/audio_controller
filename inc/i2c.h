#ifndef I2C_H
#define I2C_H

#include <stdint.h>

#define TLV320ADC3101_I2C_ADDR 0x18u

void i2c2_init(void);

int i2c2_probe(uint8_t address);
int i2c2_write(uint8_t address, uint8_t reg, uint8_t data);
int i2c2_read(uint8_t address, uint8_t reg, uint8_t *data);

#endif
