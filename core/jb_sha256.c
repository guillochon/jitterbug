/* jb_sha256.c - straightforward FIPS 180-4 SHA-256 + RFC 2104 HMAC. C89. */
#include <string.h>
#include "jb_sha256.h"

static const jb_u32 K[64] = {
    0x428a2f98UL,0x71374491UL,0xb5c0fbcfUL,0xe9b5dba5UL,0x3956c25bUL,0x59f111f1UL,0x923f82a4UL,0xab1c5ed5UL,
    0xd807aa98UL,0x12835b01UL,0x243185beUL,0x550c7dc3UL,0x72be5d74UL,0x80deb1feUL,0x9bdc06a7UL,0xc19bf174UL,
    0xe49b69c1UL,0xefbe4786UL,0x0fc19dc6UL,0x240ca1ccUL,0x2de92c6fUL,0x4a7484aaUL,0x5cb0a9dcUL,0x76f988daUL,
    0x983e5152UL,0xa831c66dUL,0xb00327c8UL,0xbf597fc7UL,0xc6e00bf3UL,0xd5a79147UL,0x06ca6351UL,0x14292967UL,
    0x27b70a85UL,0x2e1b2138UL,0x4d2c6dfcUL,0x53380d13UL,0x650a7354UL,0x766a0abbUL,0x81c2c92eUL,0x92722c85UL,
    0xa2bfe8a1UL,0xa81a664bUL,0xc24b8b70UL,0xc76c51a3UL,0xd192e819UL,0xd6990624UL,0xf40e3585UL,0x106aa070UL,
    0x19a4c116UL,0x1e376c08UL,0x2748774cUL,0x34b0bcb5UL,0x391c0cb3UL,0x4ed8aa4aUL,0x5b9cca4fUL,0x682e6ff3UL,
    0x748f82eeUL,0x78a5636fUL,0x84c87814UL,0x8cc70208UL,0x90befffaUL,0xa4506cebUL,0xbef9a3f7UL,0xc67178f2UL
};

#define ROR(x,n) ((jb_u32)(((x) >> (n)) | ((x) << (32 - (n)))))

static void compress(jb_u32 h[8], const jb_u8 blk[64])
{
    jb_u32 w[64], a, b, c, d, e, f, g, hh, t1, t2;
    int i;
    for (i = 0; i < 16; i++) w[i] = JB_LOAD32BE(blk + 4 * i);
    for (i = 16; i < 64; i++) {
        jb_u32 s0 = ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3);
        jb_u32 s1 = ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = (jb_u32)(w[i-16] + s0 + w[i-7] + s1);
    }
    a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; hh = h[7];
    for (i = 0; i < 64; i++) {
        t1 = (jb_u32)(hh + (ROR(e,6) ^ ROR(e,11) ^ ROR(e,25)) + ((e & f) ^ (~e & g)) + K[i] + w[i]);
        t2 = (jb_u32)((ROR(a,2) ^ ROR(a,13) ^ ROR(a,22)) + ((a & b) ^ (a & c) ^ (b & c)));
        hh = g; g = f; f = e; e = (jb_u32)(d + t1); d = c; c = b; b = a; a = (jb_u32)(t1 + t2);
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    jb_wipe(w, sizeof w);
}

void jb_sha256_init(jb_sha256_ctx *c)
{
    c->h[0] = 0x6a09e667UL; c->h[1] = 0xbb67ae85UL; c->h[2] = 0x3c6ef372UL; c->h[3] = 0xa54ff53aUL;
    c->h[4] = 0x510e527fUL; c->h[5] = 0x9b05688cUL; c->h[6] = 0x1f83d9abUL; c->h[7] = 0x5be0cd19UL;
    c->len_hi = c->len_lo = 0;
    c->buflen = 0;
}

void jb_sha256_update(jb_sha256_ctx *c, const void *data, size_t len)
{
    const jb_u8 *p = (const jb_u8 *)data;
    while (len > 0) {
        size_t take = 64 - c->buflen;
        if (take > len) take = len;
        memcpy(c->buf + c->buflen, p, take);
        c->buflen += (jb_u32)take;
        p += take; len -= take;
        c->len_lo = (jb_u32)(c->len_lo + take);
        if (c->len_lo < take) c->len_hi++;
        if (c->buflen == 64) { compress(c->h, c->buf); c->buflen = 0; }
    }
}

void jb_sha256_final(jb_sha256_ctx *c, jb_u8 out[JB_SHA256_LEN])
{
    jb_u8 pad[72];
    jb_u32 bits_hi = (jb_u32)((c->len_hi << 3) | (c->len_lo >> 29));
    jb_u32 bits_lo = (jb_u32)(c->len_lo << 3);
    size_t padlen = (c->buflen < 56) ? (56 - c->buflen) : (120 - c->buflen);
    int i;
    memset(pad, 0, sizeof pad);
    pad[0] = 0x80;
    JB_STORE32BE(pad + padlen, bits_hi);
    JB_STORE32BE(pad + padlen + 4, bits_lo);
    jb_sha256_update(c, pad, padlen + 8);
    for (i = 0; i < 8; i++) JB_STORE32BE(out + 4 * i, c->h[i]);
    jb_wipe(c, sizeof *c);
}

void jb_sha256(const void *data, size_t len, jb_u8 out[JB_SHA256_LEN])
{
    jb_sha256_ctx c;
    jb_sha256_init(&c);
    jb_sha256_update(&c, data, len);
    jb_sha256_final(&c, out);
}

void jb_hmac_init(jb_hmac_ctx *c, const jb_u8 *key, size_t keylen)
{
    jb_u8 k[64], pad[64];
    int i;
    memset(k, 0, sizeof k);
    if (keylen > 64) jb_sha256(key, keylen, k);
    else if (keylen) memcpy(k, key, keylen);
    for (i = 0; i < 64; i++) pad[i] = (jb_u8)(k[i] ^ 0x36);
    jb_sha256_init(&c->inner); jb_sha256_update(&c->inner, pad, 64);
    for (i = 0; i < 64; i++) pad[i] = (jb_u8)(k[i] ^ 0x5c);
    jb_sha256_init(&c->outer); jb_sha256_update(&c->outer, pad, 64);
    jb_wipe(k, sizeof k); jb_wipe(pad, sizeof pad);
}

void jb_hmac_update(jb_hmac_ctx *c, const void *data, size_t len)
{
    jb_sha256_update(&c->inner, data, len);
}

void jb_hmac_final(jb_hmac_ctx *c, jb_u8 out[JB_SHA256_LEN])
{
    jb_u8 ih[JB_SHA256_LEN];
    jb_sha256_final(&c->inner, ih);
    jb_sha256_update(&c->outer, ih, sizeof ih);
    jb_sha256_final(&c->outer, out);
    jb_wipe(ih, sizeof ih);
}

void jb_wipe(void *p, size_t n)
{
    volatile jb_u8 *v = (volatile jb_u8 *)p;
    while (n--) *v++ = 0;
}
