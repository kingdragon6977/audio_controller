#include <assert.h>
#include <stdio.h>
#include "stm32f10x.h"
#include "uart.h"
GPIO_TypeDef ga,gb;
RCC_TypeDef rcc;
USART_TypeDef usart1,usart2;
static unsigned microseconds, next_half=5333u, packets, overruns, yields;
GPIO_TypeDef *clock_test_gpio(void) { return &gb; }
USART_TypeDef *uart_test_usart2(void) {
    /* Model one debug character slot per 100 us, while PCM needs 2.76 ms
     * of foreground transmission every 5.333 ms. */
    microseconds += 10u;
    usart2.SR = microseconds % 100u == 0u ? USART_SR_TXE : 0u;
    return &usart2;
}
uint32_t board_millis(void) { return microseconds/1000u; }
void audio_stream_service_dma(void) {
    ++yields;
    if(microseconds>=next_half) {
        if(microseconds-next_half>=5333u) ++overruns;
        microseconds+=2760u;
        next_half+=5333u;
        packets+=2u;
    }
}
int main(void) {
    unsigned i;
    uart2_init();
    for(i=0;i<200u;++i) uart2_print("A full diagnostic status line that takes multiple DMA periods to transmit.\r\n");
    assert(microseconds>1000000u);assert(packets>300u);assert(!overruns);
    assert(yields>20000u);
    {
      esp_uart_rx_event_t e;
      assert(!esp_uart_first_rx_error(&e));
    }
    esp_uart_mark_phase(4u);
    usart1.SR=USART_SR_TXE|USART_SR_RXNE|USART_SR_ORE|USART_SR_FE|USART_SR_NE;
    usart1.DR=0xf5;assert((unsigned char)esp_uart_getc()==0xf5);
    assert(esp_uart_rx_bytes()==1 && esp_uart_overrun_errors()==1);
    assert(esp_uart_framing_errors()==1 && esp_uart_noise_errors()==1);
    { esp_uart_rx_event_t e; assert(esp_uart_first_rx_error(&e));
      assert(e.phase==4 && e.byte==0xf5 && e.at_ms==microseconds/1000u);
      assert(e.sr & USART_SR_ORE); }
    usart1.SR=USART_SR_TXE; /* model the hardware's SR/DR flag clear */
    esp_uart_init();
    { esp_uart_rx_event_t e; assert(!esp_uart_first_rx_error(&e)); }
    assert(!esp_uart_overrun_errors());
    usart1.SR=USART_SR_TXE|USART_SR_RXNE;usart1.DR=0xf0;
    esp_uart_poll_startup();usart1.SR=USART_SR_TXE;
    microseconds+=1100000u;
    usart1.SR=USART_SR_TXE|USART_SR_RXNE;usart1.DR=0xf0;
    esp_uart_poll_startup();usart1.SR=USART_SR_TXE;
    esp_uart_mark_phase(4u);
    assert(esp_uart_available());assert((unsigned char)esp_uart_getc()==0xf0);
    assert(!esp_uart_available());assert(esp_uart_rx_bytes()==2);
    assert(!esp_uart_overrun_errors());
    esp_uart_init();usart1.SR=USART_SR_TXE|USART_SR_RXNE;usart1.DR=0xf4;
    esp_uart_poll_startup();usart1.SR=USART_SR_TXE;
    esp_uart_mark_phase(4u);assert(!esp_uart_available()); /* No deferred boot grant. */
    puts("UART debug output services DMA deadlines; USART1 error accounting PASS");
}
