#ifndef CODEC_H
#define CODEC_H

#include <stdint.h>

/* TLV320ADC3101 / TLV320ADC3101-Q1 7-bit I2C address. */
#define TLV320ADC3101_ADDR 0x18u

/* Hardware reset on STM32 PB14, active low. */
void codec_reset(void);

/* Apply the five Page-0 writes captured from the AV6301. */
int codec_apply_av6301_profile(void);

/* Read and print the key Page-0 registers used by the captured profile. */
void codec_dump_profile(void);

#endif
