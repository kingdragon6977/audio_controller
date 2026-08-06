#ifndef CLI_H
#define CLI_H

void cli_init(void);
void cli_task(void);
void uart4_init(void);

void uart4_putc(char c);

void uart4_print(const char *);

char uart4_getc(void);

int uart4_available(void);

#endif
