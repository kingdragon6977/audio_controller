#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "wifi_config.h"

#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME "audio-esp01"
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

static const uint32_t UART_BAUD = 1000000u;
static const uint16_t PCM_SAMPLES_PER_FRAME = 64u;
static const uint16_t PCM_BYTES_PER_FRAME = PCM_SAMPLES_PER_FRAME * 2u;
static const uint16_t UDP_FRAMES = 4u;
static const uint16_t UDP_BYTES = PCM_BYTES_PER_FRAME * UDP_FRAMES;
static const uint8_t CTRL_READY = 0xF0u;
static const uint8_t CTRL_STOP = 0xF1u;
static const uint8_t CTRL_BOOT_PREP = 0xF2u;
static const uint8_t BOOT_PREP_PASS = 0x01u;
static const uint32_t BOOT_PREP_TIMEOUT_MS = 750u;
static const uint32_t BOOT1_LOW_SETTLE_MS = 250u;
static const uint8_t BOOT_PREP_ACK_MAGIC[] = {
    0xB0u, 0x07u, 0x10u, 0xADu
};

#ifndef UDP_CONTROL_PORT
#define UDP_CONTROL_PORT 5004
#endif

/*
 * STM32F103 ROM-bootloader control hardware:
 *   ESP GPIO0 LOW  -> P-MOS on  -> STM32 BOOT0 HIGH
 *   ESP GPIO0 HIGH -> P-MOS off -> STM32 BOOT0 pulled LOW
 *   ESP GPIO2 HIGH -> N-MOS on  -> STM32 NRST LOW
 *   ESP GPIO2 LOW  -> N-MOS off -> STM32 NRST released
 *
 * GPIO0/GPIO2 are also ESP8266 boot straps, so these levels are only driven
 * after the ESP has booted normally.
 */
static const uint8_t STM32_BOOT_PIN = 0u;
static const uint8_t STM32_RESET_PIN = 2u;
static const uint32_t STM32_ROM_BAUD = 115200u;
static const uint8_t STM32_ACK = 0x79u;
static const uint16_t STM32_EXPECTED_PID = 0x0414u;
static const uint16_t STM32_EXPECTED_FLASH_KB = 256u;
static const uint32_t STM32_FLASH_SIZE_REG = 0x1FFFF7E0u;

// Keep the listener separate from outbound traffic. beginPacket() sets the
// UDP PCB's remote endpoint, which would make a shared socket reject control
// datagrams sent from any other IP/source-port pair.
WiFiUDP udpTx;
WiFiUDP udpControl;
IPAddress targetIp;

static uint8_t header[10];
static uint8_t payload[PCM_BYTES_PER_FRAME];
static uint8_t udpBuffer[UDP_BYTES];
static uint16_t udpFill = 0u;
static uint16_t expectedSeq = 0u;
static bool haveSeq = false;
static bool announcedReady = false;
static bool pcmSeen = false;
static bool otaActive = false;
static uint32_t lastReadyMs = 0u;
static uint32_t lastHeartbeatMs = 0u;
static uint32_t pcmFrames = 0u;

enum ParseState {
    WAIT_A5,
    WAIT_5A,
    READ_HEADER,
    READ_PAYLOAD
};

static ParseState state = WAIT_A5;
static uint8_t headerPos = 0u;
static uint16_t payloadPos = 0u;

static void resetParser()
{
    state = WAIT_A5;
    headerPos = 0u;
    payloadPos = 0u;
}

static bool headerValid()
{
    uint16_t count = (uint16_t)header[6] | ((uint16_t)header[7] << 8);
    uint16_t rate = (uint16_t)header[8] | ((uint16_t)header[9] << 8);

    return header[0] == 0xA5u &&
           header[1] == 0x5Au &&
           header[2] == 0x01u &&
           header[3] == 0x01u &&
           count == PCM_SAMPLES_PER_FRAME &&
           rate == 24000u;
}

static void sendHeartbeat()
{
    char msg[96];
    int n = snprintf(msg, sizeof(msg),
                     "ESP01_HEARTBEAT ip=%s pcm=%lu uart=%lu\n",
                     WiFi.localIP().toString().c_str(),
                     (unsigned long)pcmFrames,
                     (unsigned long)UART_BAUD);

    if (n > 0) {
        udpTx.beginPacket(targetIp, UDP_TARGET_PORT);
        udpTx.write((const uint8_t *)msg, (size_t)n);
        udpTx.endPacket();
    }

    lastHeartbeatMs = millis();
}

static void forwardFrame()
{
    uint16_t seq = (uint16_t)header[4] | ((uint16_t)header[5] << 8);

    pcmSeen = true;
    pcmFrames++;

    if (haveSeq && seq != expectedSeq) {
        udpFill = 0u;
    }
    expectedSeq = (uint16_t)(seq + 1u);
    haveSeq = true;

    memcpy(&udpBuffer[udpFill], payload, PCM_BYTES_PER_FRAME);
    udpFill += PCM_BYTES_PER_FRAME;

    if (udpFill == UDP_BYTES) {
        udpTx.beginPacket(targetIp, UDP_TARGET_PORT);
        udpTx.write(udpBuffer, UDP_BYTES);
        udpTx.endPacket();
        udpFill = 0u;
    }
}

static void consumeByte(uint8_t b)
{
    switch (state) {
    case WAIT_A5:
        if (b == 0xA5u) {
            header[0] = b;
            state = WAIT_5A;
        }
        break;

    case WAIT_5A:
        if (b == 0x5Au) {
            header[1] = b;
            headerPos = 2u;
            state = READ_HEADER;
        } else if (b == 0xA5u) {
            header[0] = b;
        } else {
            state = WAIT_A5;
        }
        break;

    case READ_HEADER:
        header[headerPos++] = b;
        if (headerPos == sizeof(header)) {
            if (!headerValid()) {
                resetParser();
                break;
            }
            payloadPos = 0u;
            state = READ_PAYLOAD;
        }
        break;

    case READ_PAYLOAD:
        payload[payloadPos++] = b;
        if (payloadPos == PCM_BYTES_PER_FRAME) {
            forwardFrame();
            resetParser();
        }
        break;
    }
}

static void sendReady();

static void stm32NormalPins()
{
    pinMode(STM32_BOOT_PIN, OUTPUT);
    digitalWrite(STM32_BOOT_PIN, HIGH);  // BOOT0 low through P-MOS stage
    pinMode(STM32_RESET_PIN, OUTPUT);
    digitalWrite(STM32_RESET_PIN, LOW);  // NRST released through N-MOS stage
}

static void stm32Reset(bool bootloader)
{
    // Select boot source before releasing reset.
    digitalWrite(STM32_BOOT_PIN, bootloader ? LOW : HIGH);
    delay(2);
    digitalWrite(STM32_RESET_PIN, HIGH); // assert NRST low
    delay(25);
    digitalWrite(STM32_RESET_PIN, LOW);  // release NRST
    delay(60);
}

static void serialDrainRx()
{
    while (Serial.available() > 0)
        (void)Serial.read();
}

static int serialReadTimeout(uint32_t timeoutMs)
{
    uint32_t start = millis();
    while ((uint32_t)(millis() - start) < timeoutMs) {
        if (Serial.available() > 0)
            return Serial.read();
        yield();
    }
    return -1;
}

static bool stm32SendCommand(uint8_t command)
{
    uint8_t pair[2] = {command, (uint8_t)(command ^ 0xFFu)};
    Serial.write(pair, sizeof(pair));
    Serial.flush();
    return serialReadTimeout(500u) == STM32_ACK;
}

static bool stm32SendAddress(uint32_t address)
{
    uint8_t bytes[5];
    bytes[0] = (uint8_t)(address >> 24);
    bytes[1] = (uint8_t)(address >> 16);
    bytes[2] = (uint8_t)(address >> 8);
    bytes[3] = (uint8_t)address;
    bytes[4] = (uint8_t)(bytes[0] ^ bytes[1] ^ bytes[2] ^ bytes[3]);

    Serial.write(bytes, sizeof(bytes));
    Serial.flush();
    return serialReadTimeout(500u) == STM32_ACK;
}

static bool stm32ReadMemory(uint32_t address, uint8_t *data, size_t length)
{
    if (data == NULL || length == 0u || length > 256u)
        return false;

    if (!stm32SendCommand(0x11u))
        return false;
    if (!stm32SendAddress(address))
        return false;

    uint8_t count = (uint8_t)(length - 1u);
    uint8_t pair[2] = {count, (uint8_t)(count ^ 0xFFu)};
    Serial.write(pair, sizeof(pair));
    Serial.flush();

    if (serialReadTimeout(500u) != STM32_ACK)
        return false;

    for (size_t i = 0u; i < length; ++i) {
        int b = serialReadTimeout(500u);
        if (b < 0)
            return false;
        data[i] = (uint8_t)b;
    }

    return true;
}

static bool stm32PrepareBoot()
{
    size_t matched = 0u;
    uint32_t start;

    // Discard complete/partial PCM frames already buffered before requesting
    // a quiet link and an explicit PB2/BOOT1-low confirmation.
    serialDrainRx();
    Serial.write(CTRL_BOOT_PREP);
    Serial.flush();

    start = millis();
    while ((uint32_t)(millis() - start) < BOOT_PREP_TIMEOUT_MS) {
        while (Serial.available() > 0) {
            uint8_t b = (uint8_t)Serial.read();

            if (b == BOOT_PREP_ACK_MAGIC[matched]) {
                matched++;
                if (matched == sizeof(BOOT_PREP_ACK_MAGIC)) {
                    int status = serialReadTimeout(100u);
                    return status == BOOT_PREP_PASS;
                }
            } else {
                matched = (b == BOOT_PREP_ACK_MAGIC[0]) ? 1u : 0u;
            }
        }
        yield();
    }

    return false;
}

static void udpReply(const IPAddress &ip, uint16_t port, const char *message)
{
    udpTx.beginPacket(ip, port);
    udpTx.write((const uint8_t *)message, strlen(message));
    udpTx.endPacket();
}

static void stm32BootloaderTest(const IPAddress &replyIp, uint16_t replyPort)
{
    char result[192];
    size_t used = 0u;
    bool bootPrepInitialOk = false;
    bool bootPrepSettledOk = false;
    bool syncOk = false;
    bool getOk = false;
    bool romReadSupported = false;
    bool romWriteSupported = false;
    bool romEraseSupported = false;
    bool idOk = false;
    bool flashSizeOk = false;
    bool targetMatch = false;
    int syncResponse = -1;
    char syncStatus[16];
    uint8_t bootVersion = 0u;
    uint16_t productId = 0u;
    uint16_t flashKb = 0u;

    resetParser();
    udpFill = 0u;
    haveSeq = false;
    pcmSeen = false;
    announcedReady = false;

    // Ask the running application to stop PCM and drive PB2/BOOT1 low. Its
    // acknowledgment includes GPIO output-latch and pin-level readback.
    bootPrepInitialOk = stm32PrepareBoot();
    if (!bootPrepInitialOk) {
        udpReply(replyIp, replyPort,
                 "STM32_BOOT_TEST sync=SKIPPED get=SKIPPED getid=SKIPPED "
                 "bootprep_initial=FAIL bootprep_settled=SKIPPED\n");
        sendReady();
        return;
    }

    // Keep PB2 actively low long enough to discharge the LED/board node before
    // checking it again and changing it from an output to BOOT1 via reset.
    delay(BOOT1_LOW_SETTLE_MS);
    bootPrepSettledOk = stm32PrepareBoot();
    if (!bootPrepSettledOk) {
        udpReply(replyIp, replyPort,
                 "STM32_BOOT_TEST sync=SKIPPED get=SKIPPED getid=SKIPPED "
                 "bootprep_initial=PASS bootprep_settled=FAIL\n");
        sendReady();
        return;
    }

    // STM32 USART ROM protocol uses autobaud sync and even parity.
    Serial.end();
    delay(5);
    Serial.setRxBufferSize(256);
    Serial.begin(STM32_ROM_BAUD, SERIAL_8E1);
    Serial.setDebugOutput(false);
    serialDrainRx();

    stm32Reset(true);
    serialDrainRx();

    Serial.write((uint8_t)0x7Fu);
    Serial.flush();
    syncResponse = serialReadTimeout(1000u);
    syncOk = (syncResponse == STM32_ACK);

    if (syncOk && stm32SendCommand(0x00u)) {
        int n = serialReadTimeout(500u);
        if (n >= 0 && n <= 31) {
            int version = serialReadTimeout(500u);
            if (version >= 0) {
                bootVersion = (uint8_t)version;
                bool bytesOk = true;
                for (int i = 0; i < n; ++i) {
                    int command = serialReadTimeout(500u);
                    if (command < 0) {
                        bytesOk = false;
                        break;
                    }

                    if ((uint8_t)command == 0x11u)
                        romReadSupported = true;
                    else if ((uint8_t)command == 0x31u)
                        romWriteSupported = true;
                    else if ((uint8_t)command == 0x43u ||
                             (uint8_t)command == 0x44u)
                        romEraseSupported = true;
                }
                if (bytesOk && serialReadTimeout(500u) == STM32_ACK)
                    getOk = true;
            }
        }
    }

    if (syncOk && stm32SendCommand(0x02u)) {
        int n = serialReadTimeout(500u);
        if (n >= 0 && n <= 3) {
            uint16_t id = 0u;
            bool bytesOk = true;
            for (int i = 0; i <= n; ++i) {
                int b = serialReadTimeout(500u);
                if (b < 0) {
                    bytesOk = false;
                    break;
                }
                id = (uint16_t)((id << 8) | (uint8_t)b);
            }
            if (bytesOk && serialReadTimeout(500u) == STM32_ACK) {
                productId = id;
                idOk = true;
            }
        }
    }

    if (getOk && romReadSupported &&
        idOk && productId == STM32_EXPECTED_PID) {
        uint8_t flashSizeBytes[2];
        if (stm32ReadMemory(STM32_FLASH_SIZE_REG,
                            flashSizeBytes,
                            sizeof(flashSizeBytes))) {
            flashKb = (uint16_t)flashSizeBytes[0] |
                      ((uint16_t)flashSizeBytes[1] << 8);
            flashSizeOk = true;
        }
    }

    targetMatch = getOk &&
                  romReadSupported &&
                  idOk &&
                  productId == STM32_EXPECTED_PID &&
                  flashSizeOk &&
                  flashKb == STM32_EXPECTED_FLASH_KB;

    // Return the shared UART and STM32 to the normal PCM application.
    digitalWrite(STM32_BOOT_PIN, HIGH); // BOOT0 low
    Serial.end();
    delay(5);
    Serial.setRxBufferSize(2048);
    Serial.begin(UART_BAUD, SERIAL_8N1);
    Serial.setDebugOutput(false);
    stm32Reset(false);
    resetParser();
    udpFill = 0u;
    haveSeq = false;
    pcmSeen = false;
    lastReadyMs = 0u;

    if (syncOk)
        snprintf(syncStatus, sizeof(syncStatus), "ACK");
    else if (syncResponse < 0)
        snprintf(syncStatus, sizeof(syncStatus), "TIMEOUT");
    else
        snprintf(syncStatus, sizeof(syncStatus), "RX_0x%02X",
                 (unsigned int)(uint8_t)syncResponse);

    used = (size_t)snprintf(result, sizeof(result),
                            "STM32_BOOT_TEST sync=%s get=%s getid=%s "
                            "bootprep_initial=PASS bootprep_settled=PASS",
                            syncStatus,
                            getOk ? "OK" : "FAIL",
                            idOk ? "OK" : "FAIL");
    if (getOk && used < sizeof(result))
        used += (size_t)snprintf(result + used, sizeof(result) - used,
                                 " bootver=0x%02X cmds=R%cW%cE%c",
                                 bootVersion,
                                 romReadSupported ? '+' : '-',
                                 romWriteSupported ? '+' : '-',
                                 romEraseSupported ? '+' : '-');
    if (idOk && used < sizeof(result))
        used += (size_t)snprintf(result + used, sizeof(result) - used,
                                 " pid=0x%04X", productId);
    if (flashSizeOk && used < sizeof(result))
        used += (size_t)snprintf(result + used, sizeof(result) - used,
                                 " flash_kb=%u", (unsigned int)flashKb);
    if (used < sizeof(result))
        used += (size_t)snprintf(result + used, sizeof(result) - used,
                                 " target=%s",
                                 targetMatch ? "PASS" : "FAIL");
    if (used < sizeof(result) - 2u) {
        result[used++] = '\n';
        result[used] = '\0';
    }

    udpReply(replyIp, replyPort, result);

    // Re-arm normal streaming immediately; repeated READY remains the fallback.
    sendReady();
}

static void handleUdpControl()
{
    int packetSize = udpControl.parsePacket();
    if (packetSize <= 0)
        return;

    char command[40];
    int count = udpControl.read(command, sizeof(command) - 1u);
    if (count < 0)
        return;
    command[count] = '\0';

    while (count > 0 &&
           (command[count - 1] == '\n' || command[count - 1] == '\r' ||
            command[count - 1] == ' ' || command[count - 1] == '\t')) {
        command[--count] = '\0';
    }

    IPAddress replyIp = udpControl.remoteIP();
    uint16_t replyPort = udpControl.remotePort();

    if (strcmp(command, "STM32_BOOT_TEST") == 0) {
        udpReply(replyIp, replyPort, "STM32_BOOT_TEST starting (read-only)\n");
        stm32BootloaderTest(replyIp, replyPort);
    }
}

static void sendReady()
{
    Serial.write(CTRL_READY);
    Serial.flush();
    announcedReady = true;
    lastReadyMs = millis();
}

static void setupOta()
{
    ArduinoOTA.setHostname(OTA_HOSTNAME);

    if (OTA_PASSWORD[0] != '\0') {
        ArduinoOTA.setPassword(OTA_PASSWORD);
    }

    ArduinoOTA.onStart([]() {
        otaActive = true;
        Serial.write(CTRL_STOP);
        Serial.flush();
        announcedReady = false;
        pcmSeen = false;
        udpFill = 0u;
        haveSeq = false;
        resetParser();
    });

    ArduinoOTA.onEnd([]() {
        otaActive = false;
    });

    ArduinoOTA.onError([](ota_error_t) {
        otaActive = false;
        pcmSeen = false;
        lastReadyMs = 0u;
    });

    ArduinoOTA.begin();
}

static void connectWifi()
{
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        yield();
    }

    targetIp.fromString(UDP_TARGET_IP);
    udpControl.stop();
    udpControl.begin(UDP_CONTROL_PORT);

    pcmSeen = false;
    pcmFrames = 0u;
    sendReady();
    sendHeartbeat();
}

void setup()
{
    // Establish safe STM32 states before Wi-Fi/audio initialization.
    stm32NormalPins();

    Serial.setRxBufferSize(2048);
    Serial.begin(UART_BAUD);
    Serial.setDebugOutput(false);
    delay(100);
    connectWifi();
    setupOta();
}

void loop()
{
    ArduinoOTA.handle();

    if (otaActive) {
        yield();
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        if (announcedReady) {
            Serial.write(CTRL_STOP);
            Serial.flush();
            announcedReady = false;
        }

        WiFi.disconnect();
        delay(100);
        connectWifi();
        resetParser();
        udpFill = 0u;
        haveSeq = false;
    }

    handleUdpControl();

    while (Serial.available() > 0) {
        consumeByte((uint8_t)Serial.read());
    }

    if (!pcmSeen && (uint32_t)(millis() - lastReadyMs) >= 1000u) {
        sendReady();
    }

    if ((uint32_t)(millis() - lastHeartbeatMs) >= 1000u) {
        sendHeartbeat();
    }

    yield();
}
