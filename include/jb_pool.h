/* jb_pool.h - SHA-256 entropy accumulator with conservative accounting.
 * Hashing does not create entropy; credit comes only from per-source
 * assumptions (q8 bits/sample) that health tests have not contradicted. */
#ifndef JB_POOL_H
#define JB_POOL_H
#include "jb_sha256.h"

typedef struct {
    jb_sha256_ctx acc;
    jb_u32 credit_q8;         /* credited entropy, 1/256-bit units, saturating */
    jb_u32 samples;
    jb_u32 extracts;
} jb_pool;

void jb_pool_init(jb_pool *p);
void jb_pool_add(jb_pool *p, const void *data, size_t len, jb_u32 credit_q8);
jb_u32 jb_pool_credit_bits(const jb_pool *p);
/* Produce a 32-byte seed if credit >= need_bits. Resets credit and folds
 * the output back so successive extracts differ. */
int  jb_pool_extract(jb_pool *p, jb_u32 need_bits, jb_u8 seed[JB_SHA256_LEN]);

#endif
