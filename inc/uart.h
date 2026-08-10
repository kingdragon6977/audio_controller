#ifndef UART_H
#define UART_H

/* USART2 - primary/debug CLI on PA2/PA3 */
void uart2_init(void);
int uart2_available(void);
char uart2_getc(void);
void uart2_putc(char c);
void uart2_print(const char *s);

/* USART1 - ESP-01 interface on PA9/PA10 */
void esp_uart_init(void);
int esp_uart_available(void);
char esp_uart_getc(void);
void esp_uart_putc(char c);
void esp_uart_print(const char *s);

#endif
