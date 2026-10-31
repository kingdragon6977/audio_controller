#include "stm32f10x.h"
#include "audio_clock.h"
#include "board.h"
#include "uart.h"
#include <stdio.h>

static uint32_t observed_wclk;
static int last_rate_result = -1;
void audio_clock_invalidate_rate(void) { observed_wclk = 0u; }
int audio_clock_rate_valid(void) { return observed_wclk >= 47520u && observed_wclk <= 48480u && audio_clock_ready(); }

static int source_ready(void)
{
    RCC_ClocksTypeDef c;
    RCC_GetClocksFreq(&c);
    return c.SYSCLK_Frequency == 72000000u && c.HCLK_Frequency == 72000000u &&
           c.PCLK2_Frequency == 72000000u &&
           (RCC->CR & (RCC_CR_HSERDY | RCC_CR_PLLRDY)) == (RCC_CR_HSERDY | RCC_CR_PLLRDY) &&
           (RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL &&
           (RCC->CFGR & RCC_CFGR_PLLSRC) != 0u &&
           (RCC->CFGR & (RCC_CFGR_MCO | RCC_CFGR_PPRE2)) == 0u;
}

int audio_clock_ready(void)
{
    return source_ready() &&
           (RCC->APB2ENR & (RCC_APB2Periph_GPIOA | RCC_APB2Periph_TIM1)) ==
               (RCC_APB2Periph_GPIOA | RCC_APB2Periph_TIM1) &&
           (AFIO->MAPR & GPIO_FullRemap_TIM1) == 0u &&
           (GPIOA->CRH & 0x0fu) == 0x0bu &&
           TIM1->CR1 == (TIM_CR1_ARPE | TIM_CR1_CEN) &&
           TIM1->CR2 == 0u && TIM1->SMCR == 0u && TIM1->DIER == 0u &&
           TIM1->PSC == 0u && TIM1->ARR == 5u && TIM1->CCR1 == 3u &&
           TIM1->RCR == 0u && TIM1->CCMR1 == 0x0068u &&
           TIM1->CCMR2 == 0u && TIM1->CCER == TIM_CCER_CC1E &&
           TIM1->BDTR == TIM_BDTR_MOE;
}

int audio_clock_init(void)
{
    GPIO_InitTypeDef gpio;
    audio_clock_invalidate_rate();
    /* Fail before claiming PA8 if RCC or pin routing differs from this board. */
    if (!source_ready() || (AFIO->MAPR & GPIO_FullRemap_TIM1) != 0u)
        return 0;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO | RCC_APB2Periph_TIM1, ENABLE);
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_TIM1, ENABLE);
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_TIM1, DISABLE);
    TIM1->CR1 = TIM_CR1_ARPE;
    TIM1->PSC = 0u;
    TIM1->ARR = 5u;
    TIM1->CCR1 = 3u;
    TIM1->CCMR1 = 0x0068u; /* CH1 PWM1, preload, output; channels 2..4 disabled. */
    TIM1->CCER = TIM_CCER_CC1E;
    TIM1->BDTR = TIM_BDTR_MOE;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR = 0u;
    TIM1->CR1 |= TIM_CR1_CEN;
    gpio.GPIO_Pin = GPIO_Pin_8;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    if (audio_clock_ready()) return 1;
    TIM1->BDTR = 0u;
    TIM1->CR1 = 0u;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);
    return 0;
}

void audio_clock_print(void)
{
    char buf[200];
    uart2_print("\r\nCODEC MCLK: PA8 TIM1_CH1 -> 40 ohm -> TLV pin 24; IOVDD=3.3 V\r\n");
    sprintf(buf, " TIM1 PSC=%lu ARR=%lu CCR1=%lu CR1=%04lX CCER=%04lX BDTR=%04lX GPIOA_CRH=%08lX\r\n",
            (unsigned long)TIM1->PSC, (unsigned long)TIM1->ARR, (unsigned long)TIM1->CCR1,
            (unsigned long)TIM1->CR1, (unsigned long)TIM1->CCER,
            (unsigned long)TIM1->BDTR, (unsigned long)GPIOA->CRH);
    uart2_print(buf);
    uart2_print(audio_clock_ready()
        ? " MCLK REGISTER CHECK: PASS - calculated 12000000 Hz, 50% duty; no timer IRQ/DMA.\r\n"
        : " MCLK REGISTER CHECK: FAIL - codec/capture activation blocked.\r\n");
    uart2_print(" Scope at TLV MCLK remains authoritative for frequency, voltage and ringing.\r\n");
}

uint32_t audio_clock_wclk_hz(void) { return observed_wclk; }

int audio_clock_check_wclk(int verbose)
{
    uint32_t start, elapsed = 0u, edges = 0u, budget = 2000000u;
    unsigned int previous, current;
    int ok = 0;
    observed_wclk = 0u;
    if (!audio_clock_ready() || !(SysTick->CTRL & SysTick_CTRL_ENABLE_Msk)) goto report;
    /* Align to a tick so the first partial millisecond cannot bias a short window. */
    start = board_millis();
    while (board_millis() == start && budget) --budget;
    if (!budget) goto report;
    start = board_millis();
    previous = (GPIOB->IDR & GPIO_Pin_12) != 0u;
    budget = 2000000u;
    do {
        current = (GPIOB->IDR & GPIO_Pin_12) != 0u;
        if (current && !previous) ++edges;
        previous = current;
        elapsed = board_millis() - start;
    } while (elapsed < 20u && --budget);
    if (!budget || !elapsed) goto report;
    observed_wclk = edges * 1000u / elapsed;
    ok = observed_wclk >= 47520u && observed_wclk <= 48480u; /* +/-1% estimate gate */
report:
    if (verbose || ok != last_rate_result) {
        char buf[160];
        sprintf(buf, "I2S RATE PREFLIGHT: %s observed_hz=%lu expected_hz=48000 window_ms=%lu (GPIO estimate)\r\n",
                ok ? "PASS" : "FAIL", (unsigned long)observed_wclk, (unsigned long)elapsed);
        uart2_print(buf);
    }
    last_rate_result = ok;
    return ok;
}
