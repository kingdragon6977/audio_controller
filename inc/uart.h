#ifndef UART_H
#define UART_H

void uart4_init(void);

void uart4_putc(char c);

void uart4_print(const char *s);

char uart4_getc(void);

int uart4_available(void);

#endif
