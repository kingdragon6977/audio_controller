#include "audio.h"

volatile uint16_t audio_buffer[AUDIO_SAMPLES];

volatile uint32_t audio_blocks = 0;
