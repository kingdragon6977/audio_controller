#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

#define AUDIO_SAMPLES 512

extern volatile uint16_t audio_buffer[AUDIO_SAMPLES];

extern volatile uint32_t audio_blocks;

#endif
