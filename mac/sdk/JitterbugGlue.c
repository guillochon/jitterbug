/* JitterbugGlue.c - find the Jitterbug extension and call it. C89.
 * MIT License, Copyright (c) 2026 James Guillochon. See LICENSE. */
#include <Gestalt.h>
#include <MixedMode.h>
#include <string.h>
#include "Jitterbug.h"

#if defined(_ARCH_PPC) || defined(__powerpc__) || defined(__ppc__) || defined(powerc)
#define JB_GLUE_PPC 1
#endif

static JBDispatchTable *jb_table(void)
{
    long response;
    JBDispatchTable *t;
    if (Gestalt((OSType)kJBGestaltSelector, &response) != noErr || response == 0)
        return NULL;
    t = (JBDispatchTable *)response;
    if (t->version < kJBAPIVersion || t->size < sizeof(JBDispatchTable))
        return NULL;
    return t;
}

#ifdef JB_GLUE_PPC
/* Apple's MixedMode.h macros; Retro68's Multiversal Interfaces lack them. */
#ifndef SIZE_CODE
#define SIZE_CODE(size) (((size) == 4) ? kFourByteCode : ((size) == 2) ? kTwoByteCode : \
                         ((size) == 1) ? kOneByteCode : kNoByteCode)
#endif
#ifndef RESULT_SIZE
#define RESULT_SIZE(sizeCode) ((ProcInfoType)(sizeCode) << kCallingConventionWidth)
#endif

/* long f(void *, unsigned long) and long f(void *), as 68K C stack calls. */
enum {
    kJBTwoArgProcInfo = kCStackBased
        | RESULT_SIZE(SIZE_CODE(sizeof(long)))
        | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(void *)))
        | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(unsigned long))),
    kJBOneArgProcInfo = kCStackBased
        | RESULT_SIZE(SIZE_CODE(sizeof(long)))
        | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(void *)))
};
#define JB_CALL2(p, a, b) ((OSStatus)CallUniversalProc((UniversalProcPtr)(p), kJBTwoArgProcInfo, (a), (b)))
#define JB_CALL1(p, a)    ((OSStatus)CallUniversalProc((UniversalProcPtr)(p), kJBOneArgProcInfo, (a)))
#else
typedef long (*JBTwoArgProc)(void *, unsigned long);
typedef long (*JBOneArgProc)(void *);
#define JB_CALL2(p, a, b) ((OSStatus)((JBTwoArgProc)(p))((void *)(a), (unsigned long)(b)))
#define JB_CALL1(p, a)    ((OSStatus)((JBOneArgProc)(p))((void *)(a)))
#endif

Boolean JBIsPresent(void)
{
    return jb_table() != NULL;
}

OSStatus JBRandomBytes(void *out, UInt32 len)
{
    JBDispatchTable *t = jb_table();
    if (out == NULL) return kJBErrArg;
    if (t == NULL) {
        memset(out, 0, len);
        return kJBErrNotPresent;
    }
    return JB_CALL2(t->randomBytes, out, len);
}

OSStatus JBStatus(JBStatusRec *status)
{
    JBDispatchTable *t = jb_table();
    if (status == NULL) return kJBErrArg;
    memset(status, 0, sizeof *status);
    if (t == NULL) return kJBErrNotPresent;
    return JB_CALL1(t->status, status);
}

OSStatus JBStir(const void *data, UInt32 len)
{
    JBDispatchTable *t = jb_table();
    if (data == NULL && len != 0) return kJBErrArg;
    if (t == NULL) return kJBErrNotPresent;
    return JB_CALL2(t->stir, data, len);
}
