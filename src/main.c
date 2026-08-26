#include "stm32f10x.h"
#include "board.h"
#include "uart.h"
#include "i2c.h"
#include "cli.h"
#include "codec.h"
#include "diagnostics.h"

static void delay(uint32_t d)
{
    while (d--)
        __asm__("nop");
}

int main(void)
{
    int codec_present = 0;
    int i2c_safe;
    int i2s_safe;
    int i2s_clock_active = 0;

    /*
     * Bring-up order is deliberate:
     *
     *   1. board safe state
     *   2. debug UART
     *   3. hardware identity/clock/pin evidence
     *   4. configure I2C
     *   5. verify I2C electrical/configuration state
     *   6. only then touch the codec
     *   7. configure/verify codec I2S clocking
     */
    board_init();
    uart2_init();
    cli_init();

    uart2_print("\r\n========================================\r\n");
    uart2_print(" audio_controller - RCT6 bring-up\r\n");
    uart2_print("========================================\r\n");
    uart2_print("USART2: PA2=TX PA3=RX 115200 8N1\r\n");

    diagnostics_print_mcu();
    diagnostics_print_clock();
    diagnostics_print_audio_pins();
    diagnostics_print_i2s_clock_ownership();

    i2s_safe = diagnostics_i2s2_safe();
    uart2_print("\r\nI2S PIN SAFETY: ");
    uart2_print(i2s_safe ? "PASS - codec-driven pins are not MCU outputs\r\n"
                         : "FAIL - I2S pin is configured as an MCU output\r\n");

    uart2_print("\r\nInitializing I2C1...\r\n");
    i2c1_init();
    uart2_print("I2C1 initialization returned.\r\n");
    diagnostics_print_i2c1();
    diagnostics_print_audio_pins();

    i2c_safe = diagnostics_i2c1_safe();
    uart2_print("I2C1 BUS SAFETY: ");
    uart2_print(i2c_safe ? "PASS - AF open-drain and SDA/SCL released HIGH\r\n"
                         : "FAIL - refusing I2C transaction because pin/bus state is unsafe\r\n");

    /*
     * TLV320ADC3101 requires a hardware reset after its supplies are valid.
     * This must happen before the first I2C transaction. PB14 is dedicated
     * to the codec RESET input on this board.
     */
    uart2_print("\r\nResetting TLV320ADC3101 on PB14...\r\n");
    codec_reset();
    uart2_print("TLV320 reset released.\r\n");
    diagnostics_print_audio_pins();

    /* Never probe a bus whose electrical state failed the preflight check. */
    if (i2c_safe)
    {
        uart2_print("\r\nTLV320ADC3101:\r\n");
        uart2_print("  RESET: PB14 active-low hardware reset\r\n");
        uart2_print("  I2C1: PB6=SCL PB7=SDA\r\n");
        uart2_print("  Address: 0x18 (7-bit)\r\n");
        uart2_print("  Probing... ");

        codec_present = i2c1_probe(TLV320ADC3101_ADDR);
        uart2_print(codec_present ? "ACK - codec responded\r\n"
                                   : "NO ACK / ERROR\r\n");
        diagnostics_print_i2c1();
    }
    else
    {
        uart2_print("\r\nCodec probe SKIPPED for safety.\r\n");
    }

    /* Re-read the pins at the point where codec I2S output could be enabled. */
    i2s_safe = diagnostics_i2s2_safe();
    uart2_print("I2S PRE-ACTIVATION SAFETY: ");
    uart2_print(i2s_safe ? "PASS\r\n"
                         : "FAIL - codec I2S activation BLOCKED\r\n");

    if (codec_present && i2s_safe)
    {
        uart2_print("Applying AV6301 codec profile...\r\n");
        if (codec_apply_av6301_profile())
        {
            uart2_print("Codec profile applied.\r\n");
            uart2_print("Read-back trail follows:\r\n");
            codec_dump_profile();

            diagnostics_print_i2s_clock_ownership();
            uart2_print("\r\nI2S CLOCK ACTIVITY CHECK: ");
            i2s_clock_active = diagnostics_i2s_clock_pins_active();
            uart2_print(i2s_clock_active
                        ? "PASS - WCLK activity observed on PB12\r\n"
                        : "WARN - no WCLK transition observed by coarse MCU poll\r\n");
            uart2_print("  NOTE: logic analyzer/scope remains authoritative for exact clock frequency and duty cycle.\r\n");
        }
        else
        {
            uart2_print("Codec profile write failed.\r\n");
        }
    }
    else if (!i2s_safe)
    {
        uart2_print("Codec profile NOT applied: I2S pins are not in a safe input state.\r\n");
    }
    else
    {
        uart2_print("Codec profile NOT applied because no ACK was received.\r\n");
    }

    /* SPI2/I2S is currently the MCU-side observation point; the TLV320 is
     * configured as the I2S clock master. Keep this report even when SPI2 is
     * disabled so its state cannot be mistaken for the codec's I2S clock. */
    diagnostics_print_i2s2();
    diagnostics_print_audio_pins();

    delay(500000u);

    uart2_print("\r\nBring-up complete. CLI ready.\r\n");
    uart2_print("> ");

    while (1)
    {
        cli_task();

        led_on();
        delay(120000u);
        led_off();
        delay(120000u);
    }
}
