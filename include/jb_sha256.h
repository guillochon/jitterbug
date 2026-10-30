/* jb_sha256.h - SHA-256 and HMAC-SHA-256 (FIPS 180-4, RFC 2104). */
#ifndef JB_SHA256_H
#define JB_SHA256_H
#include "jb_types.h"

#define JB_SHA256_LEN 32

typedef struct {
    jb_u32 h[8];
    jb_u32 len_hi, len_lo;   /* message length in bytes, 64-bit split */
    jb_u8  buf[64];
    jb_u32 buflen;
} jb_sha256_ctx;

void jb_sha256_init(jb_sha256_ctx *c);
void jb_sha256_update(jb_sha256_ctx *c, const void *data, size_t len);
void jb_sha256_final(jb_sha256_ctx *c, jb_u8 out[JB_SHA256_LEN]);
void jb_sha256(const void *data, size_t len, jb_u8 out[JB_SHA256_LEN]);

typedef struct {
    jb_sha256_ctx inner, outer;
} jb_hmac_ctx;

void jb_hmac_init(jb_hmac_ctx *c, const jb_u8 *key, size_t keylen);
void jb_hmac_update(jb_hmac_ctx *c, const void *data, size_t len);
void jb_hmac_final(jb_hmac_ctx *c, jb_u8 out[JB_SHA256_LEN]);

/* Overwrite memory in a way the compiler should not elide. */
void jb_wipe(void *p, size_t n);

#endif
