#include "stm32_update_transport.h"

#include <string.h>

namespace {

static const uint8_t MAGIC[4] = {'S','3','2','U'};
static const uint8_t VERSION = 1u;
static const uint16_t PAGE_SIZE = 2048u;
static const uint16_t CHUNK_SIZE = 256u;
static const uint16_t MAX_PAGES = 128u;

enum PacketType {
    PKT_MANIFEST_BEGIN = 1,
    PKT_PAGE_HASH      = 2,
    PKT_MANIFEST_COMMIT= 3,
    PKT_PAGE_BEGIN     = 4,
    PKT_PAGE_DATA      = 5,
    PKT_PAGE_SEAL      = 6,
    PKT_ABORT          = 7,
    PKT_STATUS         = 8
};

enum ReplyCode {
    REPLY_OK = 0,
    REPLY_BAD_PACKET = 1,
    REPLY_BAD_STATE = 2,
    REPLY_BAD_SESSION = 3,
    REPLY_BAD_RANGE = 4,
    REPLY_BAD_HASH = 5,
    REPLY_INCOMPLETE = 6
};

struct Sha256Ctx {
    uint32_t state[8];
    uint64_t bitCount;
    uint8_t buffer[64];
    size_t used;
};

struct SessionState {
    bool active;
    bool manifestCommitted;
    uint32_t session;
    uint32_t imageSize;
    uint16_t pageCount;
    uint16_t hashesReceived;
    uint8_t imageHash[32];
    uint8_t manifestHash[32];
    uint8_t pageHashes[MAX_PAGES][32];
    bool pageHashPresent[MAX_PAGES];

    bool pageActive;
    uint16_t pageIndex;
    uint16_t pageLength;
    uint16_t pageReceived;
    uint8_t pageBuffer[PAGE_SIZE];
};

static SessionState st;

static uint32_t rotr32(uint32_t x, uint32_t n)
{
    return (x >> n) | (x << (32u - n));
}

static uint32_t readBe32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static uint16_t readLe16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t readLe32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void writeLe16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void writeLe32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void sha256Transform(Sha256Ctx &ctx, const uint8_t block[64])
{
    static const uint32_t k[64] = {
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,
        0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
        0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,
        0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
        0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,
        0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,
        0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
        0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,
        0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
        0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,
        0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
        0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,
        0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
        0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,
        0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
    };

    uint32_t w[64];
    for (unsigned i = 0; i < 16; ++i)
        w[i] = readBe32(block + i * 4u);
    for (unsigned i = 16; i < 64; ++i) {
        uint32_t s0 = rotr32(w[i-15],7) ^ rotr32(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr32(w[i-2],17) ^ rotr32(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    uint32_t a=ctx.state[0], b=ctx.state[1], c=ctx.state[2], d=ctx.state[3];
    uint32_t e=ctx.state[4], f=ctx.state[5], g=ctx.state[6], h=ctx.state[7];

    for (unsigned i = 0; i < 64; ++i) {
        uint32_t s1 = rotr32(e,6) ^ rotr32(e,11) ^ rotr32(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + s1 + ch + k[i] + w[i];
        uint32_t s0 = rotr32(a,2) ^ rotr32(a,13) ^ rotr32(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }

    ctx.state[0]+=a; ctx.state[1]+=b; ctx.state[2]+=c; ctx.state[3]+=d;
    ctx.state[4]+=e; ctx.state[5]+=f; ctx.state[6]+=g; ctx.state[7]+=h;
}

static void sha256Init(Sha256Ctx &ctx)
{
    static const uint32_t initial[8] = {
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u
    };
    memcpy(ctx.state, initial, sizeof(initial));
    ctx.bitCount = 0;
    ctx.used = 0;
}

static void sha256Update(Sha256Ctx &ctx, const uint8_t *data, size_t len)
{
    ctx.bitCount += (uint64_t)len * 8u;
    while (len) {
        size_t take = 64u - ctx.used;
        if (take > len) take = len;
        memcpy(ctx.buffer + ctx.used, data, take);
        ctx.used += take;
        data += take;
        len -= take;
        if (ctx.used == 64u) {
            sha256Transform(ctx, ctx.buffer);
            ctx.used = 0;
        }
    }
}

static void sha256Final(Sha256Ctx &ctx, uint8_t out[32])
{
    ctx.buffer[ctx.used++] = 0x80u;
    if (ctx.used > 56u) {
        while (ctx.used < 64u) ctx.buffer[ctx.used++] = 0;
        sha256Transform(ctx, ctx.buffer);
        ctx.used = 0;
    }
    while (ctx.used < 56u) ctx.buffer[ctx.used++] = 0;

    for (int i = 7; i >= 0; --i)
        ctx.buffer[ctx.used++] = (uint8_t)(ctx.bitCount >> (i * 8));
    sha256Transform(ctx, ctx.buffer);

    for (unsigned i = 0; i < 8; ++i) {
        out[i*4+0] = (uint8_t)(ctx.state[i] >> 24);
        out[i*4+1] = (uint8_t)(ctx.state[i] >> 16);
        out[i*4+2] = (uint8_t)(ctx.state[i] >> 8);
        out[i*4+3] = (uint8_t)ctx.state[i];
    }
}

static void sha256(const uint8_t *data, size_t len, uint8_t out[32])
{
    Sha256Ctx ctx;
    sha256Init(ctx);
    sha256Update(ctx, data, len);
    sha256Final(ctx, out);
}

static void resetState()
{
    memset(&st, 0, sizeof(st));
}

static void reply(WiFiUDP &sock, const IPAddress &ip, uint16_t port,
                  uint8_t requestType, ReplyCode code,
                  uint32_t session, uint16_t arg)
{
    uint8_t out[18];
    memcpy(out, MAGIC, 4);
    out[4] = VERSION;
    out[5] = PKT_STATUS;
    out[6] = requestType;
    out[7] = (uint8_t)code;
    writeLe32(out + 8, session);
    writeLe16(out + 12, arg);
    writeLe16(out + 14, st.hashesReceived);
    writeLe16(out + 16, st.pageReceived);
    sock.beginPacket(ip, port);
    sock.write(out, sizeof(out));
    sock.endPacket();
}

static bool sameHash(const uint8_t a[32], const uint8_t b[32])
{
    uint8_t diff = 0;
    for (unsigned i = 0; i < 32; ++i)
        diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0;
}

} // namespace

void stm32UpdateTransportReset()
{
    resetState();
}

bool stm32UpdateTransportHandlePacket(
    const uint8_t *packet,
    size_t packetLen,
    WiFiUDP &replySocket,
    const IPAddress &replyIp,
    uint16_t replyPort)
{
    if (packetLen < 12u || memcmp(packet, MAGIC, 4) != 0 || packet[4] != VERSION)
        return false;

    const uint8_t type = packet[5];
    const uint32_t session = readLe32(packet + 8);

    if (type == PKT_ABORT) {
        resetState();
        reply(replySocket, replyIp, replyPort, type, REPLY_OK, session, 0);
        return true;
    }

    if (type == PKT_MANIFEST_BEGIN) {
        // header(12) + image_size(4) + page_count(2) + reserved(2) + hashes(64)
        if (packetLen != 84u) {
            reply(replySocket, replyIp, replyPort, type, REPLY_BAD_PACKET, session, 0);
            return true;
        }

        const uint32_t imageSize = readLe32(packet + 12);
        const uint16_t pageCount = readLe16(packet + 16);
        if (session == 0u || imageSize == 0u || imageSize > 256u * 1024u ||
            pageCount == 0u || pageCount > MAX_PAGES ||
            pageCount != (uint16_t)((imageSize + PAGE_SIZE - 1u) / PAGE_SIZE)) {
            reply(replySocket, replyIp, replyPort, type, REPLY_BAD_RANGE, session, 0);
            return true;
        }

        resetState();
        st.active = true;
        st.session = session;
        st.imageSize = imageSize;
        st.pageCount = pageCount;
        memcpy(st.imageHash, packet + 20, 32);
        memcpy(st.manifestHash, packet + 52, 32);
        reply(replySocket, replyIp, replyPort, type, REPLY_OK, session, pageCount);
        return true;
    }

    if (!st.active) {
        reply(replySocket, replyIp, replyPort, type, REPLY_BAD_STATE, session, 0);
        return true;
    }
    if (session != st.session) {
        reply(replySocket, replyIp, replyPort, type, REPLY_BAD_SESSION, session, 0);
        return true;
    }

    if (type == PKT_PAGE_HASH) {
        if (st.manifestCommitted || packetLen != 46u) {
            reply(replySocket, replyIp, replyPort, type, REPLY_BAD_STATE, session, 0);
            return true;
        }
        const uint16_t index = readLe16(packet + 12);
        if (index >= st.pageCount) {
            reply(replySocket, replyIp, replyPort, type, REPLY_BAD_RANGE, session, index);
            return true;
        }
        if (!st.pageHashPresent[index]) {
            memcpy(st.pageHashes[index], packet + 14, 32);
            st.pageHashPresent[index] = true;
            st.hashesReceived++;
        } else if (memcmp(st.pageHashes[index], packet + 14, 32) != 0) {
            reply(replySocket, replyIp, replyPort, type, REPLY_BAD_HASH, session, index);
            return true;
        }
        reply(replySocket, replyIp, replyPort, type, REPLY_OK, session, index);
        return true;
    }

    if (type == PKT_MANIFEST_COMMIT) {
        if (packetLen != 12u || st.hashesReceived != st.pageCount) {
            reply(replySocket, replyIp, replyPort, type, REPLY_INCOMPLETE, session, 0);
            return true;
        }

        Sha256Ctx manifestCtx;
        uint8_t computedManifestHash[32];
        uint8_t meta[6];
        writeLe32(meta, st.imageSize);
        writeLe16(meta + 4, st.pageCount);

        sha256Init(manifestCtx);
        sha256Update(manifestCtx, meta, sizeof(meta));
        sha256Update(manifestCtx, st.imageHash, sizeof(st.imageHash));
        for (uint16_t i = 0u; i < st.pageCount; ++i)
            sha256Update(manifestCtx, st.pageHashes[i], 32u);
        sha256Final(manifestCtx, computedManifestHash);

        if (!sameHash(computedManifestHash, st.manifestHash)) {
            reply(replySocket, replyIp, replyPort, type, REPLY_BAD_HASH, session, 0);
            return true;
        }

        st.manifestCommitted = true;
        reply(replySocket, replyIp, replyPort, type, REPLY_OK, session, st.pageCount);
        return true;
    }

    if (!st.manifestCommitted) {
        reply(replySocket, replyIp, replyPort, type, REPLY_BAD_STATE, session, 0);
        return true;
    }

    if (type == PKT_PAGE_BEGIN) {
        if (packetLen != 16u || st.pageActive) {
            reply(replySocket, replyIp, replyPort, type, REPLY_BAD_STATE, session, 0);
            return true;
        }
        const uint16_t index = readLe16(packet + 12);
        const uint16_t length = readLe16(packet + 14);
        const uint16_t expectedLength =
            (index + 1u == st.pageCount)
                ? (uint16_t)(st.imageSize - (uint32_t)index * PAGE_SIZE)
                : PAGE_SIZE;

        if (index >= st.pageCount || length == 0u || length != expectedLength) {
            reply(replySocket, replyIp, replyPort, type, REPLY_BAD_RANGE, session, index);
            return true;
        }

        st.pageActive = true;
        st.pageIndex = index;
        st.pageLength = length;
        st.pageReceived = 0u;
        reply(replySocket, replyIp, replyPort, type, REPLY_OK, session, index);
        return true;
    }

    if (type == PKT_PAGE_DATA) {
        if (!st.pageActive || packetLen < 16u) {
            reply(replySocket, replyIp, replyPort, type, REPLY_BAD_STATE, session, 0);
            return true;
        }
        const uint16_t index = readLe16(packet + 12);
        const uint16_t offset = readLe16(packet + 14);
        const size_t dataLen = packetLen - 16u;

        if (index != st.pageIndex || offset != st.pageReceived ||
            dataLen == 0u || dataLen > CHUNK_SIZE ||
            (uint32_t)offset + dataLen > st.pageLength) {
            reply(replySocket, replyIp, replyPort, type, REPLY_BAD_RANGE, session, index);
            return true;
        }

        memcpy(st.pageBuffer + offset, packet + 16, dataLen);
        st.pageReceived = (uint16_t)(st.pageReceived + dataLen);
        reply(replySocket, replyIp, replyPort, type, REPLY_OK, session, index);
        return true;
    }

    if (type == PKT_PAGE_SEAL) {
        if (!st.pageActive || packetLen != 14u) {
            reply(replySocket, replyIp, replyPort, type, REPLY_BAD_STATE, session, 0);
            return true;
        }
        const uint16_t index = readLe16(packet + 12);
        if (index != st.pageIndex || st.pageReceived != st.pageLength) {
            reply(replySocket, replyIp, replyPort, type, REPLY_INCOMPLETE, session, index);
            return true;
        }

        uint8_t actual[32];
        sha256(st.pageBuffer, st.pageLength, actual);
        if (!sameHash(actual, st.pageHashes[index])) {
            st.pageActive = false;
            st.pageReceived = 0u;
            reply(replySocket, replyIp, replyPort, type, REPLY_BAD_HASH, session, index);
            return true;
        }

        /*
         * Safety boundary: verified data stops here.
         * There is intentionally no erase/write call in this module.
         */
        st.pageActive = false;
        st.pageReceived = 0u;
        reply(replySocket, replyIp, replyPort, type, REPLY_OK, session, index);
        return true;
    }

    reply(replySocket, replyIp, replyPort, type, REPLY_BAD_PACKET, session, 0);
    return true;
}
