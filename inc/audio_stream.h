#ifndef AUDIO_STREAM_H
#define AUDIO_STREAM_H

#include <stdint.h>

int audio_stream_start(void);
void audio_stream_stop(void);
void audio_stream_task(void);
/* Main-context only: bounded DMA service, no ESP control parsing or restarts. */
void audio_stream_service_dma(void);
int audio_stream_running(void);
uint32_t audio_stream_packets(void);
uint32_t audio_stream_dma_errors(void);
uint32_t audio_stream_overruns(void);
uint32_t audio_stream_stalls(void);

#endif
