#include "stm32f10x.h"
#include "uart.h"
#include "audio_stream.h"
#include "board.h"
#include <stdio.h>

static uint32_t esp_rx_bytes;
static uint32_t esp_ore_errors;
static uint32_t esp_fe_errors;
static uint32_t esp_ne_errors;

static uint32_t rx_phase, last_rx_ms, trace_count, checkpoint_count;
static esp_uart_rx_event_t first_error, rx_trace[8], checkpoints[8];
static int have_first_error;
static uint8_t esp_initialized, startup_ready_pending;
static uint32_t startup_ready_bytes, startup_ignored_bytes;
static char esp_uart_read_hardware(void);

void esp_uart_poll_startup(void)
{
    uint32_t sr;
    uint8_t data;
    if (!esp_initialized || rx_phase >= 4u) return;
    sr = USART1->SR;
    if (!(sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE))) return;
    data = (uint8_t)esp_uart_read_hardware();
    /* Only latch idempotent READY. Never authorize flashing or parse frames
     * before initialization is complete; unexpected bytes remain traced. */
    if (sr & USART_SR_RXNE) {
        if (data == 0xf0u) {
            startup_ready_pending = 1u;
            ++startup_ready_bytes;
        } else ++startup_ignored_bytes;
    }
}

void esp_uart_mark_phase(uint32_t phase)
{
    esp_uart_poll_startup();
    rx_phase = phase;
    if (checkpoint_count < 8u) {
        esp_uart_rx_event_t *e = &checkpoints[checkpoint_count++];
        e->sr = USART1->SR; /* No DR read: preserve pending byte/error evidence. */
        e->at_ms = board_millis(); e->phase = phase;
    }
}

int esp_uart_first_rx_error(esp_uart_rx_event_t *out)
{
    if (!have_first_error) return 0;
    *out = first_error;
    return 1;
}

void esp_uart_print_rx_trace(void)
{
    uint32_t i, begin = trace_count > 8u ? trace_count - 8u : 0u;
    char buf[180];
    sprintf(buf," RX STARTUP ready_bytes=%lu ignored_bytes=%lu pending_ready=%u\r\n",
            (unsigned long)startup_ready_bytes,(unsigned long)startup_ignored_bytes,startup_ready_pending);
    uart2_print(buf);
    uart2_print("USART1 RX TRACE: phase 0=UART enabled, 1=before codec, 2=before rate, 3=after capture, 4=main loop.\r\n");
    for (i=0; i<checkpoint_count; ++i) {
        const esp_uart_rx_event_t *e = &checkpoints[i];
        sprintf(buf," RX CHECKPOINT phase=%lu t_ms=%lu SR=%08lX RXNE=%u ORE=%u\r\n",
                (unsigned long)e->phase,(unsigned long)e->at_ms,(unsigned long)e->sr,
                (e->sr & USART_SR_RXNE) ? 1u : 0u,(e->sr & USART_SR_ORE) ? 1u : 0u);
        uart2_print(buf);
    }
    if (have_first_error) {
        sprintf(buf," RX FIRST ERROR phase=%lu t_ms=%lu since_previous_ms=%lu SR=%08lX byte=%02X\r\n",
                (unsigned long)first_error.phase,(unsigned long)first_error.at_ms,
                (unsigned long)first_error.since_previous_ms,(unsigned long)first_error.sr,first_error.byte);
        uart2_print(buf);
    } else uart2_print(" RX FIRST ERROR: none\r\n");
    for (i=begin; i<trace_count; ++i) {
        const esp_uart_rx_event_t *e = &rx_trace[i % 8u];
        sprintf(buf," RX BYTE phase=%lu t_ms=%lu since_previous_ms=%lu SR=%08lX byte=%02X\r\n",
                (unsigned long)e->phase,(unsigned long)e->at_ms,(unsigned long)e->since_previous_ms,
                (unsigned long)e->sr,e->byte);
        uart2_print(buf);
    }
}

/*
 * USART2: primary/debug CLI UART
 * PA2 = TX, PA3 = RX
 * 115200 8N1
 */
void uart2_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef us;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_2;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_3;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    us.USART_BaudRate = 115200;
    us.USART_WordLength = USART_WordLength_8b;
    us.USART_StopBits = USART_StopBits_1;
    us.USART_Parity = USART_Parity_No;
    us.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    us.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART2, &us);
    USART_Cmd(USART2, ENABLE);
}

int uart2_available(void)
{
    return (USART2->SR & USART_SR_RXNE) != 0;
}

char uart2_getc(void)
{
    return (char)(USART2->DR & 0xFF);
}

void uart2_putc(char c)
{
    /* Debug output must not monopolize the foreground while capture runs.
     * The DMA-only service is reentrancy guarded and never parses ESP control. */
    esp_uart_poll_startup();
    audio_stream_service_dma();
    while (!(USART2->SR & USART_SR_TXE)) {
        esp_uart_poll_startup();
        audio_stream_service_dma();
    }

    USART2->DR = (uint16_t)c;
}

void uart2_print(const char *s)
{
    while (*s)
        uart2_putc(*s++);
}

/*
 * USART1: ESP-01 high-speed PCM transport
 * PA9  = TX -> ESP GPIO3/RX
 * PA10 = RX <- ESP GPIO1/TX
 * 1000000 8N1
 *
 * 24 kHz mono PCM16 is 48 kB/s. A 1 Mbaud 8N1 UART carries about
 * 100 kB/s, leaving comfortable framing/headroom while improving
 * signal margin versus the previous 2 Mbaud bring-up setting.
 */
void esp_uart_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef us;

    esp_rx_bytes = 0u;
    esp_ore_errors = 0u;
    esp_fe_errors = 0u;
    esp_ne_errors = 0u;
    rx_phase = last_rx_ms = trace_count = checkpoint_count = 0u;
    have_first_error = 0;
    esp_initialized = startup_ready_pending = 0u;
    startup_ready_bytes = startup_ignored_bytes = 0u;

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1,
        ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    us.USART_BaudRate = 1000000;
    us.USART_WordLength = USART_WordLength_8b;
    us.USART_StopBits = USART_StopBits_1;
    us.USART_Parity = USART_Parity_No;
    us.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    us.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART1, &us);
    USART_Cmd(USART1, ENABLE);
    esp_initialized = 1u;
    esp_uart_mark_phase(0u);
}

int esp_uart_available(void)
{
    return (rx_phase >= 4u && startup_ready_pending) || (USART1->SR & USART_SR_RXNE) != 0;
}

char esp_uart_getc(void)
{
    if (rx_phase >= 4u && startup_ready_pending) {
        startup_ready_pending = 0u;
        return (char)0xf0u; /* Already counted/traced when physically received. */
    }
    return esp_uart_read_hardware();
}

static char esp_uart_read_hardware(void)
{
    uint32_t sr = USART1->SR;
    uint8_t data = (uint8_t)(USART1->DR & 0xFFu);
    esp_uart_rx_event_t event;
    event.at_ms = board_millis();
    event.since_previous_ms = event.at_ms - last_rx_ms;
    event.phase = rx_phase; event.sr = sr; event.byte = data;
    last_rx_ms = event.at_ms;
    rx_trace[trace_count++ % 8u] = event;
    if (!have_first_error && (sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE))) {
        first_error = event;
        have_first_error = 1;
    }

    if (sr & USART_SR_ORE)
        esp_ore_errors++;
    if (sr & USART_SR_FE)
        esp_fe_errors++;
    if (sr & USART_SR_NE)
        esp_ne_errors++;
    if (sr & USART_SR_RXNE)
        esp_rx_bytes++;

    return (char)data;
}

void esp_uart_putc(char c)
{
    while (!(USART1->SR & USART_SR_TXE))
        ;

    USART1->DR = (uint16_t)c;
}

void esp_uart_write(const uint8_t *data, uint32_t length)
{
    while (length--)
        esp_uart_putc((char)*data++);
}

void esp_uart_print(const char *s)
{
    while (*s)
        esp_uart_putc(*s++);
}

uint32_t esp_uart_rx_bytes(void)
{
    return esp_rx_bytes;
}

uint32_t esp_uart_overrun_errors(void)
{
    return esp_ore_errors;
}

uint32_t esp_uart_framing_errors(void)
{
    return esp_fe_errors;
}

uint32_t esp_uart_noise_errors(void)
{
    return esp_ne_errors;
}
