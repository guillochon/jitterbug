/* jb_sim.h - HOST-ONLY statistical models of the B&W G3 noise sources.
 *
 * Purpose: exercise the ring/health/pool/DRBG pipeline, reproduce bugs,
 * and test degenerate cases on machines that are not the target.
 * The output is driven by a seeded NON-CRYPTOGRAPHIC PRNG and is fully
 * reproducible. It is never entropy, and must never be compiled into a
 * Mac build. Model parameters are rough guesses until replaced by values
 * fitted to real .jbr traces captured on the B&W. */
#ifndef JB_SIM_H
#define JB_SIM_H
#include "jb_sample.h"

typedef enum {
    JB_MODEL_TIMER,   /* p: period_s, jitter_sigma_s, spike_prob, spike_mean_s */
    JB_MODEL_HUMAN,   /* p: lognormal mu (ln s), sigma, pause_prob, pause_mean_s */
    JB_MODEL_DISK,    /* p: req_rate_hz, rpm, avg_seek_s                       */
    JB_MODEL_NET,     /* p: rate_hz, jitter_sigma_s                            */
    JB_MODEL_AUDIO,   /* p: burst_rate_hz, samples_per_burst, sigma_lsb, dc    */
    JB_MODEL_STUCK    /* p: period_s, value       (degenerate: perfectly periodic) */
} jb_model;

typedef struct {
    jb_u8    source;
    jb_model model;
    double   p[4];
} jb_sim_src;

#define JB_SIM_MAXSRC 8

typedef struct {
    const char *name;
    const char *desc;
    double      tb_hz;        /* time base frequency of the modelled Mac */
    double      tb_quantum;   /* coarsest timing resolution (s); emulators are coarse */
    int         n;
    jb_sim_src  src[JB_SIM_MAXSRC];
} jb_sim_profile;

typedef struct {
    const jb_sim_profile *prof;
    jb_u32 rng[4];
    double next_t[JB_SIM_MAXSRC];
    int    burst_left[JB_SIM_MAXSRC];
    double now;
} jb_sim;

const jb_sim_profile *jb_sim_profiles(int *count);
const jb_sim_profile *jb_sim_find(const char *name);
void   jb_sim_init(jb_sim *s, const jb_sim_profile *p, jb_u32 seed);
/* Next sample in time order; returns 0 once its time would exceed until_s. */
int    jb_sim_next(jb_sim *s, double until_s, jb_sample *out);

#endif
