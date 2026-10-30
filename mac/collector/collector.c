/* JitterbugCollector - milestone 1 hardware probe for the B&W G3.
 *
 * Records raw observations to "Jitterbug Trace" (a .jbr file, same format as
 * jb_sim dump) in the application's folder, for offline analysis on the dev
 * machine with tools/jbr2nist.py and NIST ea_non_iid, and for replay through
 * the real pipeline with `jb_sim replay`.
 *
 * Sources recorded (all at task time; interrupt-time hooks come with the INIT):
 *   event        - time-base stamp of every non-null event from WaitNextEvent
 *   timer-jitter - time base at each observed TickCount edge (60.15 Hz VBL)
 *   us-tb-beat   - Microseconds() sampled against the time base
 *
 * STATUS: builds with Retro68 + Multiversal Interfaces (PPC and 68K) but NOT
 * YET RUN on hardware.
 * Quit with Cmd-Q, 'q', or the close box. */

#include <Quickdraw.h>
#include <Windows.h>
#include <Menus.h>
#include <Fonts.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Events.h>
#include <Timer.h>
#include <stdio.h>
#include <string.h>

#include "jb_sample.h"
#include "jb_trace.h"

#define TRACE_NAME   "Jitterbug Trace"
#define BUF_SAMPLES  2048
#define BEAT_EVERY   64      /* loop iterations between us-tb-beat samples */

static jb_sample gBuf[BUF_SAMPLES];
static int       gBufN = 0;
static FILE     *gOut = NULL;
static unsigned long gCount[JB_SRC_MAX];

#if defined(_ARCH_PPC) || defined(__powerpc__) || defined(__ppc__)
#define JB_PPC 1
#endif

/* Multiversal Interfaces omit Microseconds() and UnsignedWide, so declare
 * them here. PPC: InterfaceLib exports Microseconds. 68K: trap _Microseconds
 * (0xA193) returns the high word in A0 and the low word in D0. */
typedef struct { unsigned long hi, lo; } JBWide;

#ifdef JB_PPC
extern void Microseconds(JBWide *microTickCount);
#define jb_microseconds Microseconds
#else
static void jb_microseconds(JBWide *us)
{
    register unsigned long hi __asm__("a0");
    register unsigned long lo __asm__("d0");
    __asm__ __volatile__(".short 0xA193" : "=r"(hi), "=r"(lo) : : "d1", "d2", "a1", "cc", "memory");
    us->hi = hi;
    us->lo = lo;
}
#endif

/* Low 32 bits of the PowerPC time base (25 MHz on a 100 MHz-bus B&W G3).
 * The 601 has no time base, but a G3 always does. On 68K builds fall back
 * to the low word of Microseconds(). */
static jb_u32 read_tb(void)
{
#ifdef JB_PPC
    jb_u32 v;
    __asm__ __volatile__("mftb %0" : "=r"(v));
    return v;
#else
    JBWide us;
    jb_microseconds(&us);
    return (jb_u32)us.lo;
#endif
}

static void flush_buf(void)
{
    int i;
    if (!gOut) return;
    for (i = 0; i < gBufN; i++) jb_trace_write(gOut, &gBuf[i]);
    gBufN = 0;
    fflush(gOut);
}

static void record(jb_u8 src, jb_u32 ts, jb_u32 value)
{
    gBuf[gBufN].source = src;
    gBuf[gBufN].flags = 0;
    gBuf[gBufN].ts = ts;
    gBuf[gBufN].value = value;
    gCount[src]++;
    if (++gBufN == BUF_SAMPLES) flush_buf();
}

/* Pascal-string number formatting without relying on NumToString's header. */
static void draw_line(short y, const char *label, unsigned long n)
{
    char buf[64];
    Str255 ps;
    size_t len;
    sprintf(buf, "%-14s %10lu", label, n);
    len = strlen(buf);
    ps[0] = (unsigned char)len;
    memcpy(ps + 1, buf, len);
    MoveTo(12, y);
    DrawString(ps);
}

static void redraw(WindowPtr w)
{
    SetPort(w);
    EraseRect(&w->portRect);
    TextFont(4); TextSize(9);            /* Monaco 9 */
    draw_line(20, "event",        gCount[JB_SRC_EVENT]);
    draw_line(34, "timer-jitter", gCount[JB_SRC_TIMER_JITTER]);
    draw_line(48, "us-tb-beat",   gCount[JB_SRC_US_TB_BEAT]);
    MoveTo(12, 70);
    DrawString(gOut ? "\pRecording to \"Jitterbug Trace\". Q to stop."
                    : "\pERROR: could not open trace file.");
}

int main(void)
{
    WindowPtr win;
    Rect r;
    EventRecord ev;
    jb_u8 srcs[3];
    unsigned long lastTick, now, iter = 0, lastDraw = 0;
    int running = 1;
    JBWide us;

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();

    SetRect(&r, 40, 60, 360, 150);
    win = NewWindow(NULL, &r, "\pJitterbug Collector", true, documentProc,
                    (WindowPtr)-1, true, 0);

    gOut = fopen(TRACE_NAME, "wb");
    if (gOut) {
        srcs[0] = JB_SRC_EVENT; srcs[1] = JB_SRC_TIMER_JITTER; srcs[2] = JB_SRC_US_TB_BEAT;
        jb_trace_write_header(gOut, 25000000UL, srcs, 3);
    }
    redraw(win);

    lastTick = TickCount();
    while (running) {
        /* sleep = 0: poll as fast as possible so tick edges are seen promptly.
         * This deliberately hogs the CPU; it is a measurement tool. */
        if (WaitNextEvent(everyEvent, &ev, 0, NULL)) {
            jb_u32 t = read_tb();
            record(JB_SRC_EVENT, t, ((jb_u32)(jb_u16)ev.where.h << 16) | (jb_u16)ev.where.v);
            switch (ev.what) {
            case keyDown: {
                char c = (char)(ev.message & charCodeMask);
                if (c == 'q' || c == 'Q') running = 0;
                break; }
            case mouseDown: {
                WindowPtr hit;
                short part = FindWindow(ev.where, &hit);
                if (part == inGoAway && hit == win && TrackGoAway(win, ev.where)) running = 0;
                else if (part == inDrag) DragWindow(hit, ev.where, &qd.screenBits.bounds);
                break; }
            case updateEvt:
                BeginUpdate((WindowPtr)ev.message);
                redraw((WindowPtr)ev.message);
                EndUpdate((WindowPtr)ev.message);
                break;
            }
        }

        now = TickCount();
        if (now != lastTick) {
            record(JB_SRC_TIMER_JITTER, read_tb(), (jb_u32)now);
            lastTick = now;
        }
        if (++iter % BEAT_EVERY == 0) {
            jb_microseconds(&us);
            record(JB_SRC_US_TB_BEAT, read_tb(), (jb_u32)us.lo);
        }
        if (now - lastDraw >= 30) { redraw(win); lastDraw = now; }
    }

    flush_buf();
    if (gOut) fclose(gOut);
    return 0;
}
