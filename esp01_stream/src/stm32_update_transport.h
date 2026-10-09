#ifndef STM32_UPDATE_TRANSPORT_H
#define STM32_UPDATE_TRANSPORT_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

/*
 * Non-destructive STM32 update staging protocol.
 *
 * This layer can receive/validate an immutable manifest and one page of image
 * data. It intentionally has no dependency on the STM32 ROM erase/write code.
 */
typedef bool (*Stm32UpdateReadCallback)(
    uint32_t address,
    uint8_t *data,
    size_t length,
    void *context);

typedef enum {
    STM32_VERIFY_NO_MANIFEST = 0,
    STM32_VERIFY_READ_FAILED = 1,
    STM32_VERIFY_HASH_MISMATCH = 2,
    STM32_VERIFY_OK = 3
} Stm32UpdateVerifyResult;

typedef bool (*Stm32UpdateFlashBeginCallback)(
    uint32_t session,
    uint32_t imageSize,
    uint16_t pageCount,
    void *context);

typedef bool (*Stm32UpdateFlashPageCallback)(
    uint16_t pageIndex,
    uint32_t address,
    const uint8_t *data,
    uint16_t length,
    void *context);

typedef void (*Stm32UpdateFlashEndCallback)(
    bool success,
    void *context);

void stm32UpdateTransportReset();

void stm32UpdateTransportSetFlashCallbacks(
    Stm32UpdateFlashBeginCallback beginCallback,
    Stm32UpdateFlashPageCallback pageCallback,
    Stm32UpdateReadCallback readCallback,
    Stm32UpdateFlashEndCallback endCallback,
    void *context);

/*
 * Read-only verification against the currently committed frozen manifest.
 * The transport owns the expected image size/hash. The caller supplies only
 * a fixed ROM-read callback; no erase/write capability is exposed here.
 */
Stm32UpdateVerifyResult stm32UpdateTransportVerifyCommittedImage(
    Stm32UpdateReadCallback reader,
    void *context,
    uint32_t *imageSizeOut,
    uint8_t actualHashOut[32],
    uint8_t expectedHashOut[32]);

bool stm32UpdateTransportHandlePacket(
    const uint8_t *packet,
    size_t packetLen,
    WiFiUDP &replySocket,
    const IPAddress &replyIp,
    uint16_t replyPort);

#endif
