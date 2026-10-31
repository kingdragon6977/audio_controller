#include "stm32f10x.h"
#include "audio_stream.h"
#include "board.h"
#include "diagnostics.h"
#include "i2s_rx.h"
#include "uart.h"
#include "flash_authorization.h"
#include "audio_clock.h"
#include "codec.h"
#include "audio_dma_buffer.h"

#define STREAM_DMA_SLOTS      1024u
#define STREAM_HALF_SLOTS      512u
#define STREAM_PACKET_SAMPLES   64u
#define STREAM_SAMPLE_RATE   24000u
/* 512 slots = 256 stereo frames = 5.333 ms at 48 kHz. Two 138-byte
 * UART frames take 2.76 ms at 1 Mbaud, leaving time to service capture. */
#if (STREAM_HALF_SLOTS % 256u) != 0u
#error Stream half-buffer must contain complete PCM UART frames
#endif
#define ESP_CTRL_READY         0xF0u
#define ESP_CTRL_STOP          0xF1u
#define ESP_CTRL_BOOT_PREP     0xF2u
#define ESP_CTRL_FLASH_PREP    0xF3u
#define ESP_CTRL_FLASH_AUTH    0xF4u
#define ESP_CTRL_HEALTH        0xF5u
#define BOOT_PREP_PASS         0x01u
#define BOOT_PREP_FAIL         0x00u
#define BOOT_PREP_PROTOCOL     0x01u

static const uint8_t boot_prep_ack_magic[] = {
    0xB0u, 0x07u, 0x10u, 0xADu
};

static const uint8_t boot_prep_target_tag[] = {
    'R', 'C', 'T', '6'
};

static uint16_t stream_dma_buffer[STREAM_DMA_SLOTS];
static uint16_t stream_snapshot[STREAM_HALF_SLOTS];
static uint32_t stream_overruns;
static uint32_t stream_stalls;
static uint32_t last_dma_ms;
static uint32_t last_start_ms;
static int16_t packet_samples[STREAM_PACKET_SAMPLES];
static uint32_t packet_fill;
static uint16_t packet_sequence;
static uint32_t packet_count;
static uint32_t dma_error_count;
static uint8_t running;
static uint8_t esp_ready;
static uint8_t ws_high_at_dma;
static uint8_t start_message_printed;
static FlashAuthorization flash_authorization;

static void release_flash_authorization(void)
{
    flash_authorization_clear(&flash_authorization);
    board_boot_auth_off();
}

static void authorize_flash_request(void)
{
    uint8_t request[FLASH_AUTH_BYTES];
    uint8_t status = BOOT_PREP_FAIL, protocol = 2u;
    uint8_t crc_bytes[2];
    uint32_t start = board_millis();
    unsigned int received = 0u, crc_received = 0u;
    esp_ready = 0u;
    audio_stream_stop();
    start_message_printed = 0u;
    /* Bounded framing: incomplete traffic must never assert PA0. */
    while (received < sizeof(request) &&
           (uint32_t)(board_millis() - start) < 100u) {
        if (esp_uart_available()) {
            request[received] = (uint8_t)esp_uart_getc();
            ++received;
        }
    }
    while (crc_received < 2u &&
           (uint32_t)(board_millis() - start) < 100u) {
        if (esp_uart_available()) crc_bytes[crc_received++] = (uint8_t)esp_uart_getc();
    }
    if (received == sizeof(request) && crc_received == 2u &&
        ((uint16_t)crc_bytes[0] | ((uint16_t)crc_bytes[1] << 8)) == flash_auth_crc16(request) &&
        board_boot1_hold_low()) {
        int was_active = flash_authorization.active;
        if (flash_authorization_request(&flash_authorization, request, board_millis())) {
            if (!was_active) board_boot_auth_lease(FLASH_AUTH_LEASE_MS);
            if (board_boot_auth_is_on()) status = BOOT_PREP_PASS;
        }
    }
    if (status != BOOT_PREP_PASS) release_flash_authorization();
    esp_uart_write(boot_prep_ack_magic, sizeof(boot_prep_ack_magic));
    esp_uart_write(&status, 1u);
    esp_uart_write(&protocol, 1u);
    esp_uart_write(boot_prep_target_tag, sizeof(boot_prep_target_tag));
    if (received == sizeof(request)) esp_uart_write(request, 4u);
    uart2_print(status == BOOT_PREP_PASS
                ? "STM32 FLASH AUTH: PASS - session/image bound; PA0 lease 2s.\r\n"
                : "STM32 FLASH AUTH: FAIL - authorization released.\r\n");
}

static void clear_spi2_rx_state(void)
{
    volatile uint16_t value;

    if ((SPI2->SR & SPI_SR_RXNE) != 0u)
    {
        value = SPI2->DR;
        (void)value;
    }

    if ((SPI2->SR & SPI_SR_OVR) != 0u)
    {
        value = SPI2->DR;
        value = SPI2->SR;
        (void)value;
    }
}

static int wait_for_ws_falling_edge(void)
{
    uint32_t timeout = 400000u;

    while ((GPIOB->IDR & GPIO_Pin_12) == 0u)
    {
        if (timeout-- == 0u)
            return 0;
    }

    timeout = 400000u;
    while ((GPIOB->IDR & GPIO_Pin_12) != 0u)
    {
        if (timeout-- == 0u)
            return 0;
    }

    return 1;
}

static int sync_receiver_before_dma(void)
{
    uint32_t timeout;
    uint16_t sr;
    volatile uint16_t value;
    unsigned int words_seen = 0u;

    I2S_Cmd(SPI2, DISABLE);
    clear_spi2_rx_state();

    if (!wait_for_ws_falling_edge())
        return 0;

    I2S_Cmd(SPI2, ENABLE);

    while (words_seen < 4u)
    {
        timeout = 400000u;
        while ((SPI2->SR & SPI_SR_RXNE) == 0u)
        {
            if ((SPI2->SR & SPI_SR_OVR) != 0u)
            {
                clear_spi2_rx_state();
                I2S_Cmd(SPI2, DISABLE);
                return 0;
            }

            if (timeout-- == 0u)
            {
                I2S_Cmd(SPI2, DISABLE);
                return 0;
            }
        }

        sr = SPI2->SR;
        value = SPI2->DR;
        (void)value;
        words_seen++;

        if ((sr & SPI_SR_OVR) != 0u)
        {
            clear_spi2_rx_state();
            I2S_Cmd(SPI2, DISABLE);
            return 0;
        }

        if ((sr & I2S_FLAG_CHSIDE) != 0u)
            return 1;
    }

    I2S_Cmd(SPI2, DISABLE);
    return 0;
}

static void send_packet(void)
{
    uint8_t header[10];

    header[0] = 0xA5u;
    header[1] = 0x5Au;
    header[2] = 0x01u;
    header[3] = 0x01u;
    header[4] = (uint8_t)(packet_sequence & 0xFFu);
    header[5] = (uint8_t)(packet_sequence >> 8);
    header[6] = (uint8_t)(STREAM_PACKET_SAMPLES & 0xFFu);
    header[7] = (uint8_t)(STREAM_PACKET_SAMPLES >> 8);
    header[8] = (uint8_t)(STREAM_SAMPLE_RATE & 0xFFu);
    header[9] = (uint8_t)(STREAM_SAMPLE_RATE >> 8);

    esp_uart_write(header, sizeof(header));
    esp_uart_write((const uint8_t *)packet_samples, sizeof(packet_samples));

    packet_sequence++;
    packet_count++;
    packet_fill = 0u;
}

static uint32_t stream_dma_remaining(void) { return DMA1_Channel4->CNDTR; }

static int process_half(unsigned int first_slot)
{
    unsigned int frame;
    if (!audio_dma_snapshot(stream_snapshot, stream_dma_buffer, STREAM_HALF_SLOTS,
                            first_slot, stream_dma_remaining)) return 0;

    for (frame = 0u; frame < STREAM_HALF_SLOTS / 2u; frame += 2u)
    {
        unsigned int base = frame * 2u;
        unsigned int left_index = base + (ws_high_at_dma ? 1u : 0u);

        packet_samples[packet_fill++] = (int16_t)stream_snapshot[left_index];

        if (packet_fill == STREAM_PACKET_SAMPLES)
            send_packet();
    }
    return 1;
}

int audio_stream_start(void)
{
    DMA_InitTypeDef dma;
    I2S_InitTypeDef i2s;
    volatile uint16_t dummy_dr;
    volatile uint16_t dummy_sr;

    if (running)
        return 1;

    i2s_rx_stop();
    if (!diagnostics_i2s2_safe() || !codec_profile_ready() || !audio_clock_check_wclk(0))
        return 0;

    packet_fill = 0u;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    I2S_Cmd(SPI2, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_GL4 | DMA1_FLAG_TC4 |
                  DMA1_FLAG_HT4 | DMA1_FLAG_TE4);

    dummy_dr = SPI2->DR;
    dummy_sr = SPI2->SR;
    (void)dummy_dr;
    (void)dummy_sr;

    dma.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)stream_dma_buffer;
    dma.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma.DMA_BufferSize = STREAM_DMA_SLOTS;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma.DMA_Mode = DMA_Mode_Circular;
    dma.DMA_Priority = DMA_Priority_VeryHigh;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel4, &dma);

    i2s.I2S_Mode = I2S_Mode_SlaveRx;
    i2s.I2S_Standard = I2S_Standard_Phillips;
    i2s.I2S_DataFormat = I2S_DataFormat_16b;
    i2s.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
    i2s.I2S_AudioFreq = I2S_AudioFreq_48k;
    i2s.I2S_CPOL = I2S_CPOL_Low;
    I2S_Init(SPI2, &i2s);

    if (!sync_receiver_before_dma())
        return 0;

    ws_high_at_dma = (GPIOB->IDR & GPIO_Pin_12) ? 1u : 0u;

    DMA_ClearFlag(DMA1_FLAG_GL4 | DMA1_FLAG_TC4 |
                  DMA1_FLAG_HT4 | DMA1_FLAG_TE4);
    DMA_SetCurrDataCounter(DMA1_Channel4, STREAM_DMA_SLOTS);
    DMA_Cmd(DMA1_Channel4, ENABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, ENABLE);

    last_dma_ms = board_millis();
    running = 1u;
    return 1;
}

void audio_stream_stop(void)
{
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    DMA_Cmd(DMA1_Channel4, DISABLE);
    I2S_Cmd(SPI2, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_GL4 | DMA1_FLAG_TC4 |
                  DMA1_FLAG_HT4 | DMA1_FLAG_TE4);
    clear_spi2_rx_state();
    running = 0u;
    packet_fill = 0u;
}

void audio_stream_task(void)
{
    if (flash_authorization_expired(&flash_authorization, board_millis()))
        release_flash_authorization();
    while (esp_uart_available())
    {
        uint8_t control = (uint8_t)esp_uart_getc();

        if (control == ESP_CTRL_READY)
        {
            release_flash_authorization();
            if (!esp_ready)
            {
                esp_ready = 1u;
                start_message_printed = 0u;
                uart2_print("ESP-01: Wi-Fi/UDP ready; enabling PCM stream.\r\n");
            }
        }
        else if (control == ESP_CTRL_STOP)
        {
            release_flash_authorization();
            if (esp_ready || running)
            {
                esp_ready = 0u;
                audio_stream_stop();
                start_message_printed = 0u;
                uart2_print("ESP-01: stream stopped by receiver.\r\n");
            }
        }
        else if (control == ESP_CTRL_HEALTH)
        {
            uint8_t status = (running && codec_profile_ready() && audio_clock_rate_valid() && !board_boot_auth_is_on() &&
                              (GPIOA->ODR & GPIO_Pin_0) != 0u &&
                              (GPIOA->IDR & GPIO_Pin_0) != 0u)
                             ? BOOT_PREP_PASS : BOOT_PREP_FAIL;
            uint8_t protocol = BOOT_PREP_PROTOCOL;
            esp_uart_write(boot_prep_ack_magic, sizeof(boot_prep_ack_magic));
            esp_uart_write(&status, 1u);
            esp_uart_write(&protocol, 1u);
            esp_uart_write(boot_prep_target_tag, sizeof(boot_prep_target_tag));
        }
        else if (control == ESP_CTRL_FLASH_AUTH)
        {
            authorize_flash_request();
        }
        else if (control == ESP_CTRL_BOOT_PREP ||
                 control == ESP_CTRL_FLASH_PREP)
        {
            uint8_t status;
            int boot1_ok;
            int auth_ok = 1;

            esp_ready = 0u;
            audio_stream_stop();
            start_message_printed = 0u;

            boot1_ok = board_boot1_hold_low();
            if (control == ESP_CTRL_FLASH_PREP)
                auth_ok = board_boot_auth_is_on();

            status = (boot1_ok && auth_ok) ? BOOT_PREP_PASS : BOOT_PREP_FAIL;
            {
                uint8_t protocol = BOOT_PREP_PROTOCOL;
                esp_uart_write(boot_prep_ack_magic, sizeof(boot_prep_ack_magic));
                esp_uart_write(&status, 1u);
                esp_uart_write(&protocol, 1u);
                esp_uart_write(boot_prep_target_tag, sizeof(boot_prep_target_tag));
            }

            if (control == ESP_CTRL_FLASH_PREP)
            {
                uart2_print(status == BOOT_PREP_PASS
                            ? "STM32 FLASH PREP: PASS - PB2/BOOT1 LOW and PA0 BOOT_AUTH active.\r\n"
                            : "STM32 FLASH PREP: FAIL - requires PB2/BOOT1 LOW and PA0 BOOT_AUTH active.\r\n");
            }
            else
            {
                uart2_print(status == BOOT_PREP_PASS
                            ? "STM32 BOOT PREP: PASS - PCM stopped; PB2/BOOT1 driven and read LOW.\r\n"
                            : "STM32 BOOT PREP: FAIL - PB2/BOOT1 did not read LOW; ROM test blocked.\r\n");
            }
        }
    }

    /* A diagnostic capture/meter may temporarily take DMA1 CH4 away. */
    if (running && (DMA1_Channel4->CCR & 0x0001u) == 0u)
        running = 0u;

    if (!running)
    {
        if (esp_ready && (uint32_t)(board_millis() - last_start_ms) >= 1000u)
        {
            last_start_ms = board_millis();
            if (audio_stream_start())
            {
                if (!start_message_printed)
                {
                    uart2_print("AUDIO STREAM: 24 kHz mono PCM16 -> USART1 1 Mbaud -> ESP-01.\r\n");
                    start_message_printed = 1u;
                }
            }
        }
        return;
    }

    audio_stream_service_dma();
}

static void service_dma_once(void)
{
    if (DMA_GetFlagStatus(DMA1_FLAG_TE4))
    {
        dma_error_count++;
        ++packet_sequence;
        audio_stream_stop();
        uart2_print("AUDIO STREAM: DMA error; will retry while ESP remains ready.\r\n");
        return;
    }

    if ((uint32_t)(board_millis() - last_dma_ms) >= 100u) {
        ++stream_stalls;
        ++packet_sequence;
        audio_stream_stop();
        uart2_print("AUDIO STREAM: clock/DMA progress timeout; stopped, rate recheck required.\r\n");
        return;
    }
    if (DMA_GetFlagStatus(DMA1_FLAG_HT4) && DMA_GetFlagStatus(DMA1_FLAG_TC4))
        goto overrun;
    if (DMA_GetFlagStatus(DMA1_FLAG_HT4)) {
        DMA_ClearFlag(DMA1_FLAG_HT4);
        if (!process_half(0u)) goto overrun;
        last_dma_ms = board_millis();
    }
    if (DMA_GetFlagStatus(DMA1_FLAG_TC4)) {
        DMA_ClearFlag(DMA1_FLAG_TC4);
        if (!process_half(STREAM_HALF_SLOTS)) goto overrun;
        last_dma_ms = board_millis();
    }
    return;
overrun:
    ++stream_overruns;
    ++packet_sequence; /* signal a transport discontinuity instead of hiding it */
    audio_stream_stop();
    uart2_print("AUDIO STREAM: missed DMA half-buffer; stopped, resynchronizing.\r\n");
}

void audio_stream_service_dma(void)
{
    static uint8_t busy;
    if (busy || !running) return;
    /* A diagnostic capture may own the same channel with a different buffer.
     * Never consume its flags or stop its hardware from the debug yield path. */
    if (!(DMA1_Channel4->CCR & 1u) ||
        DMA1_Channel4->CMAR != (uint32_t)stream_dma_buffer) {
        running = 0u;
        return;
    }
    busy = 1u;
    service_dma_once();
    busy = 0u;
}

int audio_stream_running(void)
{
    return running ? 1 : 0;
}

uint32_t audio_stream_packets(void)
{
    return packet_count;
}

uint32_t audio_stream_dma_errors(void)
{
    return dma_error_count;
}

uint32_t audio_stream_overruns(void) { return stream_overruns; }
uint32_t audio_stream_stalls(void) { return stream_stalls; }
