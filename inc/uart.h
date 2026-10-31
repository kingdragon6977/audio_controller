#ifndef UART_H
#define UART_H

#include <stdint.h>

/* USART2 - primary/debug CLI on PA2/PA3 */
void uart2_init(void);
int uart2_available(void);
char uart2_getc(void);
void uart2_putc(char c);
void uart2_print(const char *s);

typedef struct {
    uint32_t at_ms, since_previous_ms, sr, phase;
    uint8_t byte;
} esp_uart_rx_event_t;
/* Startup checkpoints observe SR without reading/clearing DR. */
void esp_uart_mark_phase(uint32_t phase);
void esp_uart_poll_startup(void);
int esp_uart_first_rx_error(esp_uart_rx_event_t *out);
void esp_uart_print_rx_trace(void);

/* USART1 - ESP-01 high-speed transport on PA9/PA10 */
void esp_uart_init(void);
int esp_uart_available(void);
char esp_uart_getc(void);
void esp_uart_putc(char c);
void esp_uart_write(const uint8_t *data, uint32_t length);
void esp_uart_print(const char *s);
uint32_t esp_uart_rx_bytes(void);
uint32_t esp_uart_overrun_errors(void);
uint32_t esp_uart_framing_errors(void);
uint32_t esp_uart_noise_errors(void);

#endif
