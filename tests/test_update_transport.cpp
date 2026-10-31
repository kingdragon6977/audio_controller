#include <assert.h>
#include <vector>
#include <stdio.h>
#include "../esp01_stream/src/stm32_update_transport.cpp"
std::vector<int> events;
static bool healthy;
static bool readOk = true;
static const uint8_t image[8] = {0,192,0,32,1,0,0,8};
static bool reader(uint32_t address, uint8_t *out, size_t n, void *)
{
    if (!readOk) return false;
    memcpy(out,image + address - FLASH_BASE,n); return true;
}
static void ended(bool success, void *) { assert(success); events.push_back(2); }
static bool recovered(void *) { return healthy; }
static bool denied(uint32_t,uint32_t,uint16_t,const uint8_t *,void *) { return false; }
static bool page(uint16_t,uint32_t,const uint8_t *,uint16_t,void *) { return true; }
static void setup_session()
{
    resetState(); st.active=true; st.manifestCommitted=true; st.session=7;
    st.imageSize=sizeof(image); st.pageCount=1; st.flashedPages=1; st.flashActive=true;
    sha256(image,sizeof(image),st.imageHash); events.clear(); readOk=true; healthy=false;
}
static int request(uint8_t type, WiFiUDP &udp, uint32_t session=7)
{
    uint8_t p[12]={'S','3','2','U',1,type,0,0,0,0,0,0};
    writeLe32(p+8,session);
    assert(stm32UpdateTransportHandlePacket(p,sizeof(p),udp,IPAddress{},5004));
    return udp.last[7];
}
int main()
{
    WiFiUDP udp;
    stm32UpdateTransportSetFlashCallbacks(denied,page,reader,ended,NULL);
    stm32UpdateTransportSetRecoveryCallback(recovered);
    setup_session();
    healthy=true;
    assert(request(PKT_APPLICATION_STATUS,udp)==REPLY_INCOMPLETE);
    healthy=false; events.clear();
    assert(request(PKT_FLASH_FINISH,udp)==REPLY_OK);
    assert((events == std::vector<int>{1,2})); /* ACK precedes reset */
    assert(request(PKT_FLASH_FINISH,udp)==REPLY_OK);
    assert(events.size()==3); /* lost ACK retry does not reset again */
    assert(request(PKT_APPLICATION_STATUS,udp)==REPLY_INCOMPLETE);
    healthy=true;
    assert(request(PKT_APPLICATION_STATUS,udp)==REPLY_OK);
    assert(request(PKT_APPLICATION_STATUS,udp,8)==REPLY_BAD_SESSION);
    setup_session(); st.imageHash[0]^=1;
    assert(request(PKT_FLASH_FINISH,udp)==REPLY_BAD_HASH);
    assert(!st.flashCompleted && events.size()==1);
    setup_session(); readOk=false;
    assert(request(PKT_FLASH_FINISH,udp)==REPLY_VERIFY_FAILED);
    assert(!st.flashCompleted && events.size()==1);
    setup_session(); st.flashActive=false;
    assert(request(PKT_FLASH_BEGIN,udp)==REPLY_FLASH_FAILED);
    assert(!st.flashActive);
    puts("update transport tests PASS");
}
