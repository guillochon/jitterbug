/* Jitterbug INIT - system-wide entropy collection for classic Mac OS.
 *
 * A 68K code resource (Retro68 builds INITs only as 68K; on PowerPC Macs it
 * runs under the 68K emulator). At boot it:
 *   - allocates the engine in the system heap (Retro68 puts our zero-initialised
 *     globals there because the resource is loaded into the system heap);
 *   - mixes "Jitterbug Seed" from Preferences with zero credit, then
 *     immediately replaces it so the same contents are never mixed twice;
 *   - installs a Time Manager task (interrupt time, the ring's only producer),
 *     a jGNEFilter (task time: event timing, and the engine pump), and a
 *     shutdown procedure that saves a fresh seed;
 *   - publishes the API table through Gestalt 'JBug' (see mac/sdk/Jitterbug.h);
 *   - shows its icon in the startup parade, dancing.
 *
 * Interrupt-time rule (CLAUDE.md #2): jb_timer_fire only reads the clock,
 * pushes into the preallocated ring and re-primes the timer.
 *
 * STATUS: builds; not yet booted on hardware or in an emulator.
 * MIT License, Copyright (c) 2026 James Guillochon. See LICENSE. */

#include <Multiverse.h>
#include <string.h>
#include "Retro68Runtime.h"

#include "jb_engine.h"
#include "jb_policy.h"
#include "jb_sha256.h"
#include "Jitterbug.h"

#define kTimerIntervalMs  10         /* ~100 timer-jitter samples per second */
#define kLMJGNEFilter     0x029A     /* low-memory jGNEFilter hook           */
#define kLMShowINITH      0x092C     /* ShowINIT protocol: next icon h       */
#define kLMShowINITCheck  0x092E     /* ... and its checksum                 */
#define kSystemDisk       ((short)0x8000)
#define kIconMain         128
#define kIconDanceA       129
#define kIconDanceB       130
#define kTransformDimmed  1
#define kExtendedTimeMgr  3          /* gestaltExtendedTimeMgr (InsXTime) */

static const unsigned char kSeedName[] = "\pJitterbug Seed";
static const unsigned char kLogName[]  = "\pJitterbug Log";

/* ---- state (BSS, in the system heap) ------------------------------------ */

static jb_engine       gEngine;
static TMTask          gTimer;
static JBDispatchTable gTable;
static volatile unsigned char gBusy;
static unsigned long   gBusySkips;
static int             gSeedMixed;
static int             gSeedSavedThisBoot;

/* Referenced from the assembly stubs, so not static. */
unsigned long jb_old_filter;
void jb_filter_stub(void);
void jb_timer_stub(void);
void jb_shutdown_stub(void);

/* ---- clock --------------------------------------------------------------- */

/* Multiversal omits Microseconds; trap 0xA193 returns hi in A0, lo in D0. */
static jb_u32 now_us(void)
{
    register unsigned long hi __asm__("a0");
    register unsigned long lo __asm__("d0");
    __asm__ __volatile__(".short 0xA193" : "=r"(hi), "=r"(lo) : : "d1", "d2", "a1", "cc", "memory");
    (void)hi;
    return (jb_u32)lo;
}

/* ---- re-entrancy guard ----------------------------------------------------
 * Task-time callers (the event filter and the API) can be interrupted by a
 * deferred task that calls the API. bset is a single instruction, so it is
 * atomic with respect to interrupts on one CPU. */
static int try_lock(void)
{
    unsigned char acquired;
    __asm__ __volatile__("bset #0,%1\n\tseq %0" : "=d"(acquired), "+m"(gBusy) : : "cc");
    return acquired != 0;
}

static void unlock(void)
{
    gBusy = 0;
}

static long map_err(int r)
{
    switch (r) {
    case JB_OK:            return kJBNoErr;
    case JB_ERR_NOT_READY: return kJBErrNotReady;
    case JB_ERR_HEALTH:    return kJBErrHealth;
    case JB_ERR_RESEED:    return kJBErrReseed;
    default:               return kJBErrArg;
    }
}

/* ---- seed file ------------------------------------------------------------ */

static OSErr seed_spec(FSSpec *spec)
{
    short vref;
    long dir;
    OSErr err = FindFolder(kSystemDisk, kPreferencesFolderType, true, &vref, &dir);
    if (err != noErr) return err;
    err = FSMakeFSSpec(vref, dir, kSeedName, spec);
    return (err == fnfErr) ? noErr : err;
}

static void seed_write(const jb_u8 bytes[JB_SHA256_LEN])
{
    FSSpec spec;
    short ref;
    long n = JB_SHA256_LEN;
    if (seed_spec(&spec) != noErr) return;
    (void)FSpCreate(&spec, 'JtBg', 'JBsd', smSystemScript);
    if (FSpOpenDF(&spec, fsRdWrPerm, &ref) != noErr) return;
    if (SetFPos(ref, fsFromStart, 0) == noErr && FSWrite(ref, &n, bytes) == noErr)
        (void)SetEOF(ref, JB_SHA256_LEN);
    FSClose(ref);
    (void)FlushVol(NULL, spec.vRefNum);
}

/* Mix the old seed (never credited, CLAUDE.md #5), then overwrite it at once
 * with a one-way successor so a crash before shutdown cannot replay it. */
static void seed_load_and_forward(void)
{
    FSSpec spec;
    short ref;
    long n = JB_SHA256_LEN;
    jb_u8 buf[JB_SHA256_LEN], next[JB_SHA256_LEN];
    jb_sha256_ctx h;
    jb_u32 t;

    if (seed_spec(&spec) != noErr) return;
    if (FSpOpenDF(&spec, fsRdPerm, &ref) != noErr) return;
    if (FSRead(ref, &n, buf) != noErr && n == 0) { FSClose(ref); return; }
    FSClose(ref);
    if (n <= 0) return;

    jb_engine_mix_seedfile(&gEngine, buf, (size_t)n);
    gSeedMixed = 1;

    t = now_us();
    jb_sha256_init(&h);
    jb_sha256_update(&h, "Jitterbug seed forward", 22);
    jb_sha256_update(&h, buf, (size_t)n);
    jb_sha256_update(&h, &t, sizeof t);
    jb_sha256_final(&h, next);
    seed_write(next);
    jb_wipe(buf, sizeof buf);
    jb_wipe(next, sizeof next);
}

/* Task time, lock held: once per boot after first seeding, and at shutdown. */
static void seed_save_fresh(void)
{
    jb_u8 out[JB_SHA256_LEN];
    if (jb_engine_export_seed(&gEngine, out) != JB_OK) return;
    seed_write(out);
    jb_wipe(out, sizeof out);
}

/* ---- hooks ---------------------------------------------------------------- */

/* Interrupt time. */
void jb_timer_fire(TMTask *t)
{
    jb_ring_push(&gEngine.ring, JB_SRC_TIMER_JITTER, now_us(), 0);
    PrimeTime((QElemPtr)t, kTimerIntervalMs);
}

/* Task time, inside GetNextEvent/WaitNextEvent in whatever app is running. */
void jb_event_filter(EventRecord *ev)
{
    jb_u32 t = now_us();
    if (!try_lock()) { gBusySkips++; return; }
    if (ev->what != nullEvent)
        jb_engine_add_sample(&gEngine, JB_SRC_EVENT, t,
                             ((jb_u32)(jb_u16)ev->where.h << 16) | (jb_u16)ev->where.v);
    jb_engine_pump(&gEngine);
    if (!gSeedSavedThisBoot && jb_engine_ready(&gEngine)) {
        gSeedSavedThisBoot = 1;
        seed_save_fresh();
    }
    unlock();
}

/* Shutdown, before volumes are unmounted. */
void jb_shutdown(void)
{
    if (!try_lock()) return;
    jb_engine_pump(&gEngine);
    seed_save_fresh();
    unlock();
}

/* Register-preserving glue. jGNEFilter: A1 = event record, result word at
 * 4(SP); we chain by pushing the old filter's address and returning to it.
 * Time Manager: A1 = the TMTask. Shutdown procs: stage in D0. */
__asm__(
    "    .text\n"
    "    .globl jb_filter_stub\n"
    "jb_filter_stub:\n"
    "    movem.l %d0-%d2/%a0-%a2,-(%sp)\n"
    "    move.l  %a1,-(%sp)\n"
    "    jsr     jb_event_filter\n"
    "    addq.l  #4,%sp\n"
    "    movem.l (%sp)+,%d0-%d2/%a0-%a2\n"
    "    tst.l   jb_old_filter\n"
    "    beq.s   1f\n"
    "    move.l  jb_old_filter,-(%sp)\n"
    "1:  rts\n"
    "    .globl jb_timer_stub\n"
    "jb_timer_stub:\n"
    "    movem.l %d0-%d2/%a0-%a2,-(%sp)\n"
    "    move.l  %a1,-(%sp)\n"
    "    jsr     jb_timer_fire\n"
    "    addq.l  #4,%sp\n"
    "    movem.l (%sp)+,%d0-%d2/%a0-%a2\n"
    "    rts\n"
    "    .globl jb_shutdown_stub\n"
    "jb_shutdown_stub:\n"
    "    movem.l %d0-%d2/%a0-%a2,-(%sp)\n"
    "    jsr     jb_shutdown\n"
    "    movem.l (%sp)+,%d0-%d2/%a0-%a2\n"
    "    rts\n");

/* ---- API (68K C convention; see JBDispatchTable) ---------------------------- */

long jb_api_random(void *out, unsigned long len)
{
    unsigned char *p = (unsigned char *)out;
    unsigned long left = len;
    long err = kJBNoErr;
    if (out == NULL) return kJBErrArg;
    if (!try_lock()) { memset(out, 0, len); return kJBErrBusy; }
    jb_engine_pump(&gEngine);
    while (left > 0) {
        unsigned long chunk = left > 4096UL ? 4096UL : left;
        int r = jb_engine_random(&gEngine, p, chunk);
        if (r != JB_OK) { err = map_err(r); break; }
        p += chunk;
        left -= chunk;
    }
    unlock();
    if (err != kJBNoErr) memset(out, 0, len);   /* fail closed */
    return err;
}

long jb_api_status(JBStatusRec *s)
{
    unsigned i;
    jb_u32 samples = 0;
    if (s == NULL) return kJBErrArg;
    memset(s, 0, sizeof *s);
    s->version = kJBAPIVersion;
    for (i = 0; i < JB_SRC_MAX; i++) samples += gEngine.seen[i];
    if (jb_engine_ready(&gEngine)) s->flags |= kJBStatusReady;
    s->flags |= kJBStatusPlaceholderCredits;    /* until docs/measurements/ exists */
    if (gSeedMixed) s->flags |= kJBStatusSeedFileMixed;
    if (gEngine.health_failures) s->flags |= kJBStatusHealthFailure;
    s->poolBits = jb_pool_credit_bits(&gEngine.pool);
    s->reseeds = gEngine.reseeds;
    s->samples = samples;
    s->healthFailures = gEngine.health_failures;
    s->ringDropped = gEngine.ring.dropped;
    s->busySkips = gBusySkips;
    return kJBNoErr;
}

long jb_api_stir(const void *data, unsigned long len)
{
    if (data == NULL && len != 0) return kJBErrArg;
    if (!try_lock()) return kJBErrBusy;
    jb_engine_stir(&gEngine, data, len);
    unlock();
    return kJBNoErr;
}

/* Multiversal declares NewGestalt with trap 0xA1AD, which is plain Gestalt
 * (so it "fails" with gestaltUndefSelectorErr). The real trap is 0xA3AD:
 * D0 = selector, A0 = selector function, result in D0. */
static OSErr new_gestalt(OSType selector, void *fn)
{
    register long d0 __asm__("d0") = (long)selector;
    register void *a0 __asm__("a0") = fn;
    __asm__ __volatile__(".short 0xA3AD" : "+r"(d0), "+r"(a0) : : "d1", "d2", "a1", "cc", "memory");
    return (OSErr)d0;
}

static pascal OSErr jb_gestalt(OSType selector, long *response)
{
    (void)selector;
    *response = (long)&gTable;
    return noErr;
}

/* ---- startup icon (ShowINIT protocol) ------------------------------------- */

static short parade_checksum(short h)
{
    return (short)((((unsigned short)h << 1) | ((unsigned short)h >> 15)) ^ 0x1021);
}

static void plot_parade(short iconID, short transform, int advance)
{
    struct { QDGlobals qd; long a5slot; } g;
    GrafPort port;
    long oldA5;
    short *hp = (short *)kLMShowINITH;
    short h;
    Rect r;

    oldA5 = SetA5((long)&g.a5slot);
    InitGraf(&g.qd.thePort);
    OpenPort(&port);
    h = (*(short *)kLMShowINITCheck == parade_checksum(*hp)) ? *hp : 8;
    r.bottom = port.portRect.bottom - 8;
    r.top = r.bottom - 32;
    r.left = h;
    r.right = h + 32;
    PlotIconID(&r, 0, transform, iconID);
    if (advance) {
        *hp = (short)(h + 40);
        *(short *)kLMShowINITCheck = parade_checksum(*hp);
    }
    ClosePort(&port);
    SetA5(oldA5);
}

static void show_icon(int ok)
{
    int i;
    unsigned long final;
    if (ok) {
        /* Both dance frames share the puzzle-piece mask, so each fully covers
         * the last. */
        for (i = 0; i < 6; i++) {
            plot_parade(i & 1 ? kIconDanceB : kIconDanceA, 0, 0);
            Delay(6, &final);
        }
        plot_parade(kIconMain, 0, 1);
    } else {
        plot_parade(kIconMain, kTransformDimmed, 1);
    }
}

/* ---- boot ------------------------------------------------------------------- */

/* Why install() gave up, for "Jitterbug Log" in Preferences. */
enum {
    kStepOK = 0, kStepAlreadyLoaded, kStepTimeMgr, kStepOwnResource, kStepGestalt
};
static int  gFailStep;
static long gFailDetail;

static char *put_dec(char *p, long v)
{
    char tmp[12];
    int n = 0;
    unsigned long u = (v < 0) ? (unsigned long)-v : (unsigned long)v;
    if (v < 0) *p++ = '-';
    do { tmp[n++] = (char)('0' + u % 10); u /= 10; } while (u);
    while (n) *p++ = tmp[--n];
    return p;
}

static char *put_str(char *p, const char *s)
{
    while (*s) *p++ = *s++;
    return p;
}

/* One line, replaced every boot: whether the extension installed and, if
 * not, which step failed with what detail (an OSErr or Gestalt value). */
static void write_startup_log(void)
{
    static const char *const kSteps[] = {
        "installed", "already loaded", "needs the extended Time Manager",
        "cannot find its own INIT resource", "NewGestalt failed"
    };
    char line[120], *p = line;
    FSSpec spec;
    short vref, ref;
    long dir, n;

    p = put_str(p, "Jitterbug 0.1d0: ");
    p = put_str(p, kSteps[gFailStep]);
    if (gFailStep != kStepOK) {
        p = put_str(p, " (");
        p = put_dec(p, gFailDetail);
        p = put_str(p, ")");
    }
    *p++ = '\r';
    n = p - line;

    if (FindFolder(kSystemDisk, kPreferencesFolderType, true, &vref, &dir) != noErr) return;
    if (FSMakeFSSpec(vref, dir, kLogName, &spec) == fnfErr)
        (void)FSpCreate(&spec, 'ttxt', 'TEXT', smSystemScript);
    if (FSpOpenDF(&spec, fsRdWrPerm, &ref) != noErr) return;
    if (FSWrite(ref, &n, line) == noErr) (void)SetEOF(ref, n);
    FSClose(ref);
}

static int fail(int step, long detail)
{
    gFailStep = step;
    gFailDetail = detail;
    return 0;
}

static int install(void)
{
    long response;
    jb_u32 boot[3];
    Handle self;
    OSErr err;

    if (Gestalt((OSType)kJBGestaltSelector, &response) == noErr)
        return fail(kStepAlreadyLoaded, response);
    err = Gestalt(gestaltTimeMgrVersion, &response);
    if (err != noErr) return fail(kStepTimeMgr, err);
    if (response < kExtendedTimeMgr) return fail(kStepTimeMgr, response);
    self = Get1Resource('INIT', 128);
    if (self == NULL) return fail(kStepOwnResource, ResError());

    jb_engine_init(&gEngine);
    jb_policy_apply_defaults(&gEngine);
    boot[0] = now_us();
    boot[1] = (jb_u32)TickCount();
    GetDateTime(&boot[2]);
    jb_engine_stir(&gEngine, boot, sizeof boot);
    seed_load_and_forward();

    gTable.version = kJBAPIVersion;
    gTable.size = sizeof gTable;
    gTable.randomBytes = (void *)jb_api_random;
    gTable.status = (void *)jb_api_status;
    gTable.stir = (void *)jb_api_stir;
    err = new_gestalt((OSType)kJBGestaltSelector, (void *)jb_gestalt);
    if (err != noErr) return fail(kStepGestalt, err);

    /* Nothing below can fail, so the code must now stay resident. */
    HLock(self);
    HNoPurge(self);
    DetachResource(self);

    memset(&gTimer, 0, sizeof gTimer);
    gTimer.tmAddr = (ProcPtr)jb_timer_stub;
    InsXTime((QElemPtr)&gTimer);
    PrimeTime((QElemPtr)&gTimer, kTimerIntervalMs);

    jb_old_filter = *(unsigned long *)kLMJGNEFilter;
    *(unsigned long *)kLMJGNEFilter = (unsigned long)jb_filter_stub;

    ShutDwnInstall((ProcPtr)jb_shutdown_stub, sdOnUnmount);
    return 1;
}

void _start(void)
{
    int ok;
    RETRO68_RELOCATE();
    Retro68CallConstructors();
    ok = install();
    show_icon(ok);
    write_startup_log();
    if (!ok) Retro68FreeGlobals();
}
