#include "stm32f10x.h"
#include "board.h"

static uint8_t led_state = 0;
static volatile uint32_t uptime_ms;
static volatile uint32_t auth_remaining_ms;

uint32_t board_millis(void) { return uptime_ms; }

void SysTick_Handler(void)
{
    ++uptime_ms;
    if (auth_remaining_ms && --auth_remaining_ms == 0u)
        GPIO_SetBits(GPIOA, GPIO_Pin_0);
}

void board_boot_auth_lease(unsigned int milliseconds)
{
    /* Install expiry before driving the authorization pin. */
    auth_remaining_ms = milliseconds;
    if (milliseconds) board_boot_auth_on();
    else board_boot_auth_off();
}

void board_init(void)
{
    GPIO_InitTypeDef gpio;

    /* GPIO clocks: PB2 LED/BOOT1 and PA0 BOOT_AUTH. */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB,
        ENABLE);

    /*
     * PA0 drives the gate of the first high-side 4407 authorization FET.
     * HIGH (or reset/Hi-Z with the external 47k pull-up) = authorization OFF.
     * Set the output latch HIGH before enabling output mode so initialization
     * cannot create a momentary active-low authorization pulse.
     */
    GPIO_SetBits(GPIOA, GPIO_Pin_0);
    gpio.GPIO_Pin   = GPIO_Pin_0;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio);
    /* Hold codec reset while its reference clock is established. */
    GPIO_ResetBits(GPIOB, GPIO_Pin_14);
    gpio.GPIO_Pin = GPIO_Pin_14;
    GPIO_Init(GPIOB, &gpio);
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000u);

    /* PB2 LED - active high: PB2 HIGH = LED ON */
    gpio.GPIO_Pin   = GPIO_Pin_2;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;

    GPIO_Init(GPIOB, &gpio);

    /* Simple power indicator for bring-up: stay on while firmware is running. */
    GPIO_SetBits(GPIOB, GPIO_Pin_2);
    led_state = 1;
}

void led_on(void)
{
    GPIO_SetBits(GPIOB, GPIO_Pin_2);
    led_state = 1;
}

void led_off(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_2);
    led_state = 0;
}

void led_toggle(void)
{
    if (led_state)
        led_off();
    else
        led_on();
}

int board_boot1_hold_low(void)
{
    led_off();

    return ((GPIOB->ODR & GPIO_Pin_2) == 0u &&
            (GPIOB->IDR & GPIO_Pin_2) == 0u) ? 1 : 0;
}


void board_boot_auth_off(void)
{
    auth_remaining_ms = 0u;
    /* Active-low authorization: HIGH = first 4407 OFF. */
    GPIO_SetBits(GPIOA, GPIO_Pin_0);
}

void board_boot_auth_on(void)
{
    /* Active-low authorization: LOW = first 4407 ON. */
    GPIO_ResetBits(GPIOA, GPIO_Pin_0);
}

int board_boot_auth_is_on(void)
{
    return ((GPIOA->ODR & GPIO_Pin_0) == 0u &&
            (GPIOA->IDR & GPIO_Pin_0) == 0u) ? 1 : 0;
}
