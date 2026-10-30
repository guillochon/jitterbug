/* jb_health.c - SP 800-90B RCT/APT. Cutoffs are computed once, at task
 * time (uses floating point). jb_health_feed() is integer-only. C89. */
#include <math.h>
#include "jb_health.h"

#define ALPHA_LOG2 20.0

jb_u32 jb_rct_cutoff(jb_u32 h_q8)
{
    /* C = 1 + ceil(20 / H) */
    double H = (double)h_q8 / 256.0;
    if (H <= 0.0) return 0xFFFFFFFFUL;
    return (jb_u32)(1.0 + ceil(ALPHA_LOG2 / H));
}

jb_u32 jb_apt_cutoff(jb_u32 h_q8, jb_u32 window)
{
    /* C = 1 + CRITBINOM(W, 2^-H, 1 - alpha): smallest c with
     * P(X >= c) <= alpha for X ~ Binomial(W, p). */
    double H = (double)h_q8 / 256.0;
    double p, q, pmf, cdf, target;
    jb_u32 k;
    if (H <= 0.0) return window + 1;
    p = pow(2.0, -H);
    if (p >= 1.0) return window + 1;
    q = 1.0 - p;
    target = 1.0 - pow(2.0, -ALPHA_LOG2);
    pmf = pow(q, (double)window);          /* P(X = 0) */
    cdf = pmf;
    for (k = 0; k < window; k++) {
        if (cdf >= target) return k + 1;   /* P(X <= k) >= 1-alpha */
        pmf = pmf * (double)(window - k) / (double)(k + 1) * (p / q);
        cdf += pmf;
    }
    return window;
}

void jb_health_init(jb_health *t, jb_u32 h_q8)
{
    t->rct_cutoff = jb_rct_cutoff(h_q8);
    t->apt_cutoff = jb_apt_cutoff(h_q8, JB_APT_WINDOW);
    t->rct_failures = t->apt_failures = 0;
    jb_health_reset(t);
}

void jb_health_reset(jb_health *t)
{
    t->have_last = 0; t->last = 0; t->rct_run = 0;
    t->apt_base = 0; t->apt_pos = 0; t->apt_count = 0;
    t->failed = 0;
}

int jb_health_feed(jb_health *t, jb_u8 sym)
{
    int bad = 0;

    /* Repetition Count Test */
    if (t->have_last && sym == t->last) {
        if (++t->rct_run >= t->rct_cutoff) { t->rct_failures++; bad = 1; t->rct_run = 1; }
    } else {
        t->last = sym; t->have_last = 1; t->rct_run = 1;
    }

    /* Adaptive Proportion Test */
    if (t->apt_pos == 0) {
        t->apt_base = sym; t->apt_count = 1;
    } else if (sym == t->apt_base) {
        if (++t->apt_count >= t->apt_cutoff) { t->apt_failures++; bad = 1; t->apt_count = 0; }
    }
    if (++t->apt_pos >= JB_APT_WINDOW) t->apt_pos = 0;

    if (bad) { t->failed = 1; return JB_ERR_HEALTH; }
    return JB_OK;
}
