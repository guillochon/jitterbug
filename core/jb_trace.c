/* jb_trace.c - C89 stdio; works under Retro68's libretro file I/O. */
#include <string.h>
#include "jb_trace.h"
#include "jb_policy.h"

int jb_trace_write_header(FILE *f, jb_u32 tb_hz, const jb_u8 *srcs, int nsrc)
{
    jb_u8 b[4];
    int i;
    if (fwrite("JBR1", 1, 4, f) != 4) return -1;
    JB_STORE32BE(b, tb_hz);
    if (fwrite(b, 1, 4, f) != 4) return -1;
    fputc(nsrc, f);
    for (i = 0; i < nsrc; i++) {
        const char *n = jb_source_name(srcs[i]);
        size_t len = strlen(n);
        fputc(srcs[i], f);
        fputc(jb_source_symkind(srcs[i]), f);
        fputc((int)len, f);
        fwrite(n, 1, len, f);
    }
    return ferror(f) ? -1 : 0;
}

int jb_trace_write(FILE *f, const jb_sample *s)
{
    jb_u8 r[10];
    r[0] = s->source; r[1] = s->flags;
    JB_STORE32BE(r + 2, s->ts);
    JB_STORE32BE(r + 6, s->value);
    return fwrite(r, 1, 10, f) == 10 ? 0 : -1;
}

int jb_trace_read_header(FILE *f, jb_u32 *tb_hz)
{
    jb_u8 b[4];
    int n, i, len;
    if (fread(b, 1, 4, f) != 4 || memcmp(b, "JBR1", 4) != 0) return -1;
    if (fread(b, 1, 4, f) != 4) return -1;
    *tb_hz = JB_LOAD32BE(b);
    n = fgetc(f);
    if (n == EOF) return -1;
    for (i = 0; i < n; i++) {
        fgetc(f); fgetc(f);
        len = fgetc(f);
        if (len == EOF || fseek(f, len, SEEK_CUR) != 0) return -1;
    }
    return 0;
}

int jb_trace_read(FILE *f, jb_sample *s)
{
    jb_u8 r[10];
    if (fread(r, 1, 10, f) != 10) return 0;
    s->source = r[0]; s->flags = r[1];
    s->ts = JB_LOAD32BE(r + 2);
    s->value = JB_LOAD32BE(r + 6);
    return 1;
}
