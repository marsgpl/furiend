#include "sha2.h"

LUAMOD_API int luaopen_sha2(lua_State *L) {
    luaL_newlib(L, sha2_index);
    return 1;
}

static int sha512_hash(const uint8_t *input, size_t len, uint64_t *hash) {
    // padding

    uint64_t input_bits_n = len * 8;
    uint64_t zero_bits_n = 1024 - ((input_bits_n + 1 + 128) % 1024);
    uint64_t msg_bits_n = input_bits_n + 1 + zero_bits_n + 128;
    uint64_t msg_len = msg_bits_n / 8;

    char *msg = calloc(msg_len, 1); // zerofilled

    if (msg == NULL) {
        return 0; // fail
    }

    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        input_bits_n = FLIP64(input_bits_n);
    #endif

    memcpy(msg, input, len); // orig input
    msg[len] = 0b10000000; // single '1' bit
    memcpy(msg + msg_len - 8, &input_bits_n, 8); // input len

    // init hash

    memcpy(hash, IHV, 64);

    // process

    uint64_t chunks_n = msg_len / 128;

    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        uint64_t *w64 = (uint64_t *)msg;
        for (uint64_t i = 0; i < chunks_n * 16; ++i) {
            w64[i] = FLIP64(w64[i]);
        }
    #endif

    for (uint64_t ci = 0; ci < chunks_n; ++ci) {
        uint8_t *chunk = (uint8_t *)(msg + ci * 128);
        static uint64_t w[80];

        memcpy(w, chunk, 128); // w[0..15]

        for (int wi = 16; wi < 80; ++wi) {
            uint64_t s0 = SIGMA0(w[wi - 15]);
            uint64_t s1 = SIGMA1(w[wi - 2]);
            w[wi] = w[wi - 16] + s0 + w[wi - 7] + s1;
        }

        uint64_t a = hash[0];
        uint64_t b = hash[1];
        uint64_t c = hash[2];
        uint64_t d = hash[3];
        uint64_t e = hash[4];
        uint64_t f = hash[5];
        uint64_t g = hash[6];
        uint64_t h = hash[7];

        // compression main loop

        for (int wi = 0; wi < 80; ++wi) {
            uint64_t s1 = SUM1(e);
            uint64_t ch = CH(e, f, g);
            uint64_t tmp1 = h + s1 + ch + RC[wi] + w[wi];
            uint64_t s0 = SUM0(a);
            uint64_t maj = MAJ(a, b, c);
            uint64_t tmp2 = s0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + tmp1;
            d = c;
            c = b;
            b = a;
            a = tmp1 + tmp2;
        }

        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
        hash[5] += f;
        hash[6] += g;
        hash[7] += h;
    }

    free(msg);

    return 1; // ok
}

int sha512(lua_State *L) {
    luaF_need_args(L, 1, "sha512");
    luaL_checktype(L, 1, LUA_TSTRING); // phrase to hash

    size_t len;
    const char *input = lua_tolstring(L, 1, &len);

    if (len > 18446744073709551615ULL / 8) {
        luaL_error(L, "input is too long");
    }

    static uint64_t hash[8];
    static char hex[128 + 1]; // 1 for sprintf nul byte

    int is_ok = sha512_hash((const uint8_t *)input, len, hash);

    if (!is_ok) {
        luaF_error_errno(L, "sha512 calloc failed");
    }

    for (int i = 0; i < 8; ++i) {
        sprintf(hex + i * 16, "%016" PRIx64, hash[i]);
    }

    lua_pushlstring(L, hex, 128);

    return 1;
}
