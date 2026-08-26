#ifndef I2S_RX_H
#define I2S_RX_H

#include <stdint.h>

#define I2S_RX_SAMPLES 256u

int i2s_rx_start_capture(void);
void i2s_rx_stop(void);
int i2s_rx_capture_complete(void);
uint32_t i2s_rx_error_flags(void);
const uint16_t *i2s_rx_buffer(void);

#endif
