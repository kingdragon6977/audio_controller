#ifndef AUDIO_CLOCK_H
#define AUDIO_CLOCK_H
#include <stdint.h>
#define AUDIO_MCLK_HZ 12000000u
#define AUDIO_WCLK_HZ 48000u
#define AUDIO_PCM_HZ 24000u
int audio_clock_init(void);
int audio_clock_ready(void);
void audio_clock_print(void);
/* Called only while capture/stream DMA is stopped. EXTI/TIM2 count, not waveform proof. */
int audio_clock_check_wclk(int verbose);
uint32_t audio_clock_wclk_hz(void);
void audio_clock_invalidate_rate(void);
int audio_clock_rate_valid(void);
#endif
