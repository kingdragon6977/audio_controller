#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#define WIFI_SSID        "your-ssid"
#define WIFI_PASSWORD    "your-password"

/* PC running the UDP receiver. 255.255.255.255 can be used for LAN broadcast. */
#define UDP_TARGET_IP    "255.255.255.255"
#define UDP_TARGET_PORT  5004

/* ESP8266 ArduinoOTA. Keep hostname in sync with the PlatformIO OTA environment. */
#define OTA_HOSTNAME     "audio-esp01"
/* Strongly recommended on any network you do not fully trust. Empty = no OTA password. */
#define OTA_PASSWORD     ""

#endif
