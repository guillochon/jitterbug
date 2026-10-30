/* jb_policy.c - C89. */
#include "jb_policy.h"

typedef struct { jb_u8 src; const char *name; jb_u8 kind; jb_u32 credit_q8; jb_u32 h_q8; } row;

/*                 source              name          symbol          credit  health-H */
static const row kDefaults[] = {
    { JB_SRC_TIMER_JITTER, "timer-jitter", JB_SYM_DELTA_TS, 128, 512 }, /* 0.5 b / assume 2.0 */
    { JB_SRC_EVENT,        "event",        JB_SYM_DELTA_TS, 256, 512 }, /* 1.0 b / 2.0 */
    { JB_SRC_DISK,         "disk",         JB_SYM_DELTA_TS, 256, 512 }, /* 1.0 b / 2.0 */
    { JB_SRC_NET,          "net",          JB_SYM_DELTA_TS,  64, 256 }, /* 0.25 b: LAN-observable */
    { JB_SRC_AUDIO,        "audio",        JB_SYM_VALUE,    128, 256 }, /* 0.5 b / 1.0 */
    { JB_SRC_US_TB_BEAT,   "us-tb-beat",   JB_SYM_DELTA_TS,  64, 256 }  /* 0.25 b / 1.0 */
};
#define NROWS (sizeof kDefaults / sizeof kDefaults[0])

void jb_policy_apply_defaults(jb_engine *e)
{
    unsigned i;
    for (i = 0; i < NROWS; i++) {
        jb_source_cfg c;
        c.enabled = 1;
        c.symbol_kind = kDefaults[i].kind;
        c.credit_q8 = kDefaults[i].credit_q8;
        c.health_h_q8 = kDefaults[i].h_q8;
        jb_engine_config_source(e, kDefaults[i].src, &c);
    }
}

void jb_policy_apply_zero_credit(jb_engine *e)
{
    unsigned i;
    jb_policy_apply_defaults(e);
    for (i = 0; i < NROWS; i++) e->cfg[kDefaults[i].src].credit_q8 = 0;
}

const char *jb_source_name(jb_u8 src)
{
    unsigned i;
    for (i = 0; i < NROWS; i++) if (kDefaults[i].src == src) return kDefaults[i].name;
    return "unknown";
}

jb_u8 jb_source_symkind(jb_u8 src)
{
    unsigned i;
    for (i = 0; i < NROWS; i++) if (kDefaults[i].src == src) return kDefaults[i].kind;
    return JB_SYM_DELTA_TS;
}
