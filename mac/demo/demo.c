/* Jitterbug Demo - a small app that shows how to use the Jitterbug extension.
 *
 * Push a button and the app asks the extension for random bytes, then turns
 * them into a coin flip, a die roll, a 128-bit key or a password. The status
 * line underneath shows what the extension reports once a second.
 *
 * Everything an integrator needs is in the "Using Jitterbug" section below;
 * the rest is ordinary Toolbox code for the window, buttons and menus. To use
 * Jitterbug in your own app, copy mac/sdk/Jitterbug.h and JitterbugGlue.c
 * into your project and follow the same pattern:
 *
 *   1. JBIsPresent() / JBStatus(): is the extension installed and seeded?
 *   2. JBRandomBytes(): check the result EVERY time. It fails closed (and
 *      zeroes your buffer) rather than return weak output.
 *   3. Turn bytes into numbers without modulo bias (RandomBelow below).
 *   4. Optionally JBStir() your own event timing in. It is mixed, never
 *      credited, so it can only help.
 *   5. Zero buffers that held secrets when you are done with them.
 *
 * Builds for 68K and PowerPC. Written in C89 so it can be copied into
 * CodeWarrior or MPW projects as well as Retro68 ones.
 *
 * MIT License, Copyright (c) 2026 James Guillochon. See LICENSE. */

#include <Quickdraw.h>
#include <Windows.h>
#include <Menus.h>
#include <Fonts.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Events.h>
#include <ToolUtils.h>
#include <Devices.h>
/* Retro68's Multiversal Interfaces have no Controls.h; Multiverse.h holds
 * the Control Manager. Apple's Universal Interfaces are the other way round. */
#if defined(__has_include)
#if __has_include(<Multiverse.h>)
#include <Multiverse.h>
#define JB_DEMO_MULTIVERSE 1
#endif
#endif
#ifndef JB_DEMO_MULTIVERSE
#include <Controls.h>
#endif
#include <stdio.h>
#include <string.h>

#include "Jitterbug.h"

/* ------------------------------------------------------------------------ */
/* Using Jitterbug                                                           */
/* ------------------------------------------------------------------------ */

/* A short explanation for each result the glue can return. */
static const char *ErrorText(OSStatus err)
{
    switch (err) {
    case kJBNoErr:         return "noErr";
    case kJBErrNotReady:   return "not ready: still gathering entropy";
    case kJBErrHealth:     return "every entropy source failed its health test";
    case kJBErrArg:        return "bad argument";
    case kJBErrReseed:     return "reseeding; try again shortly";
    case kJBErrBusy:       return "busy; try again";
    case kJBErrNotPresent: return "extension not installed";
    case kJBErrVersion:    return "extension too old for this app";
    default:               return "unexpected error";
    }
}

/* A uniform integer in [0, n), n > 0. Taking (random % n) would favour small
 * values whenever n does not divide 2^32, so draws below 2^32 mod n are
 * rejected and redrawn (fewer than half of all draws, for any n). */
static OSStatus RandomBelow(UInt32 n, UInt32 *out)
{
    UInt32 reject = (UInt32)(0UL - n) % n;   /* 2^32 mod n */
    unsigned char b[4];
    UInt32 v;
    OSStatus err;

    do {
        err = JBRandomBytes(b, sizeof b);
        if (err != kJBNoErr) return err;
        v = ((UInt32)b[0] << 24) | ((UInt32)b[1] << 16)
          | ((UInt32)b[2] << 8)  |  (UInt32)b[3];
    } while (v < reject);
    *out = v % n;
    return kJBNoErr;
}

/* What the window shows: a headline and the API call behind it. */
static char gResult[96];
static char gDetail[96];

static void Fail(const char *call, OSStatus err)
{
    strcpy(gResult, "No random numbers right now.");
    sprintf(gDetail, "%s returned %ld (%s)", call, (long)err, ErrorText(err));
}

static void FlipCoin(void)
{
    unsigned char b;
    OSStatus err = JBRandomBytes(&b, 1);
    if (err != kJBNoErr) { Fail("JBRandomBytes", err); return; }
    strcpy(gResult, (b & 1) ? "Heads" : "Tails");
    strcpy(gDetail, "JBRandomBytes(1 byte), low bit");
}

static void RollDie(void)
{
    UInt32 v;
    OSStatus err = RandomBelow(6, &v);
    if (err != kJBNoErr) { Fail("JBRandomBytes", err); return; }
    sprintf(gResult, "You rolled a %lu", (unsigned long)v + 1);
    strcpy(gDetail, "RandomBelow(6) + 1, unbiased");
}

static void MakeKey(void)
{
    static const char hex[] = "0123456789ABCDEF";
    unsigned char key[16];
    char *p = gResult;
    int i;
    OSStatus err = JBRandomBytes(key, sizeof key);
    if (err != kJBNoErr) { Fail("JBRandomBytes", err); return; }
    for (i = 0; i < (int)sizeof key; i++) {
        if (i && (i % 4) == 0) *p++ = ' ';
        *p++ = hex[key[i] >> 4];
        *p++ = hex[key[i] & 15];
    }
    *p = 0;
    memset(key, 0, sizeof key);   /* don't leave secrets lying around */
    strcpy(gDetail, "JBRandomBytes(16 bytes): a 128-bit key");
}

static void MakePassword(void)
{
    /* 62 symbols, 16 of them: 16 * log2(62) = 95 bits. */
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    char pw[17];
    UInt32 v;
    int i;
    for (i = 0; i < 16; i++) {
        OSStatus err = RandomBelow(sizeof alphabet - 1, &v);
        if (err != kJBNoErr) { memset(pw, 0, sizeof pw); Fail("JBRandomBytes", err); return; }
        pw[i] = alphabet[v];
    }
    pw[16] = 0;
    strcpy(gResult, pw);
    memset(pw, 0, sizeof pw);
    strcpy(gDetail, "16 x RandomBelow(62): about 95 bits");
}

/* One line describing the extension, and whether the buttons should work. */
static Boolean DescribeStatus(char *line)
{
    JBStatusRec s;
    OSStatus err = JBStatus(&s);

    if (err == kJBErrNotPresent) {
        strcpy(line, "Jitterbug is not installed. Put it in the Extensions folder and restart.");
        return false;
    }
    if (err != kJBNoErr) {
        sprintf(line, "JBStatus returned %ld (%s)", (long)err, ErrorText(err));
        return false;
    }
    if (!(s.flags & kJBStatusReady)) {
        sprintf(line, "Gathering entropy... %lu samples so far. Move the mouse or type to help.",
                (unsigned long)s.samples);
        return false;
    }
    sprintf(line, "Ready. %lu samples, %lu reseeds%s",
            (unsigned long)s.samples, (unsigned long)s.reseeds,
            (s.flags & kJBStatusPlaceholderCredits)
                ? ". Experimental: credits not yet measured." : ".");
    return true;
}

/* Your own events carry timing too. JBStir mixes them in with zero credit;
 * the result is ignored because stirring is only ever a bonus. */
static void StirEvent(const EventRecord *ev)
{
    UInt32 buf[2];
    buf[0] = ev->when;
    buf[1] = ((UInt32)(UInt16)ev->where.v << 16) | (UInt16)ev->where.h;
    (void)JBStir(buf, sizeof buf);
}

/* ------------------------------------------------------------------------ */
/* Window, buttons and menus                                                 */
/* ------------------------------------------------------------------------ */

enum { kAppleMenu = 128, kFileMenu = 129 };
enum { kAboutItem = 1, kQuitItem = 1 };
enum { kNumButtons = 4 };

static WindowPtr     gWin;
static ControlHandle gButtons[kNumButtons];
static void        (*gActions[kNumButtons])(void) = { FlipCoin, RollDie, MakeKey, MakePassword };
static char          gStatus[128];
static Boolean       gReady;
static Boolean       gActive = true;   /* window is frontmost */
static Rect          gResultBox, gDetailBox, gStatusBox;

static void DrawBox(const char *text, const Rect *box, short font, short size, short just)
{
    TextFont(font);
    TextSize(size);
    TETextBox((Ptr)text, (long)strlen(text), box, just);
}

static void DrawContent(void)
{
    Rect inner = gResultBox;
    SetPort(gWin);
    EraseRect(&gWin->portRect);
    DrawControls(gWin);
    FrameRect(&gResultBox);
    InsetRect(&inner, 8, 28);
    DrawBox(gResult, &inner, 4 /* Monaco */, 12, teCenter);
    DrawBox(gDetail, &gDetailBox, 3 /* Geneva */, 9, teCenter);
    DrawBox(gStatus, &gStatusBox, 3, 9, teFlushLeft);
}

/* Buttons work only while the extension is ready and the window is in front. */
static void HiliteButtons(void)
{
    int i;
    for (i = 0; i < kNumButtons; i++)
        HiliteControl(gButtons[i], (gReady && gActive) ? 0 : 255);
}

/* Refresh the status line, and dim the buttons while there is nothing to
 * give. Only redraws when something changed, so the window doesn't flicker. */
static void PollStatus(void)
{
    char line[sizeof gStatus];
    Boolean ready = DescribeStatus(line);
    if (ready == gReady && strcmp(line, gStatus) == 0) return;
    strcpy(gStatus, line);
    if (ready != gReady) {
        gReady = ready;
        HiliteButtons();
    }
    SetPort(gWin);
    InvalRect(&gStatusBox);
}

static void SetupWindow(void)
{
    static const unsigned char *titles[kNumButtons] = {
        "\pFlip Coin", "\pRoll Die", "\p128-bit Key", "\pPassword"
    };
    Rect r;
    int i;

    SetRect(&r, 40, 60, 40 + 432, 60 + 204);
    gWin = NewWindow(NULL, &r, "\pJitterbug Demo", true, noGrowDocProc,
                     (WindowPtr)-1, true, 0);
    SetPort(gWin);
    for (i = 0; i < kNumButtons; i++) {
        SetRect(&r, (short)(12 + 104 * i), 14, (short)(12 + 104 * i + 96), 34);
        gButtons[i] = NewControl(gWin, &r, titles[i], true, 0, 0, 1, pushButProc, i);
        HiliteControl(gButtons[i], 255);
    }
    SetRect(&gResultBox, 12, 48, 420, 136);
    SetRect(&gDetailBox, 12, 142, 420, 156);
    SetRect(&gStatusBox, 12, 172, 420, 198);
    strcpy(gResult, "Push a button.");
    strcpy(gDetail, "Each button asks the Jitterbug extension for fresh bytes.");
}

static void SetupMenus(void)
{
    MenuHandle m = NewMenu(kAppleMenu, "\p\024");   /* the Apple symbol */
    AppendMenu(m, "\pAbout Jitterbug Demo\311;(-");
    AppendResMenu(m, 'DRVR');
    InsertMenu(m, 0);
    m = NewMenu(kFileMenu, "\pFile");
    AppendMenu(m, "\pQuit/Q");
    InsertMenu(m, 0);
    DrawMenuBar();
}

/* Returns false to quit. */
static Boolean DoMenu(long choice)
{
    short menu = (short)(choice >> 16), item = (short)(choice & 0xFFFF);
    Boolean keepGoing = true;

    if (menu == kAppleMenu) {
        if (item == kAboutItem) {
            strcpy(gResult, "Jitterbug Demo");
            strcpy(gDetail, "Random numbers from system-wide timing noise, for Mac OS 9.");
            SetPort(gWin);
            InvalRect(&gWin->portRect);
        } else {
            Str255 name;
            GetMenuItemText(GetMenuHandle(kAppleMenu), item, name);
            OpenDeskAcc(name);
        }
    } else if (menu == kFileMenu && item == kQuitItem) {
        keepGoing = false;
    }
    HiliteMenu(0);
    return keepGoing;
}

static void DoContentClick(EventRecord *ev)
{
    ControlHandle c;
    Point p = ev->where;
    SetPort(gWin);
    GlobalToLocal(&p);
    if (FindControl(p, gWin, &c) && TrackControl(c, p, NULL)) {
        StirEvent(ev);
        gActions[GetControlReference(c)]();
        InvalRect(&gResultBox);
        InvalRect(&gDetailBox);
        PollStatus();   /* a request can change the extension's state */
    }
}

int main(void)
{
    EventRecord ev;
    WindowPtr hit;
    Boolean running = true;
    unsigned long lastPoll = 0;

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();

    SetupMenus();
    SetupWindow();
    gReady = false;
    PollStatus();

    while (running) {
        if (WaitNextEvent(everyEvent, &ev, 15, NULL)) {
            switch (ev.what) {
            case mouseDown:
                switch (FindWindow(ev.where, &hit)) {
                case inMenuBar:   running = DoMenu(MenuSelect(ev.where)); break;
                case inSysWindow: SystemClick(&ev, hit); break;
                case inDrag:      DragWindow(hit, ev.where, &qd.screenBits.bounds); break;
                case inGoAway:
                    if (TrackGoAway(hit, ev.where)) running = false;
                    break;
                case inContent:
                    if (hit != FrontWindow()) SelectWindow(hit);
                    else DoContentClick(&ev);
                    break;
                }
                break;
            case keyDown:
                if (ev.modifiers & cmdKey)
                    running = DoMenu(MenuKey((char)(ev.message & charCodeMask)));
                break;
            case activateEvt:
                if ((WindowPtr)ev.message == gWin) {
                    gActive = (ev.modifiers & activeFlag) != 0;
                    HiliteButtons();
                }
                break;
            case updateEvt:
                if ((WindowPtr)ev.message == gWin) {
                    BeginUpdate(gWin);
                    DrawContent();
                    EndUpdate(gWin);
                }
                break;
            }
        }
        if (TickCount() - lastPoll >= 60) {
            lastPoll = TickCount();
            PollStatus();
        }
    }
    return 0;
}
