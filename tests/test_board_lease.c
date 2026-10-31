#include <assert.h>
#include <stdio.h>
#include "stm32f10x.h"
#include "board.h"
GPIO_TypeDef mock_gpioa, mock_gpiob;
uint32_t SystemCoreClock = 72000000u;
uint32_t systick_reload;
void SysTick_Handler(void);
int main(void)
{
    unsigned i;
    board_init();
    assert(systick_reload == 72000u);
    assert(!board_boot_auth_is_on());
    board_boot_auth_lease(2000);
    assert(board_boot_auth_is_on());
    for (i=0; i<1999; ++i) SysTick_Handler();
    assert(board_boot_auth_is_on());
    SysTick_Handler();
    assert(!board_boot_auth_is_on()); /* no main-loop calls needed */
    assert(mock_gpioa.ODR & GPIO_Pin_0);
    assert(board_millis() == 2000u);
    board_boot_auth_lease(2);
    board_boot_auth_off();
    for (i=0; i<3; ++i) SysTick_Handler();
    assert(!board_boot_auth_is_on());
    puts("board authorization lease tests PASS");
}
