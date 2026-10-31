#ifndef AUDIO_FREQUENCY_H
#define AUDIO_FREQUENCY_H
#include <stdint.h>
/* PB12 rising-edge EXTI count over a TIM2 50 ms gate. Capture must be stopped. */
int audio_frequency_measure(uint32_t *hz, uint32_t *edges, uint32_t *gate_us);
/* Only with codec RESET held and PA8 TIM1 temporarily at 12 kHz. */
int audio_frequency_measure_pa8(uint32_t *hz, uint32_t *edges, uint32_t *gate_us);
/* PB15 EXTI15 / 20 ms gate, for MCLK/128 routed onto codec DOUT. */
int audio_frequency_measure_dout(uint32_t *hz, uint32_t *edges, uint32_t *gate_us);
#endif
