/* jb_drbg.c - HMAC_DRBG (SHA-256). C89.
 * Cross-checked against tests/ref_hmac_drbg.py; TODO: add NIST CAVP vectors. */
#include <string.h>
#include "jb_drbg.h"

static void drbg_update(jb_drbg *d, const void *a, size_t alen,
                        const void *b, size_t blen)
{
    jb_hmac_ctx h;
    jb_u8 sep;
    int round;
    for (round = 0; round < 2; round++) {
        sep = (jb_u8)round;
        jb_hmac_init(&h, d->K, sizeof d->K);
        jb_hmac_update(&h, d->V, sizeof d->V);
        jb_hmac_update(&h, &sep, 1);
        if (alen) jb_hmac_update(&h, a, alen);
        if (blen) jb_hmac_update(&h, b, blen);
        jb_hmac_final(&h, d->K);
        jb_hmac_init(&h, d->K, sizeof d->K);
        jb_hmac_update(&h, d->V, sizeof d->V);
        jb_hmac_final(&h, d->V);
        if (alen + blen == 0) break;   /* provided_data empty: one round */
    }
}

void jb_drbg_instantiate(jb_drbg *d, const void *seed, size_t seedlen,
                         const void *pers, size_t perslen)
{
    memset(d->K, 0x00, sizeof d->K);
    memset(d->V, 0x01, sizeof d->V);
    drbg_update(d, seed, seedlen, pers, perslen);
    d->reseed_counter = 1;
    d->instantiated = 1;
}

void jb_drbg_reseed(jb_drbg *d, const void *seed, size_t seedlen,
                    const void *add, size_t addlen)
{
    drbg_update(d, seed, seedlen, add, addlen);
    d->reseed_counter = 1;
}

int jb_drbg_generate(jb_drbg *d, void *out, size_t outlen,
                     const void *add, size_t addlen)
{
    jb_hmac_ctx h;
    jb_u8 *o = (jb_u8 *)out;
    if (!d->instantiated) return JB_ERR_NOT_READY;
    if (outlen > JB_DRBG_MAX_BYTES_PER_REQUEST) return JB_ERR_ARG;
    if (d->reseed_counter > JB_DRBG_MAX_REQUESTS) return JB_ERR_RESEED;
    if (addlen) drbg_update(d, add, addlen, 0, 0);
    while (outlen > 0) {
        size_t n = outlen < JB_SHA256_LEN ? outlen : JB_SHA256_LEN;
        jb_hmac_init(&h, d->K, sizeof d->K);
        jb_hmac_update(&h, d->V, sizeof d->V);
        jb_hmac_final(&h, d->V);
        memcpy(o, d->V, n);
        o += n; outlen -= n;
    }
    drbg_update(d, add, addlen, 0, 0);
    d->reseed_counter++;
    return JB_OK;
}

void jb_drbg_wipe(jb_drbg *d)
{
    jb_wipe(d, sizeof *d);
}
