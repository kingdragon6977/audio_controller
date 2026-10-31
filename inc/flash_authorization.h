#ifndef FLASH_AUTHORIZATION_H
#define FLASH_AUTHORIZATION_H
#include <stdint.h>
#define FLASH_AUTH_BYTES 40u
#define FLASH_AUTH_LEASE_MS 2000u
static inline uint16_t flash_auth_crc16(const uint8_t *p)
{
    uint16_t crc = 0xffffu;
    unsigned int i, bit;
    for (i = 0; i < FLASH_AUTH_BYTES; ++i) {
        crc ^= (uint16_t)p[i] << 8;
        for (bit = 0; bit < 8u; ++bit)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000u) ? 0x1021u : 0u));
    }
    return crc;
}
typedef struct {
    uint8_t request[FLASH_AUTH_BYTES];
    uint32_t started;
    uint8_t active;
} FlashAuthorization;
/* Request: LE session, LE image size, canonical manifest SHA-256.
 * Binding/checksums prevent accidental mixups; they are not authentication. */
int flash_authorization_request(FlashAuthorization *state,
                                const uint8_t *request, uint32_t now);
int flash_authorization_expired(const FlashAuthorization *state, uint32_t now);
void flash_authorization_clear(FlashAuthorization *state);
#endif
