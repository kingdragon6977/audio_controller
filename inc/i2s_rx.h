#ifndef I2S_RX_H
#define I2S_RX_H

#include <stdint.h>

#define I2S_RX_SAMPLES 256u

typedef struct
{
    uint32_t sr_before;
    uint32_t sr_after_enable;
    uint32_t sr_after_capture;
    uint32_t dr_after_capture;
    uint32_t dma_isr_after_capture;
    uint16_t dma_cndtr_after_capture;
    uint32_t ws_sync_ok;
    uint32_t ws_level_at_enable;
} i2s_rx_debug_t;

int i2s_rx_start_capture(void);
void i2s_rx_stop(void);
int i2s_rx_capture_complete(void);
uint32_t i2s_rx_error_flags(void);
const uint16_t *i2s_rx_buffer(void);
void i2s_rx_get_debug(i2s_rx_debug_t *debug);
void i2s_rx_print_analysis(void);

#endif
