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
    usart1.SR=USART_SR_TXE|USART_SR_RXNE|USART_SR_ORE|USART_SR_FE|USART_SR_NE;
    usart1.DR=0xf5;assert((unsigned char)esp_uart_getc()==0xf5);
    assert(esp_uart_rx_bytes()==1 && esp_uart_overrun_errors()==1);
    assert(esp_uart_framing_errors()==1 && esp_uart_noise_errors()==1);
    puts("UART debug output services DMA deadlines; USART1 error accounting PASS");
}
