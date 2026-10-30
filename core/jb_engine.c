/* jb_engine.c - C89. */
#include <string.h>
#include "jb_engine.h"

void jb_engine_init(jb_engine *e)
{
    memset(e, 0, sizeof *e);
    jb_ring_init(&e->ring);
    jb_pool_init(&e->pool);
    e->seed_bits = JB_SEED_BITS_DEFAULT;
}

void jb_engine_config_source(jb_engine *e, jb_u8 src, const jb_source_cfg *c)
{
    if (src >= JB_SRC_MAX) return;
    e->cfg[src] = *c;
    jb_health_init(&e->health[src], c->health_h_q8);
    e->have_ts[src] = 0;
}

void jb_engine_mix_seedfile(jb_engine *e, const void *data, size_t len)
{
    jb_u8 tag = 0xFF;
    jb_pool_add(&e->pool, &tag, 1, 0);
    jb_pool_add(&e->pool, data, len, 0);
}

jb_u8 jb_symbol(jb_u8 kind, jb_u32 ts, jb_u32 prev_ts, jb_u32 value)
{
    if (kind == JB_SYM_VALUE) return (jb_u8)(value & 0xFF);
    return (jb_u8)((jb_u32)(ts - prev_ts) & 0xFF);
}

static void absorb(jb_engine *e, const jb_sample *s)
{
    jb_u8 rec[10];
    jb_u32 credit = 0;
    const jb_source_cfg *c;
    if (s->source >= JB_SRC_MAX) return;
    c = &e->cfg[s->source];
    e->seen[s->source]++;

    rec[0] = s->source; rec[1] = s->flags;
    JB_STORE32BE(rec + 2, s->ts);
    JB_STORE32BE(rec + 6, s->value);

    if (c->enabled) {
        int need_prev = (c->symbol_kind == JB_SYM_DELTA_TS);
        if (!need_prev || e->have_ts[s->source]) {
            jb_u8 sym = jb_symbol(c->symbol_kind, s->ts, e->last_ts[s->source], s->value);
            if (jb_health_feed(&e->health[s->source], sym) != JB_OK)
                e->health_failures++;
            if (!e->health[s->source].failed) credit = c->credit_q8;
        }
    }
    e->last_ts[s->source] = s->ts;
    e->have_ts[s->source] = 1;
    /* Always mix (it cannot hurt); only credit healthy, enabled sources. */
    jb_pool_add(&e->pool, rec, sizeof rec, credit);
}

jb_u32 jb_engine_pump(jb_engine *e)
{
    jb_sample s;
    jb_u32 n = 0;
    jb_u8 seed[JB_SHA256_LEN];
    while (jb_ring_pop(&e->ring, &s)) { absorb(e, &s); n++; }
    if (jb_pool_extract(&e->pool, e->seed_bits, seed) == JB_OK) {
        if (!e->drbg.instantiated)
            jb_drbg_instantiate(&e->drbg, seed, sizeof seed, "Jitterbug", 9);
        else
            jb_drbg_reseed(&e->drbg, seed, sizeof seed, 0, 0);
        e->reseeds++;
        jb_wipe(seed, sizeof seed);
    }
    return n;
}

int jb_engine_ready(const jb_engine *e)
{
    return e->drbg.instantiated;
}

int jb_engine_random(jb_engine *e, void *out, size_t len)
{
    if (!e->drbg.instantiated) return JB_ERR_NOT_READY;
    return jb_drbg_generate(&e->drbg, out, len, 0, 0);
}

int jb_engine_export_seed(jb_engine *e, jb_u8 out[JB_SHA256_LEN])
{
    return jb_engine_random(e, out, JB_SHA256_LEN);
}
