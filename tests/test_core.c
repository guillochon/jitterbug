/* test_core.c - unit tests for the portable core (host). */
#include <stdio.h>
#include <string.h>
#include "jb_sha256.h"
#include "jb_drbg.h"
#include "jb_health.h"
#include "jb_sample.h"
#include "jb_pool.h"

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

static void hex(const jb_u8 *b, size_t n, char *out)
{
    size_t i;
    for (i = 0; i < n; i++) sprintf(out + 2 * i, "%02x", b[i]);
}

static int sha_is(const void *m, size_t n, const char *want)
{
    jb_u8 d[32]; char h[65];
    jb_sha256(m, n, d); hex(d, 32, h);
    return strcmp(h, want) == 0;
}

static void test_sha256(void)
{
    static jb_u8 million[1000000];
    jb_sha256_ctx c; jb_u8 d[32]; char h[65]; int i;
    CHECK(sha_is("", 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"), "SHA-256 empty");
    CHECK(sha_is("abc", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), "SHA-256 abc");
    CHECK(sha_is("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
                 "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"), "SHA-256 448-bit");
    memset(million, 'a', sizeof million);
    jb_sha256_init(&c);
    for (i = 0; i < 1000; i++) jb_sha256_update(&c, million + i * 1000, 1000); /* chunked */
    jb_sha256_final(&c, d); hex(d, 32, h);
    CHECK(strcmp(h, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0, "SHA-256 million a (chunked)");
}

static void test_hmac(void)
{
    jb_hmac_ctx c; jb_u8 key[20], d[32]; char h[65];
    memset(key, 0x0b, 20);
    jb_hmac_init(&c, key, 20); jb_hmac_update(&c, "Hi There", 8); jb_hmac_final(&c, d); hex(d, 32, h);
    CHECK(strcmp(h, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7") == 0, "HMAC RFC 4231 case 1");
    jb_hmac_init(&c, (const jb_u8 *)"Jefe", 4);
    jb_hmac_update(&c, "what do ya want for nothing?", 28); jb_hmac_final(&c, d); hex(d, 32, h);
    CHECK(strcmp(h, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843") == 0, "HMAC RFC 4231 case 2");
}

static void test_drbg(void)
{
    jb_drbg d; jb_u8 seed[32], out[40]; char h[81]; int i;
    for (i = 0; i < 32; i++) seed[i] = (jb_u8)i;
    jb_drbg_instantiate(&d, seed, 32, "Jitterbug", 9);
    jb_drbg_generate(&d, out, 40, 0, 0); hex(out, 40, h);
    CHECK(strcmp(h, "01ea4ca5af6dcc3736be23d741612388444c6c9a3d0ad74d5dc7a5b57faf1c6d3f4a9814224c7c41") == 0,
          "HMAC-DRBG matches Python reference (generate)");
    for (i = 0; i < 32; i++) seed[i] = (jb_u8)(32 + i);
    jb_drbg_reseed(&d, seed, 32, 0, 0);
    jb_drbg_generate(&d, out, 32, 0, 0); hex(out, 32, h);
    CHECK(strcmp(h, "2b364a464367800bfee96f1a058f41554f7bc770e95fee799519033624f464ea") == 0,
          "HMAC-DRBG matches Python reference (after reseed)");
    { jb_drbg z; memset(&z, 0, sizeof z);
      CHECK(jb_drbg_generate(&z, out, 1, 0, 0) == JB_ERR_NOT_READY, "uninstantiated DRBG refuses"); }
}

static void test_health(void)
{
    jb_health t; int i, failed = 0;
    jb_u32 seed = 12345;
    CHECK(jb_rct_cutoff(256) == 21, "RCT cutoff H=1 is 21");
    CHECK(jb_rct_cutoff(512) == 11, "RCT cutoff H=2 is 11");
    printf("      APT cutoffs (W=512): H=0.5 -> %lu, H=1 -> %lu, H=2 -> %lu\n",
           (unsigned long)jb_apt_cutoff(128, 512), (unsigned long)jb_apt_cutoff(256, 512),
           (unsigned long)jb_apt_cutoff(512, 512));
    /* Cross-checked with exact (60-digit decimal) binomial arithmetic. */
    CHECK(jb_apt_cutoff(128, 512) == 410, "APT cutoff H=0.5 W=512 is 410");
    CHECK(jb_apt_cutoff(256, 512) == 311, "APT cutoff H=1 W=512 is 311");
    CHECK(jb_apt_cutoff(512, 512) == 177, "APT cutoff H=2 W=512 is 177");
    jb_health_init(&t, 256);
    for (i = 0; i < 100; i++) if (jb_health_feed(&t, 7) != JB_OK) failed = 1;
    CHECK(failed && t.rct_failures > 0, "stuck source trips RCT");
    jb_health_init(&t, 256);
    failed = 0;
    for (i = 0; i < 100000; i++) {
        seed = seed * 1103515245u + 12345u;
        if (jb_health_feed(&t, (jb_u8)(seed >> 16)) != JB_OK) failed = 1;
    }
    CHECK(!failed, "well-spread symbols pass RCT+APT");
    jb_health_init(&t, 256);
    failed = 0;
    for (i = 0; i < 5000; i++) {
        seed = seed * 1103515245u + 12345u;
        /* 90% one value: APT should catch it, RCT may not */
        if (jb_health_feed(&t, ((seed >> 16) % 10) ? 42 : (jb_u8)(seed >> 8)) != JB_OK) failed = 1;
    }
    CHECK(failed && t.apt_failures > 0, "heavily biased source trips APT");
}

static void test_ring(void)
{
    static jb_ring r; jb_sample s; int i, ok = 1;
    jb_ring_init(&r);
    for (i = 0; i < (int)JB_RING_SIZE; i++) ok &= jb_ring_push(&r, 1, (jb_u32)i, 0);
    CHECK(ok && !jb_ring_push(&r, 1, 0, 0) && r.dropped == 1, "ring fills and counts drops");
    for (i = 0; i < (int)JB_RING_SIZE; i++) ok &= jb_ring_pop(&r, &s) && s.ts == (jb_u32)i;
    CHECK(ok && !jb_ring_pop(&r, &s), "ring drains FIFO");
}

static void test_pool(void)
{
    jb_pool p; jb_u8 a[32], b[32]; int i;
    jb_pool_init(&p);
    jb_pool_add(&p, "x", 1, 100 * 256);
    CHECK(jb_pool_extract(&p, 256, a) == JB_ERR_NOT_READY, "pool refuses below threshold");
    for (i = 0; i < 2; i++) jb_pool_add(&p, "y", 1, 100 * 256);
    CHECK(jb_pool_extract(&p, 256, a) == JB_OK && jb_pool_credit_bits(&p) == 0, "pool extracts and resets credit");
    jb_pool_add(&p, "y", 1, 300 * 256);
    jb_pool_extract(&p, 256, b);
    CHECK(memcmp(a, b, 32) != 0, "successive extracts differ");
}

int main(void)
{
    test_sha256(); test_hmac(); test_drbg(); test_health(); test_ring(); test_pool();
    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
