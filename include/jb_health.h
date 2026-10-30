/* jb_health.h - continuous health tests from NIST SP 800-90B sec 4.4:
 * Repetition Count Test (RCT) and Adaptive Proportion Test (APT),
 * run per source on 8-bit symbols, false-positive rate alpha = 2^-20.
 *
 * H (bits of min-entropy per symbol) is an ASSUMPTION per source until
 * measured on real hardware with ea_non_iid. See docs/ENTROPY_POLICY.md. */
#ifndef JB_HEALTH_H
#define JB_HEALTH_H
#include "jb_types.h"

#define JB_APT_WINDOW 512u

typedef struct {
    /* config */
    jb_u32 rct_cutoff;
    jb_u32 apt_cutoff;
    /* RCT state */
    int    have_last;
    jb_u8  last;
    jb_u32 rct_run;
    /* APT state */
    jb_u8  apt_base;
    jb_u32 apt_pos;           /* 0..JB_APT_WINDOW-1; 0 = start new window */
    jb_u32 apt_count;
    /* results */
    jb_u32 rct_failures;
    jb_u32 apt_failures;
    int    failed;            /* sticky until jb_health_reset() */
} jb_health;

/* h_q8: assumed min-entropy per symbol in 1/256-bit units (e.g. 256 = 1.0 bit). */
void jb_health_init(jb_health *t, jb_u32 h_q8);
void jb_health_reset(jb_health *t);
/* Feed one symbol. Returns JB_OK or JB_ERR_HEALTH (on the failing sample). */
int  jb_health_feed(jb_health *t, jb_u8 sym);

/* Exposed for tests/tools. */
jb_u32 jb_rct_cutoff(jb_u32 h_q8);
jb_u32 jb_apt_cutoff(jb_u32 h_q8, jb_u32 window);

#endif
