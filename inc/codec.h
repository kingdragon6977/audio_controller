#ifndef CODEC_H
#define CODEC_H

#include <stdint.h>

/* TLV320ADC3101 / TLV320ADC3101-Q1 7-bit I2C address. */
#define TLV320ADC3101_ADDR 0x18u

/*
 * Apply the five Page-0 writes captured from the AV6301.
 * This is intentionally NOT called automatically during boot: the AV6301
 * must be isolated from the shared I2C bus before the STM32 takes control.
 */
int codec_apply_av6301_profile(void);

/* Read and print the key Page-0 registers used by the captured profile. */
void codec_dump_profile(void);

#endif
