/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#define IDM_ABOUT	100	/* about box */
#define IDM_BASICTEST	101	/* run basic tests */
#define IDM_PERFTEST	102	/* run perf tests */
#define IDM_TRYLOCKTEST	103	/* run trylock tests */
#define IDM_DEBUGON	104	/* turn on debug lock tracing */
#define IDM_DEBUGOFF	105	/* turn off debug lock tracing */

BOOL InitApplication(HANDLE);
BOOL InitInstance(HANDLE, INT);
LONG APIENTRY MainWndProc(HWND, UINT, UINT, LONG);
BOOL APIENTRY About(HWND, UINT, UINT, LONG);

extern void main_ForceDisplay(HANDLE hWnd);

extern void main_ClearDisplay(void);

/* max of 10 lines on the screen */
#define HW_NLINES 10

/* screen image to write to from tests
 * This shouldn't be global, but it doesn't matter that much.
 */
extern char main_screenText[HW_NLINES][80];

#ifdef WIN32
#define GET_WM_HSCROLL_CODE(wp, lp)    LOWORD(wp)
#define GET_WM_HSCROLL_POS(wp, lp)     HIWORD(wp)
#define GET_WM_HSCROLL_HWND(wp, lp)    (HWND)(lp)
#else
#define GET_WM_HSCROLL_CODE(wp, lp)    (wp)
#define GET_WM_HSCROLL_POS(wp, lp)     LOWORD(lp)
#define GET_WM_HSCROLL_HWND(wp, lp)    (HWND)HIWORD(lp)
#endif
