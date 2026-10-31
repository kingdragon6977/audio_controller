#include "audio_dma_buffer.h"
static int safe(unsigned first, unsigned half, uint32_t remaining)
{
    int first_active;
    if (remaining > half * 2u) return 0;
    first_active = remaining > half || remaining == 0u;
    return (first == 0u) != first_active;
}
int audio_dma_snapshot(uint16_t *out, const volatile uint16_t *buffer,
                       unsigned half, unsigned first, uint32_t (*remaining)(void))
{
    unsigned i;
    if (!out || !buffer || !remaining || !half || (first != 0u && first != half)) return 0;
    if (!safe(first, half, remaining())) return 0;
    for (i=0; i<half; ++i) out[i] = buffer[first+i];
    return safe(first, half, remaining());
}
