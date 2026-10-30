/* jb_ring.c - SPSC ring; no allocation, no Toolbox calls (ISR-safe). C89. */
#include "jb_sample.h"

void jb_ring_init(jb_ring *r)
{
    r->head = r->tail = r->dropped = 0;
}

int jb_ring_push(jb_ring *r, jb_u8 source, jb_u32 ts, jb_u32 value)
{
    jb_u32 h = r->head;
    jb_sample *s;
    if ((jb_u32)(h - r->tail) >= JB_RING_SIZE) { r->dropped++; return 0; }
    s = &r->slot[h & (JB_RING_SIZE - 1)];
    s->source = source; s->flags = 0; s->ts = ts; s->value = value;
    r->head = h + 1;          /* publish after the slot is written */
    return 1;
}

int jb_ring_pop(jb_ring *r, jb_sample *out)
{
    jb_u32 t = r->tail;
    if (t == r->head) return 0;
    *out = r->slot[t & (JB_RING_SIZE - 1)];
    r->tail = t + 1;
    return 1;
}
