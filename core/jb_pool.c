/* jb_pool.c - C89. */
#include <string.h>
#include "jb_pool.h"

static const char kFold[] = "Jitterbug pool fold v1";

void jb_pool_init(jb_pool *p)
{
    jb_sha256_init(&p->acc);
    jb_sha256_update(&p->acc, "Jitterbug pool v1", 17);
    p->credit_q8 = 0; p->samples = 0; p->extracts = 0;
}

void jb_pool_add(jb_pool *p, const void *data, size_t len, jb_u32 credit_q8)
{
    jb_sha256_update(&p->acc, data, len);
    p->samples++;
    if (p->credit_q8 + credit_q8 < p->credit_q8) p->credit_q8 = 0xFFFFFFFFUL;
    else p->credit_q8 += credit_q8;
}

jb_u32 jb_pool_credit_bits(const jb_pool *p)
{
    return p->credit_q8 >> 8;
}

int jb_pool_extract(jb_pool *p, jb_u32 need_bits, jb_u8 seed[JB_SHA256_LEN])
{
    jb_sha256_ctx copy;
    jb_u8 ctr[4];
    if (jb_pool_credit_bits(p) < need_bits) return JB_ERR_NOT_READY;
    copy = p->acc;
    JB_STORE32BE(ctr, p->extracts);
    jb_sha256_update(&copy, ctr, 4);
    jb_sha256_final(&copy, seed);
    /* Fold: new state depends on old output, not reversible to it. */
    jb_sha256_init(&p->acc);
    jb_sha256_update(&p->acc, kFold, sizeof kFold - 1);
    jb_sha256_update(&p->acc, seed, JB_SHA256_LEN);
    jb_sha256_update(&p->acc, ctr, 4);
    p->credit_q8 = 0;
    p->extracts++;
    return JB_OK;
}
