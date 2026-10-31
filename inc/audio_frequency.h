#ifndef AUDIO_FREQUENCY_H
#define AUDIO_FREQUENCY_H
#include <stdint.h>
/* PB12 rising-edge EXTI count over a TIM2 50 ms gate. Capture must be stopped. */
int audio_frequency_measure(uint32_t *hz, uint32_t *edges, uint32_t *gate_us);
#endif
