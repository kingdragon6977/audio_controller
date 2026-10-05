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
void stm32UpdateTransportReset();
bool stm32UpdateTransportHandlePacket(
    const uint8_t *packet,
    size_t packetLen,
    WiFiUDP &replySocket,
    const IPAddress &replyIp,
    uint16_t replyPort);

#endif
