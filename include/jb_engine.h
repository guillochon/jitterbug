/* jb_engine.h - ties ring -> health -> pool -> DRBG together. This is the
 * code that will live in the PPC shared library and be driven from the
 * INIT's task-time hook. Host builds drive it from the simulator. */
#ifndef JB_ENGINE_H
#define JB_ENGINE_H
#include "jb_sample.h"
#include "jb_health.h"
#include "jb_pool.h"
#include "jb_drbg.h"

#define JB_SEED_BITS_DEFAULT   256u  /* credit needed to (re)seed the DRBG */

typedef struct {
    int    enabled;
    jb_u8  symbol_kind;       /* JB_SYM_*                                 */
    jb_u32 credit_q8;         /* entropy credited per healthy sample      */
    jb_u32 health_h_q8;       /* assumed H used to size health cutoffs    */
} jb_source_cfg;

typedef struct {
    jb_source_cfg cfg[JB_SRC_MAX];
    jb_health     health[JB_SRC_MAX];
    jb_u32        last_ts[JB_SRC_MAX];
    int           have_ts[JB_SRC_MAX];
    jb_u32        seen[JB_SRC_MAX];
    jb_ring       ring;
    jb_pool       pool;
    jb_drbg       drbg;
    jb_u32        seed_bits;
    jb_u32        reseeds;
    jb_u32        health_failures;
} jb_engine;

void jb_engine_init(jb_engine *e);
void jb_engine_config_source(jb_engine *e, jb_u8 src, const jb_source_cfg *c);
/* Mix a persisted seed file. Never credited: it may be stale or copied. */
void jb_engine_mix_seedfile(jb_engine *e, const void *data, size_t len);
/* Task time only: absorb one sample now, bypassing the ring. For producers
 * that already run at task time (the INIT's event filter), so the ring keeps
 * its single, interrupt-time producer. Seeding still happens in pump. */
void   jb_engine_add_sample(jb_engine *e, jb_u8 src, jb_u32 ts, jb_u32 value);
/* Mix caller-supplied bytes (JBStir). Never credited. */
void   jb_engine_stir(jb_engine *e, const void *data, size_t len);
/* Task time: drain the ring, test, mix, (re)seed. Returns samples drained. */
jb_u32 jb_engine_pump(jb_engine *e);
int    jb_engine_ready(const jb_engine *e);
int    jb_engine_random(jb_engine *e, void *out, size_t len);
/* Bytes to write back to the seed file (never reuse a seed across boots). */
int    jb_engine_export_seed(jb_engine *e, jb_u8 out[JB_SHA256_LEN]);
/* Symbol extraction shared with the trace tools. */
jb_u8  jb_symbol(jb_u8 kind, jb_u32 ts, jb_u32 prev_ts, jb_u32 value);

#endif
