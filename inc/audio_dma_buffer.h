#ifndef AUDIO_DMA_BUFFER_H
#define AUDIO_DMA_BUFFER_H
#include <stdint.h>
/* Snapshot a completed half only while DMA remains in the opposite half. */
int audio_dma_snapshot(uint16_t *out, const volatile uint16_t *buffer,
                       unsigned half_slots, unsigned first,
                       uint32_t (*remaining)(void));
#endif
