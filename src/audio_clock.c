#include "stm32f10x.h"
#include "audio_clock.h"
#include "board.h"
#include "audio_frequency.h"
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

int audio_clock_test_output(void)
{
    uint32_t hz = 0u, edges = 0u, gate = 0u;
    int measured, restored;
    char buf[190];
    audio_clock_invalidate_rate();
    if (!audio_clock_ready() || ((GPIOB->ODR | GPIOB->IDR) & GPIO_Pin_14) ||
        (SPI2->I2SCFGR & SPI_I2SCFGR_I2SE) || (DMA1_Channel4->CCR & 1u)) {
        uart2_print("PA8 OUTPUT TEST: BLOCKED - require valid clock, codec RESET LOW and stopped capture.\r\n");
        return 0;
    }
    TIM1->CCER = 0u;
    TIM1->CR1 = TIM_CR1_ARPE;
    TIM1->ARR = 5999u;
    TIM1->CCR1 = 3000u;
    TIM1->CNT = 0u;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR = 0u;
    TIM1->CR1 |= TIM_CR1_CEN;
    TIM1->CCER = TIM_CCER_CC1E;
    measured = audio_frequency_measure_pa8(&hz, &edges, &gate);
    restored = audio_clock_init();
    sprintf(buf, "PA8 OUTPUT TEST: expected_hz=12000 observed_hz=%lu edges=%lu gate_us=%lu measurement=%s rate=%s restore_12MHz=%s\r\n",
            (unsigned long)hz, (unsigned long)edges, (unsigned long)gate,
            measured ? "VALID" : "INVALID",
            measured && hz >= 11880u && hz <= 12120u ? "PASS" : "FAIL",
            restored ? "PASS" : "FAIL");
    uart2_print(buf);
    uart2_print("PA8 pad readback at 12 kHz; TLV-side 12-MHz waveform is not verified.\r\n");
    return measured && hz >= 11880u && hz <= 12120u && restored;
}

uint32_t audio_clock_wclk_hz(void) { return observed_wclk; }

int audio_clock_check_wclk(int verbose)
{
    uint32_t edges = 0u, gate_us = 0u;
    int measured = 0, ok = 0;
    observed_wclk = 0u;
    if (audio_clock_ready() && (SysTick->CTRL & SysTick_CTRL_ENABLE_Msk))
        measured = audio_frequency_measure(&observed_wclk, &edges, &gate_us);
    ok = measured && observed_wclk >= 47520u && observed_wclk <= 48480u;
    if (verbose || ok != last_rate_result) {
        char buf[190];
        sprintf(buf, "I2S RATE PREFLIGHT: %s observed_hz=%lu expected_hz=48000 edges=%lu gate_us=%lu method=EXTI12_TIM2 measurement=%s\r\n",
                ok ? "PASS" : "FAIL", (unsigned long)observed_wclk,
                (unsigned long)edges, (unsigned long)gate_us, measured ? "VALID" : "INVALID");
        uart2_print(buf);
    }
    last_rate_result = ok;
    return ok;
}
