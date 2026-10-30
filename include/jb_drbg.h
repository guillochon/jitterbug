/* jb_drbg.h - HMAC_DRBG with SHA-256 (NIST SP 800-90A Rev.1, sec 10.1.2).
 * No prediction resistance; reseeding is driven by the engine. */
#ifndef JB_DRBG_H
#define JB_DRBG_H
#include "jb_sha256.h"

/* Far below the SP 800-90A limit of 2^48; we reseed often anyway. */
#define JB_DRBG_MAX_REQUESTS 0x00100000UL
#define JB_DRBG_MAX_BYTES_PER_REQUEST 65536UL

typedef struct {
    jb_u8  K[JB_SHA256_LEN];
    jb_u8  V[JB_SHA256_LEN];
    jb_u32 reseed_counter;
    int    instantiated;
} jb_drbg;

void jb_drbg_instantiate(jb_drbg *d, const void *seed, size_t seedlen,
                         const void *pers, size_t perslen);
void jb_drbg_reseed(jb_drbg *d, const void *seed, size_t seedlen,
                    const void *add, size_t addlen);
int  jb_drbg_generate(jb_drbg *d, void *out, size_t outlen,
                      const void *add, size_t addlen);
void jb_drbg_wipe(jb_drbg *d);

#endif
