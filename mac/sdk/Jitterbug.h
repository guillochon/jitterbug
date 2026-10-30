/* Jitterbug.h - public interface to the Jitterbug system extension.
 *
 * Copy Jitterbug.h and JitterbugGlue.c into your project. They are C89,
 * build for 68K and PowerPC (Retro68, CodeWarrior, MPW), and add no link-time
 * dependency: when the extension is not installed every call returns
 * kJBErrNotPresent and your app keeps working.
 *
 * Fail closed: JBRandomBytes never returns weak output. Until the extension
 * has gathered enough credited entropy it returns kJBErrNotReady and zeroes
 * the buffer. Check the result of every call.
 *
 * While kJBStatusPlaceholderCredits is set, the per-source credits have not
 * been measured on real hardware (see docs/ENTROPY_POLICY.md). Treat output
 * as experimental, or mix it in with zero credit, until that flag clears.
 *
 * MIT License, Copyright (c) 2026 James Guillochon. See LICENSE. */
#ifndef JITTERBUG_H
#define JITTERBUG_H

#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Gestalt selector. Its response is the address of a JBDispatchTable. */
#define kJBGestaltSelector  0x4A427567UL   /* 'JBug' */
#define kJBAPIVersion       1UL

/* Results. Chosen outside the ranges Apple's managers use. */
enum {
    kJBNoErr          = 0,
    kJBErrNotReady    = -29001,   /* not enough credited entropy yet         */
    kJBErrHealth      = -29002,   /* every source failed its health test     */
    kJBErrArg         = -29003,
    kJBErrReseed      = -29004,   /* DRBG needs a reseed; try again shortly  */
    kJBErrBusy        = -29005,   /* called re-entrantly; try again          */
    kJBErrNotPresent  = -29006,   /* extension not installed (glue only)     */
    kJBErrVersion     = -29007    /* extension too old for this glue         */
};

/* JBStatusRec.flags */
#define kJBStatusReady              0x00000001UL  /* JBRandomBytes will succeed */
#define kJBStatusPlaceholderCredits 0x00000002UL  /* credits not yet measured   */
#define kJBStatusSeedFileMixed      0x00000004UL  /* boot seed file was mixed (never credited) */
#define kJBStatusHealthFailure      0x00000008UL  /* a source has failed a health test */

/* Fixed layout: every field is 32 bits, so 68K and PPC compilers agree. */
typedef struct JBStatusRec {
    UInt32 version;         /* kJBAPIVersion                                 */
    UInt32 flags;           /* kJBStatus*                                    */
    UInt32 poolBits;        /* credited bits waiting in the pool             */
    UInt32 reseeds;         /* DRBG (re)seeds since boot                     */
    UInt32 samples;         /* samples absorbed since boot                   */
    UInt32 healthFailures;  /* health-test failures since boot               */
    UInt32 ringDropped;     /* interrupt-time samples lost to a full ring    */
    UInt32 busySkips;       /* event samples skipped while busy              */
} JBStatusRec;

/* What the Gestalt selector points at. The procedures are 68K code using the
 * C stack convention (arguments pushed right to left, caller pops, long
 * result in D0); PowerPC callers reach them through the Mixed Mode Manager.
 * Use the glue functions below rather than calling these directly. */
typedef struct JBDispatchTable {
    UInt32 version;         /* kJBAPIVersion                                 */
    UInt32 size;            /* sizeof(JBDispatchTable) in the extension      */
    void  *randomBytes;     /* long (void *out, unsigned long len)           */
    void  *status;          /* long (JBStatusRec *s)                         */
    void  *stir;            /* long (const void *data, unsigned long len)    */
} JBDispatchTable;

/* True if the extension is installed and speaks this version. */
Boolean  JBIsPresent(void);
/* Fill out with len random bytes, or fail closed (buffer zeroed). */
OSStatus JBRandomBytes(void *out, UInt32 len);
/* Read the extension's state. */
OSStatus JBStatus(JBStatusRec *status);
/* Contribute your own timing or state. Mixed in, never credited. */
OSStatus JBStir(const void *data, UInt32 len);

#ifdef __cplusplus
}
#endif

#endif
