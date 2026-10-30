/* test_sim.c - end-to-end pipeline tests driven by the simulator. */
#include <stdio.h>
#include <string.h>
#include "jb_engine.h"
#include "jb_policy.h"
#include "jb_sim.h"

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

static double run(const char *profile, double secs, jb_u32 seed, int zero, jb_engine *e)
{
    jb_sim s; jb_sample smp; double next = 0.01, ready = -1;
    jb_engine_init(e);
    if (zero) jb_policy_apply_zero_credit(e); else jb_policy_apply_defaults(e);
    jb_sim_init(&s, jb_sim_find(profile), seed);
    while (jb_sim_next(&s, secs, &smp)) {
        jb_ring_push(&e->ring, smp.source, smp.ts, smp.value);
        if (s.now >= next) { jb_engine_pump(e); if (ready < 0 && jb_engine_ready(e)) ready = s.now; next = s.now + 0.01; }
    }
    jb_engine_pump(e);
    return ready;
}

int main(void)
{
    static jb_engine e1, e2;
    jb_u8 a[32], b[32];
    double r;

    r = run("bw-g3-active", 30, 1, 0, &e1);
    printf("      bw-g3-active seeded at %.2f s\n", r);
    CHECK(r >= 0 && r < 30, "active profile seeds the DRBG");

    r = run("degenerate", 60, 1, 0, &e1);
    CHECK(r < 0, "degenerate profile never seeds");
    CHECK(e1.health[JB_SRC_TIMER_JITTER].failed && e1.health[JB_SRC_AUDIO].failed,
          "degenerate sources fail health tests");

    r = run("bw-g3-active", 30, 1, 1, &e1);
    CHECK(r < 0, "zero-credit policy never seeds from live samples");

    run("bw-g3-idle", 30, 7, 0, &e1); run("bw-g3-idle", 30, 7, 0, &e2);
    jb_engine_random(&e1, a, 32); jb_engine_random(&e2, b, 32);
    CHECK(memcmp(a, b, 32) == 0, "same sim seed reproduces identical output (sim is deterministic)");

    run("bw-g3-idle", 30, 8, 0, &e2); jb_engine_random(&e2, b, 32);
    CHECK(memcmp(a, b, 32) != 0, "different sim seed gives different output");

    r = run("emulator", 60, 1, 0, &e1);
    printf("      emulator profile: %s\n", r < 0 ? "never seeded" : "seeded");

    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
