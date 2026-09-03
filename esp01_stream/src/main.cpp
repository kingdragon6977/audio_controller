#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "wifi_config.h"

static const uint32_t UART_BAUD = 1000000u;
static const uint16_t PCM_SAMPLES_PER_FRAME = 64u;
static const uint16_t PCM_BYTES_PER_FRAME = PCM_SAMPLES_PER_FRAME * 2u;
static const uint16_t UDP_FRAMES = 4u;
static const uint16_t UDP_BYTES = PCM_BYTES_PER_FRAME * UDP_FRAMES;
static const uint8_t CTRL_READY = 0xF0u;
static const uint8_t CTRL_STOP = 0xF1u;

WiFiUDP udp;
IPAddress targetIp;

static uint8_t header[10];
static uint8_t payload[PCM_BYTES_PER_FRAME];
static uint8_t udpBuffer[UDP_BYTES];
static uint16_t udpFill = 0u;
static uint16_t expectedSeq = 0u;
static bool haveSeq = false;
static bool announcedReady = false;
static bool pcmSeen = false;
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
        udp.beginPacket(targetIp, UDP_TARGET_PORT);
        udp.write((const uint8_t *)msg, (size_t)n);
        udp.endPacket();
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
        udp.beginPacket(targetIp, UDP_TARGET_PORT);
        udp.write(udpBuffer, UDP_BYTES);
        udp.endPacket();
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

static void sendReady()
{
    Serial.write(CTRL_READY);
    Serial.flush();
    announcedReady = true;
    lastReadyMs = millis();
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
    udp.begin(UDP_TARGET_PORT);

    pcmSeen = false;
    pcmFrames = 0u;
    sendReady();
    sendHeartbeat();
}

void setup()
{
    Serial.setRxBufferSize(2048);
    Serial.begin(UART_BAUD);
    Serial.setDebugOutput(false);
    delay(100);
    connectWifi();
}

void loop()
{
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
