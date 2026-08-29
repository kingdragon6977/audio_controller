#include <string.h>
#include "stm32f10x.h"
#include "uart.h"
#include "i2c.h"
#include "codec.h"
#include "i2s_rx.h"
#include "cli.h"
#include <stdio.h>

#define CLI_LINE_SIZE 64
#define CLI_HISTORY_SIZE 8

static char line[CLI_LINE_SIZE];
static int line_index = 0;
static int cursor_index = 0;
static char history[CLI_HISTORY_SIZE][CLI_LINE_SIZE];
static unsigned int history_count = 0u;
static unsigned int history_head = 0u;
static int history_pos = -1;
static unsigned char esc_state = 0u;

static void cli_redraw(void)
{
    int i;
    uart2_print("\r\033[2K> ");
    uart2_print(line);
    for (i = line_index; i > cursor_index; --i)
        uart2_print("\033[D");
}

static void cli_history_store(const char *cmd)
{
    unsigned int last;
    if (!cmd[0]) return;
    if (history_count)
    {
        last = (history_head + CLI_HISTORY_SIZE - 1u) % CLI_HISTORY_SIZE;
        if (strcmp(history[last], cmd) == 0) return;
    }
    strncpy(history[history_head], cmd, CLI_LINE_SIZE - 1u);
    history[history_head][CLI_LINE_SIZE - 1u] = 0;
    history_head = (history_head + 1u) % CLI_HISTORY_SIZE;
    if (history_count < CLI_HISTORY_SIZE) history_count++;
}

static void cli_history_recall(int direction)
{
    unsigned int oldest, slot;
    if (!history_count) return;
    if (direction < 0)
    {
        if (history_pos < 0) history_pos = (int)history_count - 1;
        else if (history_pos > 0) history_pos--;
    }
    else
    {
        if (history_pos < 0) return;
        if (history_pos < (int)history_count - 1) history_pos++;
        else
        {
            history_pos = -1;
            line[0] = 0; line_index = 0; cursor_index = 0;
            cli_redraw();
            return;
        }
    }
    oldest = (history_head + CLI_HISTORY_SIZE - history_count) % CLI_HISTORY_SIZE;
    slot = (oldest + (unsigned int)history_pos) % CLI_HISTORY_SIZE;
    strcpy(line, history[slot]);
    line_index = (int)strlen(line);
    cursor_index = line_index;
    cli_redraw();
}

static int run_i2s_capture(unsigned int sequence, int verbose)
{
    uint32_t timeout = 3000000u;
    const uint16_t *samples;
    unsigned int i;
    if (verbose) uart2_print("Starting frame-aligned I2S capture...\r\n");
    if (!i2s_rx_start_capture()) {
        char buf[64];
        if (verbose) uart2_print("I2S capture FAILED to start (pin safety or WCLK sync).\r\n");
        else { sprintf(buf, "  #%u FAIL start/sync\r\n", sequence); uart2_print(buf); }
        i2s_rx_stop(); return 0;
    }
    while (!i2s_rx_capture_complete() && timeout != 0u) { timeout--; __asm__("nop"); }
    if (i2s_rx_error_flags()) {
        char buf[64];
        if (verbose) uart2_print("I2S capture FAILED - DMA transfer error.\r\n");
        else { sprintf(buf, "  #%u FAIL DMA\r\n", sequence); uart2_print(buf); }
        i2s_rx_stop(); return 0;
    }
    if (!i2s_rx_capture_complete()) {
        char buf[64];
        if (verbose) uart2_print("I2S capture FAILED - timeout.\r\n");
        else { sprintf(buf, "  #%u FAIL timeout\r\n", sequence); uart2_print(buf); }
        i2s_rx_stop(); return 0;
    }
    if (verbose) { uart2_print("I2S capture PASS.\r\n"); i2s_rx_print_analysis(); }
    else {
        int32_t min_a=32767,max_a=-32768,min_b=32767,max_b=-32768,peak_a=0,peak_b=0,sum_a=0,sum_b=0;
        unsigned int pairs=0u; char buf[160]; samples=i2s_rx_buffer();
        for (i=2u; i+1u<I2S_RX_SAMPLES; i+=2u) {
            int32_t a=(int16_t)samples[i], b=(int16_t)samples[i+1u];
            int32_t aa=(a<0)?-a:a, ab=(b<0)?-b:b;
            if(a<min_a)min_a=a; if(a>max_a)max_a=a; if(b<min_b)min_b=b; if(b>max_b)max_b=b;
            if(aa>peak_a)peak_a=aa; if(ab>peak_b)peak_b=ab; sum_a+=a; sum_b+=b; pairs++;
        }
        sprintf(buf,"  #%u A[min=%ld max=%ld mean=%ld peak=%ld] B[min=%ld max=%ld mean=%ld peak=%ld]\r\n",
            sequence,(long)min_a,(long)max_a,(long)(pairs?sum_a/(int32_t)pairs:0),(long)peak_a,
            (long)min_b,(long)max_b,(long)(pairs?sum_b/(int32_t)pairs:0),(long)peak_b); uart2_print(buf);
    }
    i2s_rx_stop(); return 1;
}

static int parse_capture_count(const char *cmd, unsigned int *count)
{
    const char prefix[]="i2s capture "; const char *p; unsigned int value=0u;
    if(strncmp(cmd,prefix,sizeof(prefix)-1u)!=0)return 0; p=cmd+sizeof(prefix)-1u; if(*p==0)return 0;
    while(*p>='0'&&*p<='9'){value=value*10u+(unsigned int)(*p-'0');if(value>50u)return 0;p++;}
    if(*p!=0||value==0u)return 0; *count=value; return 1;
}

static void execute(char *cmd)
{
    unsigned int capture_count;
    if(strcmp(cmd,"help")==0){
        uart2_print("\r\nCommands:\r\n help\r\n id\r\n uid\r\n clock\r\n codec\r\n codec dump\r\n codec apply\r\n");
        uart2_print(" i2s capture        (full one-shot report)\r\n i2s capture N      (compact repeated captures, N=1..50)\r\n");
        uart2_print(" reboot\r\n led on\r\n led off\r\n esp test\r\n\r\nLine editing: Backspace/Delete, Left/Right arrows, Up/Down history (8 commands)\r\n"); return;
    }
    if(strcmp(cmd,"id")==0){uart2_print("STM32F103RCT6 HD\r\n");return;}
    if(strcmp(cmd,"clock")==0){static const char hex[]="0123456789ABCDEF";int s;uart2_print("RCC->CR   = ");for(s=28;s>=0;s-=4)uart2_putc(hex[(RCC->CR>>s)&0x0Fu]);uart2_print("\r\nRCC->CFGR = ");for(s=28;s>=0;s-=4)uart2_putc(hex[(RCC->CFGR>>s)&0x0Fu]);uart2_print("\r\n");return;}
    if(strcmp(cmd,"codec")==0){uart2_print("TLV320ADC3101 @ 0x18: ");uart2_print(i2c1_probe(TLV320ADC3101_ADDR)?"ACK\r\n":"NO ACK\r\n");return;}
    if(strcmp(cmd,"codec dump")==0){if(!i2c1_probe(TLV320ADC3101_ADDR)){uart2_print("TLV320ADC3101 @ 0x18: NO ACK\r\n");return;}codec_dump_profile();return;}
    if(strcmp(cmd,"codec apply")==0){uart2_print("Applying captured AV6301 TLV320ADC3101 profile...\r\nWARNING: this drives the shared I2C bus; use only after isolating the AV6301.\r\n");uart2_print(codec_apply_av6301_profile()?"Codec profile applied successfully.\r\n":"Codec profile FAILED (I2C timeout/NACK).\r\n");return;}
    if(strcmp(cmd,"i2s capture")==0){(void)run_i2s_capture(1u,1);return;}
    if(parse_capture_count(cmd,&capture_count)){unsigned int n,passed=0u;char buf[80];sprintf(buf,"Running %u compact frame-aligned I2S captures...\r\n",capture_count);uart2_print(buf);for(n=1u;n<=capture_count;++n)if(run_i2s_capture(n,0))passed++;sprintf(buf,"Repeated capture summary: %u/%u PASS\r\n",passed,capture_count);uart2_print(buf);return;}
    if(strncmp(cmd,"i2s capture ",12u)==0){uart2_print("Usage: i2s capture N, where N is 1..50\r\n");return;}
    if(strcmp(cmd,"uid")==0){uint32_t *uid=(uint32_t*)0x1FFFF7E8;char buf[80];sprintf(buf,"%08lX %08lX %08lX\r\n",uid[0],uid[1],uid[2]);uart2_print(buf);return;}
    if(strcmp(cmd,"led on")==0){GPIO_SetBits(GPIOB,GPIO_Pin_2);uart2_print("LED ON\r\n");return;}
    if(strcmp(cmd,"led off")==0){GPIO_ResetBits(GPIOB,GPIO_Pin_2);uart2_print("LED OFF\r\n");return;}
    if(strcmp(cmd,"esp test")==0){esp_uart_print("AT\r\n");uart2_print("ESP: AT sent\r\n");return;}
    if(strcmp(cmd,"reboot")==0){NVIC_SystemReset();}
    if(cmd[0]) uart2_print("Unknown command\r\n");
}

void cli_init(void)
{
    line_index=0; cursor_index=0; line[0]=0; history_count=0u; history_head=0u; history_pos=-1; esc_state=0u;
}

void cli_task(void)
{
    while(uart2_available()) {
        unsigned char c=(unsigned char)uart2_getc(); int i;
        if(esc_state==1u){if(c=='['||c=='O')esc_state=2u;else esc_state=0u;continue;}
        if(esc_state==2u){
            esc_state=0u;
            if(c=='A'){cli_history_recall(-1);continue;}
            if(c=='B'){cli_history_recall(1);continue;}
            if(c=='C'){if(cursor_index<line_index){uart2_print("\033[C");cursor_index++;}continue;}
            if(c=='D'){if(cursor_index>0){uart2_print("\033[D");cursor_index--;}continue;}
            if(c=='3'){esc_state=3u;continue;}
            continue;
        }
        if(esc_state==3u){esc_state=0u;if(c=='~'&&cursor_index<line_index){for(i=cursor_index;i<line_index;i++)line[i]=line[i+1];line_index--;cli_redraw();}continue;}
        if(c==0x1Bu){esc_state=1u;continue;}
        if(c=='\r'||c=='\n'){
            if(c=='\n'&&line_index==0)continue;
            line[line_index]=0; uart2_print("\r\n"); cli_history_store(line); execute(line);
            line_index=0;cursor_index=0;line[0]=0;history_pos=-1;uart2_print("> "); continue;
        }
        if(c==0x08u||c==0x7Fu){
            if(cursor_index>0){for(i=cursor_index-1;i<line_index;i++)line[i]=line[i+1];cursor_index--;line_index--;cli_redraw();}
            continue;
        }
        if(c>=0x20u&&c<=0x7Eu&&line_index<CLI_LINE_SIZE-1){
            for(i=line_index;i>cursor_index;i--)line[i]=line[i-1];line[cursor_index]=(char)c;line_index++;cursor_index++;line[line_index]=0;cli_redraw();
        }
    }
}
