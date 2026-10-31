#include "flash_authorization.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    FlashAuthorization s = {0};
    uint8_t p[FLASH_AUTH_BYTES] = {1,0,0,0, 8,0,0,0};
    uint16_t crc = flash_auth_crc16(p);
    assert(crc == 0x3154u); /* independent CRC-CCITT reference */
    p[20] ^= 1;
    assert(flash_auth_crc16(p) != crc);
    p[20] ^= 1;
    assert(flash_authorization_request(&s,p,100));
    assert(flash_authorization_request(&s,p,400));
    assert(s.started == 100); /* recheck cannot renew authorization */
    p[8] ^= 1;
    assert(!flash_authorization_request(&s,p,500)); /* changed image */
    p[8] ^= 1;
    p[0] = 2;
    assert(!flash_authorization_request(&s,p,500)); /* changed session */
    p[0] = 1;
    p[4] = 9;
    assert(!flash_authorization_request(&s,p,500)); /* changed length */
    p[4] = 8;
    assert(!flash_authorization_expired(&s,2099));
    assert(flash_authorization_expired(&s,2100));
    assert(!flash_authorization_request(&s,p,2100));
    flash_authorization_clear(&s);
    assert(flash_authorization_request(&s,p,0xfffffff0u));
    assert(!flash_authorization_expired(&s,0x10));
    assert(flash_authorization_expired(&s,1984)); /* timer rollover */
    flash_authorization_clear(&s);
    memset(p,0,sizeof(p));
    assert(!flash_authorization_request(&s,p,0));
    p[0]=1; p[4]=7;
    assert(!flash_authorization_request(&s,p,0));
    p[4]=0; p[6]=5;
    assert(!flash_authorization_request(&s,p,0)); /* oversized image */
    puts("flash authorization tests PASS");
    return 0;
}
