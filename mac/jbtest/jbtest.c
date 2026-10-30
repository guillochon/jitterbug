/* JitterbugTest - checks the Jitterbug extension through the public glue.
 *
 * Writes "Jitterbug Test Log" next to itself: whether the extension is
 * present, its status once a second, and the results of two JBRandomBytes
 * calls once it reports ready (never the bytes themselves). Gives up after
 * TIMEOUT_S seconds.
 *
 * If a file named "Jitterbug AutoTest" sits next to the app, it powers the
 * Mac off when done; scripts/test-qemu-os9.sh uses that from Startup Items.
 * Otherwise quit with Cmd-Q, 'q', or the close box.
 *
 * MIT License, Copyright (c) 2026 James Guillochon. See LICENSE. */

#include <Quickdraw.h>
#include <Windows.h>
#include <Menus.h>
#include <Fonts.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Events.h>
#include <Multiverse.h>   /* ShutDwnPower */
#include <stdio.h>
#include <string.h>

#include "Jitterbug.h"

#define LOG_NAME   "Jitterbug Test Log"
#define AUTO_NAME  "Jitterbug AutoTest"
#define TIMEOUT_S  180

static FILE *gLog;
static char  gLine[4][80];

static void say(int line, const char *text)
{
    strncpy(gLine[line], text, sizeof gLine[line] - 1);
    if (gLog) { fprintf(gLog, "%s\n", text); fflush(gLog); }
}

static void redraw(WindowPtr w)
{
    int i;
    SetPort(w);
    EraseRect(&w->portRect);
    TextFont(4); TextSize(9);
    for (i = 0; i < 4; i++) {
        Str255 ps;
        size_t n = strlen(gLine[i]);
        ps[0] = (unsigned char)n;
        memcpy(ps + 1, gLine[i], n);
        MoveTo(10, (short)(16 + 14 * i));
        DrawString(ps);
    }
}

static void log_status(unsigned long secs)
{
    JBStatusRec s;
    char buf[160];
    OSStatus err = JBStatus(&s);
    sprintf(buf, "t=%lus status=%ld flags=%08lx pool=%lu reseeds=%lu samples=%lu health=%lu dropped=%lu busy=%lu",
            secs, (long)err, (unsigned long)s.flags, (unsigned long)s.poolBits,
            (unsigned long)s.reseeds, (unsigned long)s.samples,
            (unsigned long)s.healthFailures, (unsigned long)s.ringDropped,
            (unsigned long)s.busySkips);
    say(1, buf);
}

int main(void)
{
    WindowPtr win;
    Rect r;
    EventRecord ev;
    unsigned long start, lastSec = (unsigned long)-1, secs;
    int running = 1, done = 0, autotest;
    FILE *f;
    char buf[160];

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();

    SetRect(&r, 40, 60, 560, 130);
    win = NewWindow(NULL, &r, "\pJitterbug Test", true, documentProc, (WindowPtr)-1, true, 0);

    f = fopen(AUTO_NAME, "r");
    autotest = (f != NULL);
    if (f) fclose(f);
    gLog = fopen(LOG_NAME, "w");

    say(0, JBIsPresent() ? "PRESENT: Jitterbug extension found (API v1)"
                         : "ABSENT: Jitterbug extension not installed");
    if (!JBIsPresent()) done = 1;

    start = TickCount();
    while (running) {
        if (WaitNextEvent(everyEvent, &ev, 1, NULL)) {
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
        secs = (TickCount() - start) / 60;
        if (!done && secs != lastSec) {
            JBStatusRec s;
            lastSec = secs;
            log_status(secs);
            JBStatus(&s);
            if (s.flags & kJBStatusReady) {
                unsigned char a[32], b[32];
                OSStatus e1 = JBRandomBytes(a, sizeof a);
                OSStatus e2 = JBRandomBytes(b, sizeof b);
                OSStatus e3 = JBStir("JitterbugTest", 13);
                sprintf(buf, "%s: random=%ld,%ld differ=%d stir=%ld",
                        (e1 == kJBNoErr && e2 == kJBNoErr && memcmp(a, b, sizeof a) != 0 && e3 == kJBNoErr)
                            ? "PASS" : "FAIL",
                        (long)e1, (long)e2, memcmp(a, b, sizeof a) != 0, (long)e3);
                say(2, buf);
                memset(a, 0, sizeof a);
                memset(b, 0, sizeof b);
                done = 1;
            } else if (secs >= TIMEOUT_S) {
                say(2, "FAIL: not ready before timeout");
                done = 1;
            }
            redraw(win);
        }
        if (done && autotest) running = 0;
    }

    say(3, "END");
    if (gLog) fclose(gLog);
    if (autotest) ShutDwnPower();
    return 0;
}
