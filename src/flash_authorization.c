#include "flash_authorization.h"
#include <string.h>
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
int flash_authorization_expired(const FlashAuthorization *s, uint32_t now)
{
    return s->active && (uint32_t)(now - s->started) >= FLASH_AUTH_LEASE_MS;
}
void flash_authorization_clear(FlashAuthorization *s)
{
    s->active = 0u;
}
int flash_authorization_request(FlashAuthorization *s, const uint8_t *p, uint32_t now)
{
    uint32_t size = le32(p + 4);
    if (!le32(p) || size < 8u || size > 256u * 1024u)
        return 0;
    if (s->active) {
        /* The settled recheck cannot extend the lease or change the image. */
        return !flash_authorization_expired(s, now) &&
               memcmp(s->request, p, FLASH_AUTH_BYTES) == 0;
    }
    memcpy(s->request, p, FLASH_AUTH_BYTES);
    s->started = now;
    s->active = 1u;
    return 1;
}
