/* jb_types.h - fixed-width types for C89 targets (Retro68 PPC/68K and host). */
#ifndef JB_TYPES_H
#define JB_TYPES_H

#include <limits.h>
#include <stddef.h>

typedef unsigned char  jb_u8;
typedef unsigned short jb_u16;

#if UINT_MAX == 0xFFFFFFFFUL
typedef unsigned int jb_u32;
#elif ULONG_MAX == 0xFFFFFFFFUL
typedef unsigned long jb_u32;
#else
#error "Jitterbug needs a 32-bit unsigned type"
#endif

#define JB_OK              0
#define JB_ERR_NOT_READY  -1   /* not enough credited entropy yet          */
#define JB_ERR_HEALTH     -2   /* a health test failed                     */
#define JB_ERR_ARG        -3
#define JB_ERR_RESEED     -4   /* DRBG reseed interval exhausted           */

/* Big-endian load/store helpers (the Mac is big-endian; files use BE). */
#define JB_LOAD32BE(p) (((jb_u32)(p)[0] << 24) | ((jb_u32)(p)[1] << 16) | \
                        ((jb_u32)(p)[2] << 8)  |  (jb_u32)(p)[3])
#define JB_STORE32BE(p, v) do { (p)[0] = (jb_u8)((v) >> 24); \
                                (p)[1] = (jb_u8)((v) >> 16); \
                                (p)[2] = (jb_u8)((v) >> 8);  \
                                (p)[3] = (jb_u8)(v); } while (0)

#endif
