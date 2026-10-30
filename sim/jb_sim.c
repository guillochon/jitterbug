/* jb_sim.c - host-only noise models (C99). See jb_sim.h for caveats. */
#include <math.h>
#include <string.h>
#include "jb_sim.h"

/* B&W G3: 100 MHz bus, PPC time base ticks every 4 bus clocks = 25 MHz. */
#define BW_TB_HZ 25000000.0

static const jb_sim_profile kProfiles[] = {
  { "bw-g3-idle", "B&W G3 sitting idle on a LAN: timers, background net, rare disk, audio-in noise",
    BW_TB_HZ, 0.0, 5, {
      { JB_SRC_TIMER_JITTER, JB_MODEL_TIMER, { 0.010, 2e-6, 0.02, 50e-6 } },
      { JB_SRC_US_TB_BEAT,   JB_MODEL_TIMER, { 0.005, 1e-6, 0.01, 20e-6 } },
      { JB_SRC_NET,          JB_MODEL_NET,   { 3.0, 30e-6 } },
      { JB_SRC_DISK,         JB_MODEL_DISK,  { 0.2, 5400.0, 0.010 } },
      { JB_SRC_AUDIO,        JB_MODEL_AUDIO, { 1.0, 64.0, 2.0, 3.0 } } } },
  { "bw-g3-active", "B&W G3 in use: mouse/keyboard, disk and network activity",
    BW_TB_HZ, 0.0, 6, {
      { JB_SRC_TIMER_JITTER, JB_MODEL_TIMER, { 0.010, 3e-6, 0.05, 80e-6 } },
      { JB_SRC_US_TB_BEAT,   JB_MODEL_TIMER, { 0.005, 1e-6, 0.02, 20e-6 } },
      { JB_SRC_EVENT,        JB_MODEL_HUMAN, { -3.9, 0.7, 0.05, 0.8 } },
      { JB_SRC_NET,          JB_MODEL_NET,   { 20.0, 40e-6 } },
      { JB_SRC_DISK,         JB_MODEL_DISK,  { 5.0, 5400.0, 0.010 } },
      { JB_SRC_AUDIO,        JB_MODEL_AUDIO, { 1.0, 64.0, 2.0, 3.0 } } } },
  { "cold-boot", "First seconds after power-on: no user input, no network yet",
    BW_TB_HZ, 0.0, 3, {
      { JB_SRC_TIMER_JITTER, JB_MODEL_TIMER, { 0.010, 1e-6, 0.01, 30e-6 } },
      { JB_SRC_US_TB_BEAT,   JB_MODEL_TIMER, { 0.005, 0.5e-6, 0.0, 0.0 } },
      { JB_SRC_AUDIO,        JB_MODEL_AUDIO, { 1.0, 64.0, 1.5, 3.0 } } } },
  { "emulator", "SheepShaver/QEMU-like: host-clock timing quantised to 1 us, little real jitter",
    BW_TB_HZ, 1e-6, 3, {
      { JB_SRC_TIMER_JITTER, JB_MODEL_TIMER, { 0.010, 0.2e-6, 0.0, 0.0 } },
      { JB_SRC_US_TB_BEAT,   JB_MODEL_TIMER, { 0.005, 0.1e-6, 0.0, 0.0 } },
      { JB_SRC_AUDIO,        JB_MODEL_AUDIO, { 1.0, 64.0, 0.0, 0.0 } } } },
  { "degenerate", "Broken sources: perfectly periodic timer, silent audio. Must never seed.",
    BW_TB_HZ, 0.0, 2, {
      { JB_SRC_TIMER_JITTER, JB_MODEL_STUCK, { 0.010, 0.0 } },
      { JB_SRC_AUDIO,        JB_MODEL_STUCK, { 0.020, 3.0 } } } }
};

const jb_sim_profile *jb_sim_profiles(int *count)
{
    *count = (int)(sizeof kProfiles / sizeof kProfiles[0]);
    return kProfiles;
}

const jb_sim_profile *jb_sim_find(const char *name)
{
    int i, n;
    const jb_sim_profile *p = jb_sim_profiles(&n);
    for (i = 0; i < n; i++) if (strcmp(p[i].name, name) == 0) return &p[i];
    return 0;
}

/* xoshiro128** - fast, reproducible, NOT cryptographic. */
static jb_u32 rotl(jb_u32 x, int k) { return (x << k) | (x >> (32 - k)); }
static jb_u32 rng_u32(jb_sim *s)
{
    jb_u32 *r = s->rng;
    jb_u32 out = rotl(r[1] * 5, 7) * 9, t = r[1] << 9;
    r[2] ^= r[0]; r[3] ^= r[1]; r[1] ^= r[2]; r[0] ^= r[3];
    r[2] ^= t; r[3] = rotl(r[3], 11);
    return out;
}
static double uni(jb_sim *s) { return (rng_u32(s) + 0.5) / 4294967296.0; }
static double expo(jb_sim *s, double mean) { return -mean * log(uni(s)); }
static double gauss(jb_sim *s)
{
    return sqrt(-2.0 * log(uni(s))) * cos(6.283185307179586 * uni(s));
}

static double first_time(jb_sim *s, const jb_sim_src *src)
{
    switch (src->model) {
    case JB_MODEL_TIMER: case JB_MODEL_STUCK: return src->p[0];
    case JB_MODEL_HUMAN: return 0.5 + uni(s);
    case JB_MODEL_DISK:  return expo(s, 1.0 / src->p[0]);
    case JB_MODEL_NET:   return expo(s, 1.0 / src->p[0]);
    case JB_MODEL_AUDIO: return expo(s, 1.0 / src->p[0]);
    }
    return 1.0;
}

void jb_sim_init(jb_sim *s, const jb_sim_profile *p, jb_u32 seed)
{
    int i;
    jb_u32 z = seed ? seed : 1;
    memset(s, 0, sizeof *s);
    s->prof = p;
    for (i = 0; i < 4; i++) {            /* splitmix32-style seeding */
        z += 0x9E3779B9u;
        jb_u32 x = z;
        x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
        x = (x ^ (x >> 13)) * 0xC2B2AE35u;
        s->rng[i] = x ^ (x >> 16);
    }
    for (i = 0; i < p->n; i++) s->next_t[i] = first_time(s, &p->src[i]);
}

static jb_u32 to_tb(const jb_sim *s, double t)
{
    double q = s->prof->tb_quantum;
    if (q > 0.0) t = floor(t / q) * q;
    return (jb_u32)fmod(floor(t * s->prof->tb_hz), 4294967296.0);
}

int jb_sim_next(jb_sim *s, double until_s, jb_sample *out)
{
    const jb_sim_profile *p = s->prof;
    int i, k = 0;
    double t, obs;
    const double *a;
    for (i = 1; i < p->n; i++) if (s->next_t[i] < s->next_t[k]) k = i;
    t = s->next_t[k];
    if (t > until_s) return 0;
    s->now = t;
    a = p->src[k].p;
    out->source = p->src[k].source;
    out->flags = 0;
    out->value = 0;

    switch (p->src[k].model) {
    case JB_MODEL_TIMER:
        obs = t + fabs(gauss(s)) * a[1];
        if (a[2] > 0.0 && uni(s) < a[2]) obs += expo(s, a[3]);
        out->ts = to_tb(s, obs);
        out->value = (jb_u32)fmod(floor(obs * 1e6), 4294967296.0); /* Microseconds() view */
        s->next_t[k] = t + a[0];
        break;
    case JB_MODEL_STUCK:
        out->ts = to_tb(s, t);
        out->value = (jb_u32)a[1];
        s->next_t[k] = t + a[0];
        break;
    case JB_MODEL_HUMAN: {
        int dx = (int)lround(gauss(s) * 4.0), dy = (int)lround(gauss(s) * 4.0);
        out->ts = to_tb(s, t);
        out->value = ((jb_u32)(dx & 0xFFFF) << 16) | (jb_u32)(dy & 0xFFFF);
        s->next_t[k] = t + exp(a[0] + a[1] * gauss(s));
        if (uni(s) < a[2]) s->next_t[k] += expo(s, a[3]);
        break; }
    case JB_MODEL_DISK: {
        double rot = 60.0 / a[1];
        obs = t + uni(s) * 2.0 * a[2] + uni(s) * rot + 0.0005;
        out->ts = to_tb(s, obs);
        out->value = rng_u32(s) & 0xFFFF;   /* block number */
        s->next_t[k] = obs + expo(s, 1.0 / a[0]);
        break; }
    case JB_MODEL_NET:
        obs = t + fabs(gauss(s)) * a[1];
        out->ts = to_tb(s, obs);
        out->value = 64 + rng_u32(s) % 1437; /* packet length */
        s->next_t[k] = t + expo(s, 1.0 / a[0]);
        break;
    case JB_MODEL_AUDIO: {
        double v = a[3] + gauss(s) * a[2];
        out->ts = to_tb(s, t);
        out->value = (jb_u32)(jb_u16)(short)lround(v);
        if (s->burst_left[k] <= 0) s->burst_left[k] = (int)a[1];
        if (--s->burst_left[k] > 0) s->next_t[k] = t + 1.0 / 44100.0;
        else s->next_t[k] = t + expo(s, 1.0 / a[0]);
        break; }
    }
    return 1;
}
