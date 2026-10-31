#pragma once
#include <vector>
#include <stdint.h>
#include <stddef.h>
#include "ESP8266WiFi.h"
extern std::vector<int> events;
class WiFiUDP {
public:
    std::vector<uint8_t> last;
    void beginPacket(const IPAddress &, uint16_t) { last.clear(); }
    void write(const uint8_t *p, size_t n) { last.insert(last.end(),p,p+n); }
    void endPacket() { events.push_back(1); }
};
