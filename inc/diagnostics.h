#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdint.h>

void diagnostics_print_mcu(void);
void diagnostics_print_clock(void);
void diagnostics_print_i2c1(void);
void diagnostics_print_i2s2(void);
void diagnostics_print_i2s_clock_ownership(void);
void diagnostics_print_audio_pins(void);
int diagnostics_i2c1_safe(void);
int diagnostics_i2s2_safe(void);
int diagnostics_i2s_clock_pins_active(void);

#endif
