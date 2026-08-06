
#include <string.h>

#include "monitor.h"
#include "uart.h"
#include "stm32f10x.h"

static void print_help(void)
{
    uart4_print("\r\nCommands:\r\n");
    uart4_print(" help\r\n");
    uart4_print(" id\r\n");
    uart4_print(" uid\r\n");
    uart4_print(" regs\r\n");
    uart4_print(" peek <addr>\r\n");
    uart4_print(" led on\r\n");
    uart4_print(" led off\r\n");
}

void monitor_execute(char *cmd)
{
    if(strcmp(cmd,"help")==0)
    {
        print_help();
        return;
    }

    if(strcmp(cmd,"id")==0)
    {
        uart4_print("DBGMCU_IDCODE = 0x");
        uart4_print_hex(DBGMCU->IDCODE);
        uart4_print("\r\n");
        return;
    }

    if(strcmp(cmd,"uid")==0)
    {
        uint32_t *uid=(uint32_t *)0x1FFFF7E8;

        uart4_print("UID:\r\n");

        uart4_print_hex(uid[0]);
        uart4_print("\r\n");

        uart4_print_hex(uid[1]);
        uart4_print("\r\n");

        uart4_print_hex(uid[2]);
        uart4_print("\r\n");

        return;
    }

    if(strcmp(cmd,"regs")==0)
    {
        uart4_print("RCC->CR  = ");
        uart4_print_hex(RCC->CR);
        uart4_print("\r\n");

        uart4_print("RCC->CFGR= ");
        uart4_print_hex(RCC->CFGR);
        uart4_print("\r\n");

        return;
    }



    if(strcmp(cmd,"led on")==0)
    {
        GPIOA->BSRR=GPIO_Pin_8;
        return;
    }

    if(strcmp(cmd,"led off")==0)
    {
        GPIOA->BRR=GPIO_Pin_8;
        return;
    }

    uart4_print("Unknown command\r\n");
}
