#include "stm32f10x.h"
#include "audio_frequency.h"

#define WCLK_LINE (1u << 12)
#define SHARED_LINES (0xFC00u)
static volatile uint32_t rising_edges;
static volatile uint32_t saturated;

void EXTI15_10_IRQHandler(void)
{
    if (EXTI->PR & WCLK_LINE) {
        EXTI->PR = WCLK_LINE; /* W1C before counting; keep handler short. */
        if (++rising_edges >= 3000u) {
            saturated = 1u;
            EXTI->IMR &= ~WCLK_LINE; /* Bound CPU cost for an excessive input rate. */
        }
    }
}

static uint32_t interrupt_mask(void)
{
#if defined(__arm__)
    uint32_t mask;
    __asm volatile ("mrs %0, primask" : "=r" (mask));
    return mask;
#else
    return __get_PRIMASK();
#endif
}

int audio_frequency_measure(uint32_t *hz, uint32_t *edges, uint32_t *gate_us)
{
    RCC_ClocksTypeDef clocks;
    uint32_t route, rising, falling, afio_clock, irq_priority;
    uint32_t elapsed = 0u, budget = 4000000u, count;
    unsigned irq = (unsigned)EXTI15_10_IRQn;
    *hz = *edges = *gate_us = 0u;
    RCC_GetClocksFreq(&clocks);
    /* Own only idle TIM2 and the unused shared EXTI vector. Never drive PB12. */
    if (interrupt_mask() || clocks.HCLK_Frequency != 72000000u ||
        clocks.PCLK1_Frequency != 36000000u ||
        (RCC->APB1ENR & RCC_APB1Periph_TIM2) ||
        ((EXTI->IMR | EXTI->EMR | EXTI->PR) & SHARED_LINES) ||
        (NVIC->ISER[irq / 32u] & (1u << (irq % 32u))) ||
        NVIC_GetPendingIRQ(EXTI15_10_IRQn) ||
        (GPIOB->CRH & (3u << 16)) ||
        (SPI2->I2SCFGR & SPI_I2SCFGR_I2SE) ||
        (DMA1_Channel4->CCR & 1u)) return 0;
    afio_clock = RCC->APB2ENR & RCC_APB2Periph_AFIO;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    route = AFIO->EXTICR[3]; rising = EXTI->RTSR; falling = EXTI->FTSR;
    irq_priority = NVIC_GetPriority(EXTI15_10_IRQn);
    AFIO->EXTICR[3] = (route & ~15u) | 1u; /* EXTI12 <- PB12 */
    EXTI->RTSR |= WCLK_LINE;
    EXTI->FTSR &= ~WCLK_LINE;
    EXTI->PR = WCLK_LINE;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM2, DISABLE);
    TIM2->PSC = 71u; /* APB1 timer clock = 2 * 36 MHz; 1 us ticks. */
    TIM2->ARR = 65535u;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0u;
    TIM2->CNT = 0u;
    rising_edges = 0u; saturated = 0u;
    NVIC_SetPriority(EXTI15_10_IRQn, 15u); /* minimal handler; lease tick stays enabled */
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    TIM2->CR1 = TIM_CR1_CEN;
    EXTI->IMR |= WCLK_LINE;
    NVIC_EnableIRQ(EXTI15_10_IRQn);
    do {
        elapsed = TIM2->CNT;
    } while (elapsed < 50000u && --budget);
    EXTI->IMR &= ~WCLK_LINE;
    NVIC_DisableIRQ(EXTI15_10_IRQn);
    count = rising_edges;
    /* Stop accounting before cleanup; reject late/overflowed/stopped gates. */
    elapsed = TIM2->CNT;
    TIM2->CR1 = 0u;
    *edges = count; *gate_us = elapsed;
    if (!saturated && budget && elapsed >= 50000u && elapsed <= 50100u && !(TIM2->SR & 1u))
        *hz = (uint32_t)(((uint64_t)count * 1000000u) / elapsed);
    EXTI->PR = WCLK_LINE;
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    NVIC_SetPriority(EXTI15_10_IRQn, irq_priority);
    EXTI->RTSR = rising; EXTI->FTSR = falling; AFIO->EXTICR[3] = route;
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM2, DISABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE);
    if (!afio_clock) RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, DISABLE);
    return !saturated && budget && elapsed >= 50000u && elapsed <= 50100u && *hz != 0u;
}
