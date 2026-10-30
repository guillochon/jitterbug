/* jb_sim - host driver for the Jitterbug pipeline.
 *
 *   jb_sim list
 *   jb_sim dump   -p PROFILE [-t SECONDS] [-s SEED] -o trace.jbr
 *   jb_sim run    -p PROFILE [-t SECONDS] [-s SEED] [-n BYTES] [-o out.bin|-]
 *                 [--zero-credit] [--seedfile FILE]
 *   jb_sim replay -i trace.jbr [-n BYTES] [-o out.bin|-] [--zero-credit]
 *
 * "run" and "replay" push samples through the exact core code that will run
 * on the Mac (ring -> health -> pool -> HMAC-DRBG) and report what happened.
 * Output bytes from simulated input are NOT secure: the sim PRNG is seeded
 * and reproducible. Use them only for pipeline and statistical testing. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jb_engine.h"
#include "jb_policy.h"
#include "jb_sim.h"
#include "jb_trace.h"

typedef struct {
    const char *profile, *out, *in, *seedfile;
    double seconds;
    unsigned long seed, bytes;
    int zero_credit;
} opts;

static void usage(void)
{
    fprintf(stderr,
      "usage:\n"
      "  jb_sim list\n"
      "  jb_sim dump   -p PROFILE [-t SECONDS] [-s SEED] -o trace.jbr\n"
      "  jb_sim run    -p PROFILE [-t SECONDS] [-s SEED] [-n BYTES] [-o out|-] [--zero-credit] [--seedfile F]\n"
      "  jb_sim replay -i trace.jbr [-n BYTES] [-o out|-] [--zero-credit]\n");
    exit(2);
}

static void parse(int argc, char **argv, opts *o)
{
    int i;
    memset(o, 0, sizeof *o);
    o->seconds = 60.0; o->seed = 1;
    for (i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *v = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (!strcmp(a, "--zero-credit")) { o->zero_credit = 1; continue; }
        if (!v) usage();
        if      (!strcmp(a, "-p")) o->profile = v;
        else if (!strcmp(a, "-t")) o->seconds = atof(v);
        else if (!strcmp(a, "-s")) o->seed = strtoul(v, 0, 0);
        else if (!strcmp(a, "-n")) o->bytes = strtoul(v, 0, 0);
        else if (!strcmp(a, "-o")) o->out = v;
        else if (!strcmp(a, "-i")) o->in = v;
        else if (!strcmp(a, "--seedfile")) o->seedfile = v;
        else usage();
        i++;
    }
}

static void setup_engine(jb_engine *e, const opts *o)
{
    jb_engine_init(e);
    if (o->zero_credit) jb_policy_apply_zero_credit(e);
    else jb_policy_apply_defaults(e);
    if (o->seedfile) {
        unsigned char buf[256];
        size_t n;
        FILE *f = fopen(o->seedfile, "rb");
        if (f) { n = fread(buf, 1, sizeof buf, f); fclose(f); jb_engine_mix_seedfile(e, buf, n); }
        else fprintf(stderr, "note: seed file %s not found (first boot)\n", o->seedfile);
    }
}

static void report(jb_engine *e, double first_ready, double total)
{
    int i;
    printf("%-14s %10s %10s %8s %8s %8s\n", "source", "samples", "credit/smp", "RCTfail", "APTfail", "status");
    for (i = 1; i < JB_SRC_MAX; i++) {
        if (!e->seen[i]) continue;
        printf("%-14s %10lu %10.3f %8lu %8lu %8s\n", jb_source_name((jb_u8)i),
               (unsigned long)e->seen[i], e->cfg[i].credit_q8 / 256.0,
               (unsigned long)e->health[i].rct_failures,
               (unsigned long)e->health[i].apt_failures,
               e->health[i].failed ? "FAILED" : "ok");
    }
    printf("ring drops: %lu   reseeds: %lu   pool credit now: %lu bits\n",
           (unsigned long)e->ring.dropped, (unsigned long)e->reseeds,
           (unsigned long)jb_pool_credit_bits(&e->pool));
    if (first_ready >= 0) printf("DRBG first seeded at t=%.3f s (of %.1f s)\n", first_ready, total);
    else printf("DRBG NEVER SEEDED in %.1f s - random requests would return NOT_READY\n", total);
}

static int emit(jb_engine *e, const opts *o)
{
    FILE *f;
    unsigned char buf[4096];
    unsigned long left = o->bytes;
    if (!left) return 0;
    if (!jb_engine_ready(e)) { fprintf(stderr, "not ready: no bytes written\n"); return 1; }
    f = (!o->out || !strcmp(o->out, "-")) ? stdout : fopen(o->out, "wb");
    if (!f) { perror(o->out); return 1; }
    while (left) {
        size_t n = left < sizeof buf ? left : sizeof buf;
        if (jb_engine_random(e, buf, n) != JB_OK) { fprintf(stderr, "generate failed\n"); return 1; }
        fwrite(buf, 1, n, f);
        left -= n;
    }
    if (f != stdout) fclose(f);
    return 0;
}

static int cmd_list(void)
{
    int i, n;
    const jb_sim_profile *p = jb_sim_profiles(&n);
    for (i = 0; i < n; i++) printf("%-14s %s\n", p[i].name, p[i].desc);
    return 0;
}

static int cmd_dump(const opts *o)
{
    const jb_sim_profile *p = o->profile ? jb_sim_find(o->profile) : NULL;
    jb_sim s; jb_sample smp; FILE *f; jb_u8 ids[JB_SIM_MAXSRC]; int i;
    unsigned long count = 0;
    if (!p || !o->out) usage();
    f = fopen(o->out, "wb");
    if (!f) { perror(o->out); return 1; }
    for (i = 0; i < p->n; i++) ids[i] = p->src[i].source;
    jb_trace_write_header(f, (jb_u32)p->tb_hz, ids, p->n);
    jb_sim_init(&s, p, (jb_u32)o->seed);
    while (jb_sim_next(&s, o->seconds, &smp)) { jb_trace_write(f, &smp); count++; }
    fclose(f);
    fprintf(stderr, "wrote %lu simulated samples (%s, %.1f s) to %s\n", count, p->name, o->seconds, o->out);
    return 0;
}

static int cmd_run(const opts *o)
{
    const jb_sim_profile *p = o->profile ? jb_sim_find(o->profile) : NULL;
    static jb_engine e;
    jb_sim s; jb_sample smp;
    double next_pump = 0.01, first_ready = -1.0;
    if (!p) usage();
    setup_engine(&e, o);
    jb_sim_init(&s, p, (jb_u32)o->seed);
    while (jb_sim_next(&s, o->seconds, &smp)) {
        /* Producer side: what the INIT's interrupt-time hooks will do. */
        jb_ring_push(&e.ring, smp.source, smp.ts, smp.value);
        /* Consumer side: task-time pump every 10 ms of virtual time. */
        if (s.now >= next_pump) {
            jb_engine_pump(&e);
            if (first_ready < 0 && jb_engine_ready(&e)) first_ready = s.now;
            next_pump = s.now + 0.01;
        }
    }
    jb_engine_pump(&e);
    if (first_ready < 0 && jb_engine_ready(&e)) first_ready = o->seconds;
    fprintf(stderr, "profile %s, seed %lu\n", p->name, o->seed);
    report(&e, first_ready, o->seconds);
    return emit(&e, o);
}

static int cmd_replay(const opts *o)
{
    static jb_engine e;
    jb_sample smp; jb_u32 tb_hz; FILE *f;
    jb_u32 prev = 0, n = 0, d;
    double first_ready = -1.0, t = 0.0;
    if (!o->in) usage();
    f = fopen(o->in, "rb");
    if (!f || jb_trace_read_header(f, &tb_hz) != 0) { fprintf(stderr, "bad trace %s\n", o->in); return 1; }
    setup_engine(&e, o);
    while (jb_trace_read(f, &smp)) {
        /* Elapsed time from per-sample deltas: the 32-bit time base wraps
         * every ~172 s at 25 MHz, and sources can be slightly out of order. */
        if (n++ > 0) { d = (jb_u32)(smp.ts - prev); if (d < 0x80000000UL) t += (double)d / (double)tb_hz; }
        if (n == 1 || (jb_u32)(smp.ts - prev) < 0x80000000UL) prev = smp.ts;
        jb_ring_push(&e.ring, smp.source, smp.ts, smp.value);
        if ((n & 63) == 0) {
            jb_engine_pump(&e);
            if (first_ready < 0 && jb_engine_ready(&e)) first_ready = t;
        }
    }
    fclose(f);
    jb_engine_pump(&e);
    if (first_ready < 0 && jb_engine_ready(&e)) first_ready = t;
    printf("replayed %lu samples from %s (time base %lu Hz)\n", (unsigned long)n, o->in, (unsigned long)tb_hz);
    report(&e, first_ready, t);
    return emit(&e, o);
}

int main(int argc, char **argv)
{
    opts o;
    if (argc < 2) usage();
    parse(argc, argv, &o);
    if (!strcmp(argv[1], "list"))   return cmd_list();
    if (!strcmp(argv[1], "dump"))   return cmd_dump(&o);
    if (!strcmp(argv[1], "run"))    return cmd_run(&o);
    if (!strcmp(argv[1], "replay")) return cmd_replay(&o);
    usage();
    return 2;
}
