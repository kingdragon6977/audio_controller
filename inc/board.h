#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>

uint32_t board_millis(void);
void board_boot_auth_lease(unsigned int milliseconds);

void board_init(void);

void led_on(void);
void led_off(void);
void led_toggle(void);

/* Drive the shared PB2 LED/BOOT1 pin low and verify output + pin readback. */
int board_boot1_hold_low(void);

/* PA0 -> first 4407 gate. Active-low BOOT_AUTH; reset/default is OFF. */
void board_boot_auth_off(void);
void board_boot_auth_on(void);
int board_boot_auth_is_on(void);

#endif
