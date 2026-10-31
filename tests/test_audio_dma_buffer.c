#include "audio_dma_buffer.h"
#include <assert.h>
#include <stdio.h>
static uint32_t before, after;
static int reads;
static uint32_t remaining(void) { return reads++ == 0 ? before : after; }
static int snapshot(unsigned first, uint32_t a, uint32_t b, uint16_t *out, uint16_t *buffer)
{ before=a; after=b; reads=0; return audio_dma_snapshot(out,buffer,512,first,remaining); }
int main(void)
{
    uint16_t buffer[1024], out[512]; unsigned i;
    for (i=0;i<1024;++i) buffer[i]=(uint16_t)i;
    assert(snapshot(0,512,500,out,buffer));
    for(i=0;i<512;++i)assert(out[i]==i);
    assert(snapshot(512,1024,1000,out,buffer));
    for(i=0;i<512;++i)assert(out[i]==i+512);
    assert(!snapshot(0,1000,999,out,buffer)); /* half still being filled */
    assert(!snapshot(0,1,1024,out,buffer)); /* DMA crossed into copied half */
    assert(!snapshot(512,513,512,out,buffer));
    assert(!snapshot(0,1025,1000,out,buffer));
    assert(!snapshot(1,512,500,out,buffer));
    puts("DMA snapshot boundary tests PASS");
}
