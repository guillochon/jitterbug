/* jb_sample.h - the raw observation record shared by real and simulated
 * sources, the interrupt-time ring buffer, and the .jbr trace format. */
#ifndef JB_SAMPLE_H
#define JB_SAMPLE_H
#include "jb_types.h"

/* Source IDs are stable: they appear in .jbr files recorded on hardware. */
enum {
    JB_SRC_TIMER_JITTER = 1,  /* Time Manager / tick edge vs time base     */
    JB_SRC_EVENT        = 2,  /* mouse/key/ADB event arrival (jGNEFilter)   */
    JB_SRC_DISK         = 3,  /* disk I/O completion timing                 */
    JB_SRC_NET          = 4,  /* Open Transport packet arrival              */
    JB_SRC_AUDIO        = 5,  /* sound-input LSB noise                      */
    JB_SRC_US_TB_BEAT   = 6,  /* Microseconds() vs time base beat           */
    JB_SRC_MAX          = 16
};

/* How the health tests and the NIST extractor turn a sample into a symbol. */
enum {
    JB_SYM_DELTA_TS = 0,      /* low 8 bits of timestamp delta (timing srcs) */
    JB_SYM_VALUE    = 1       /* low 8 bits of value (audio, etc.)           */
};

typedef struct {
    jb_u8  source;            /* JB_SRC_*                                    */
    jb_u8  flags;             /* reserved, 0                                 */
    jb_u32 ts;                /* low 32 bits of the PPC time base            */
    jb_u32 value;             /* source-specific payload                     */
} jb_sample;

/* Single-producer / single-consumer ring. The producer may run at
 * interrupt time (VBL, Time Manager, jGNEFilter, OT notifier); it never
 * allocates or blocks. The consumer runs at task time. On 32-bit PPC an
 * aligned word store is atomic, which is all this needs. */
#define JB_RING_SIZE 1024u    /* power of two */

typedef struct {
    jb_sample        slot[JB_RING_SIZE];
    volatile jb_u32  head;    /* written only by producer */
    volatile jb_u32  tail;    /* written only by consumer */
    volatile jb_u32  dropped; /* producer-side overflow counter */
} jb_ring;

void jb_ring_init(jb_ring *r);
/* Interrupt-safe. Returns 0 if the ring was full (sample dropped). */
int  jb_ring_push(jb_ring *r, jb_u8 source, jb_u32 ts, jb_u32 value);
/* Task time only. Returns 0 if empty. */
int  jb_ring_pop(jb_ring *r, jb_sample *out);

#endif
