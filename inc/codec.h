#ifndef CODEC_H
#define CODEC_H

#include <stdint.h>

/* TLV320ADC3101 / TLV320ADC3101-Q1 7-bit I2C address. */
#define TLV320ADC3101_ADDR 0x18u

/* Hardware reset on STM32 PB14, active low. */
void codec_reset(void);

/* Apply/verify the PA8 12-MHz clock and isolated analog-input profile. */
int codec_profile_ready(void);
int codec_apply_audio_profile(void);

/* Capture must be stopped. Tests three clock paths, then restores normal profile. */
void codec_clock_test(void);

/* Read and print the key Page-0 registers used by the captured profile. */
void codec_dump_profile(void);

#endif
