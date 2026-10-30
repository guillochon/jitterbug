/* jb_trace.h - .jbr raw-sample trace files. Written by the Mac collector
 * (real hardware) and by jb_sim (simulated); read by jb_sim replay and
 * tools/jbr2nist.py. All integers big-endian.
 *
 *   "JBR1" | u32 timebase_hz | u8 nsrc | nsrc x (u8 id, u8 symkind, u8 len, name)
 *   then records: u8 source | u8 flags | u32 ts | u32 value   (10 bytes each)
 */
#ifndef JB_TRACE_H
#define JB_TRACE_H
#include <stdio.h>
#include "jb_sample.h"

int jb_trace_write_header(FILE *f, jb_u32 tb_hz, const jb_u8 *srcs, int nsrc);
int jb_trace_write(FILE *f, const jb_sample *s);
int jb_trace_read_header(FILE *f, jb_u32 *tb_hz);
int jb_trace_read(FILE *f, jb_sample *s);   /* 1 = got one, 0 = EOF */

#endif
