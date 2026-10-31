#include "stm32f10x.h"
#include "audio_frequency.h"

static uint32_t active_line;
static volatile uint32_t rising_edges;
static volatile uint32_t saturated;

static void count_edge(void)
{
    if (EXTI->PR & active_line) {
        EXTI->PR = active_line; /* W1C before counting; keep handler short. */
        if (++rising_edges >= 3000u) {
            saturated = 1u;
            EXTI->IMR &= ~active_line; /* Bound CPU cost for an excessive input rate. */
        }
    }
}

void EXTI15_10_IRQHandler(void) { count_edge(); }
void EXTI9_5_IRQHandler(void) { count_edge(); }

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

static int measure(unsigned signal, uint32_t *hz, uint32_t *edges, uint32_t *gate_us)
{
    int pa8 = signal == 1u, dout = signal == 2u;
    RCC_ClocksTypeDef clocks;
    uint32_t route, rising, falling, afio_clock, irq_priority;
    uint32_t elapsed = 0u, budget = 4000000u, count;
    IRQn_Type vector = pa8 ? EXTI9_5_IRQn : EXTI15_10_IRQn;
    unsigned irq = (unsigned)vector, route_index = pa8 ? 2u : 3u;
    uint32_t line = pa8 ? (1u << 8) : dout ? (1u << 15) : (1u << 12);
    uint32_t window = dout ? 20000u : 50000u, shift = dout ? 12u : 0u;
    uint32_t shared = pa8 ? 0x03e0u : 0xfc00u;
    *hz = *edges = *gate_us = 0u;
    RCC_GetClocksFreq(&clocks);
    /* Own only idle TIM2 and the unused shared EXTI vector. Never drive PB12. */
    if (interrupt_mask() || clocks.HCLK_Frequency != 72000000u ||
        clocks.PCLK1_Frequency != 36000000u ||
        (RCC->APB1ENR & RCC_APB1Periph_TIM2) ||
        ((EXTI->IMR | EXTI->EMR | EXTI->PR) & shared) ||
        (NVIC->ISER[irq / 32u] & (1u << (irq % 32u))) ||
        NVIC_GetPendingIRQ(vector) ||
        (!pa8 && (GPIOB->CRH & (3u << (dout ? 28u : 16u)))) ||
        (pa8 && ((GPIOA->CRH & 15u) != 11u ||
                 ((GPIOB->ODR | GPIOB->IDR) & GPIO_Pin_14) ||
                 TIM1->PSC != 0u || TIM1->ARR != 5999u || TIM1->CCR1 != 3000u ||
                 !(TIM1->CR1 & TIM_CR1_CEN) || TIM1->CCER != TIM_CCER_CC1E ||
                 TIM1->BDTR != TIM_BDTR_MOE)) ||
        (SPI2->I2SCFGR & SPI_I2SCFGR_I2SE) ||
        (DMA1_Channel4->CCR & 1u)) return 0;
    active_line = line;
    afio_clock = RCC->APB2ENR & RCC_APB2Periph_AFIO;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    route = AFIO->EXTICR[route_index]; rising = EXTI->RTSR; falling = EXTI->FTSR;
    irq_priority = NVIC_GetPriority(vector);
    AFIO->EXTICR[route_index] = (route & ~(15u << shift)) | ((pa8 ? 0u : 1u) << shift);
    EXTI->RTSR |= active_line;
    EXTI->FTSR &= ~active_line;
    EXTI->PR = active_line;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM2, DISABLE);
    TIM2->PSC = 71u; /* APB1 timer clock = 2 * 36 MHz; 1 us ticks. */
    TIM2->ARR = 65535u;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0u;
    TIM2->CNT = 0u;
    rising_edges = 0u; saturated = 0u;
    NVIC_SetPriority(vector, 15u); /* minimal handler; lease tick stays enabled */
    NVIC_ClearPendingIRQ(vector);
    TIM2->CR1 = TIM_CR1_CEN;
    EXTI->IMR |= active_line;
    NVIC_EnableIRQ(vector);
    do {
        elapsed = TIM2->CNT;
    } while (elapsed < window && --budget);
    EXTI->IMR &= ~active_line;
    NVIC_DisableIRQ(vector);
    count = rising_edges;
    /* Stop accounting before cleanup; reject late/overflowed/stopped gates. */
    elapsed = TIM2->CNT;
    TIM2->CR1 = 0u;
    *edges = count; *gate_us = elapsed;
    if (!saturated && budget && elapsed >= window && elapsed <= window + 100u && !(TIM2->SR & 1u))
        *hz = (uint32_t)(((uint64_t)count * 1000000u) / elapsed);
    EXTI->PR = active_line;
    NVIC_ClearPendingIRQ(vector);
    NVIC_SetPriority(vector, irq_priority);
    EXTI->RTSR = rising; EXTI->FTSR = falling; AFIO->EXTICR[route_index] = route;
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM2, DISABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE);
    if (!afio_clock) RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, DISABLE);
    return !saturated && budget && elapsed >= window && elapsed <= window + 100u && *hz != 0u;
}

int audio_frequency_measure(uint32_t *hz, uint32_t *edges, uint32_t *gate_us)
{
    return measure(0, hz, edges, gate_us);
}
int audio_frequency_measure_pa8(uint32_t *hz, uint32_t *edges, uint32_t *gate_us)
{
    return measure(1, hz, edges, gate_us);
}

int audio_frequency_measure_dout(uint32_t *hz, uint32_t *edges, uint32_t *gate_us)
{
    return measure(2, hz, edges, gate_us);
}
