/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

/****************************************************************************

    PROGRAM: Main.c

    PURPOSE: system test code for osi package.

****************************************************************************/


#include <afs/param.h>
#include <afs/stds.h>

#include "windows.h"
#include <string.h>
#include "main.h"
#include "basic.h"
#include "trylock.h"
#include "perf.h"
#include "osi.h"
#include <assert.h>

/* global state for test program */
HANDLE hInst;

HWND globalWnd;

/* screen image */
char main_screenText[HW_NLINES][80];

/* screen display image size */
RECT screenRect;

/* height of a text line */
int lineHeight;

/****************************************************************************

    FUNCTION: WinMain(HANDLE, HANDLE, LPSTR, int)

    PURPOSE: calls initialization function, processes message loop

****************************************************************************/

int APIENTRY WinMain(
    HANDLE hInstance,
    HANDLE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
    )
{
    MSG msg;


    if (!InitApplication(hInstance))
        return (FALSE);

    if (!InitInstance(hInstance, nCmdShow))
        return (FALSE);

    while (GetMessage(&msg, NULL, 0, 0)) {
	TranslateMessage(&msg);
	DispatchMessage(&msg);
    }
    return (msg.wParam);
}


/****************************************************************************

    FUNCTION: InitApplication(HANDLE)

    PURPOSE: Initializes window data and registers window class

****************************************************************************/

BOOL InitApplication(HANDLE hInstance)
{
    WNDCLASS  wc;

    /* create window class */
    wc.style = CS_DBLCLKS;          /* double-click messages */
    wc.lpfnWndProc = (WNDPROC) MainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH); 
    wc.lpszMenuName =  "InputMenu";
    wc.lpszClassName = "InputWClass";

    return (RegisterClass(&wc));
}


/****************************************************************************

    FUNCTION:  InitInstance(HANDLE, int)

    PURPOSE:  Saves instance handle and creates main window

****************************************************************************/

BOOL InitInstance(
    HANDLE          hInstance,
    INT             nCmdShow)
{
    HWND            hWnd;
    HDC             hDC;
    TEXTMETRIC      textmetric;
    RECT            rect;
    UUID	    debugID;
    long code;

    hInst = hInstance;

    /* create window itself */
    hWnd = CreateWindow(
        "InputWClass",
        "OSI Lock Test Application",
        WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL,  /* horz & vert scroll bars */
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hWnd)
        return (FALSE);

    globalWnd = hWnd;

    hDC = GetDC(hWnd);
    GetTextMetrics(hDC, &textmetric);
    lineHeight = textmetric.tmExternalLeading + textmetric.tmHeight;

    rect.left   = GetDeviceCaps(hDC, LOGPIXELSX) / 4;   /* 1/4 inch */
    rect.right  = GetDeviceCaps(hDC, HORZRES);
    rect.top    = GetDeviceCaps(hDC, LOGPIXELSY) / 4;   /* 1/4 inch */
    ReleaseDC(hWnd, hDC);
    rect.bottom = rect.top + HW_NLINES * lineHeight;
    screenRect = rect;

    /* init RPC system */
    osi_LongToUID(1, &debugID);
    code = osi_InitDebug(&debugID);

    if (code == 0) wsprintf(main_screenText[0], "Initialized successfully.");
    else wsprintf(main_screenText[0], "Failed to init debug system, code %ld", code);
    
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return (TRUE);

}

/****************************************************************************

    FUNCTION: MainWndProc(HWND, unsigned, WORD, LONG)

    PURPOSE:  Processes messages

****************************************************************************/

LONG APIENTRY MainWndProc(
	HWND hWnd,
	UINT message,
	UINT wParam,
	LONG lParam)
{
    FARPROC lpProcAbout;

    HDC hDC;                         /* display-context variable     */
    HMENU hMenu;		     /* menu */
    PAINTSTRUCT ps;                  /* paint structure              */
    RECT rect;
    long i;
    long code;

    switch (message) {
	case WM_COMMAND:
	    if (LOWORD(wParam) == IDM_ABOUT) {
		lpProcAbout = (FARPROC) About;

		DialogBox(hInst,
		    "AboutBox",
		    hWnd,
		    lpProcAbout);

		break;
	    }
	    if (LOWORD(wParam) == IDM_DEBUGON) {
		osi_LockTypeSetDefault("stat");
		hMenu = GetMenu(globalWnd);
		CheckMenuItem(hMenu, IDM_DEBUGON, MF_CHECKED);
		CheckMenuItem(hMenu, IDM_DEBUGOFF, MF_UNCHECKED);
	    }
	    if (LOWORD(wParam) == IDM_DEBUGOFF) {
		osi_LockTypeSetDefault((char *) 0);
		hMenu = GetMenu(globalWnd);
		CheckMenuItem(hMenu, IDM_DEBUGON, MF_UNCHECKED);
		CheckMenuItem(hMenu, IDM_DEBUGOFF, MF_CHECKED);
	    }
	    if (LOWORD(wParam) == IDM_BASICTEST) {
		main_ClearDisplay();
		wsprintf(main_screenText[0], "Starting basic test run...");
		code = main_BasicTest(hWnd);
		wsprintf(main_screenText[0], "Basic test returned code %d", code);
		InvalidateRect(hWnd, &screenRect, TRUE);
	    }
	    else if (LOWORD(wParam) == IDM_PERFTEST) {
		main_ClearDisplay();
		wsprintf(main_screenText[0], "Starting performance test run...");
		code = main_PerfTest(hWnd);
		wsprintf(main_screenText[0], "Performance test returned code %d", code);
		InvalidateRect(hWnd, &screenRect, TRUE);
	    }
	    else if (LOWORD(wParam) == IDM_TRYLOCKTEST) {
		main_ClearDisplay();
		wsprintf(main_screenText[0], "Starting TryLock test run...");
		code = main_TryLockTest(hWnd);
		wsprintf(main_screenText[0], "TryLock test returned code %d", code);
		InvalidateRect(hWnd, &screenRect, TRUE);
	    }
	    else
		return (DefWindowProc(hWnd, message, wParam, lParam));
	    break;

        case WM_CHAR:
            wsprintf(main_screenText[0], "WM_CHAR: %c, %x, %x",
                wParam, LOWORD(lParam), HIWORD(lParam));
            InvalidateRect(hWnd, &screenRect, TRUE);
            break;

        case WM_PAINT:
            hDC = BeginPaint (hWnd, &ps);

            if (IntersectRect(&rect, &screenRect, &ps.rcPaint)) {
		for(i=0; i<HW_NLINES; i++) {
		    TextOut(hDC, screenRect.left, screenRect.top + i*lineHeight,
			main_screenText[i], strlen(main_screenText[i]));
		}
	    }

            EndPaint(hWnd, &ps);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

	default:
	    return (DefWindowProc(hWnd, message, wParam, lParam));
    }
    return (0);
}


/****************************************************************************

    FUNCTION: About(HWND, unsigned, WORD, LONG)

    PURPOSE:  Processes messages for "About" dialog box

    MESSAGES:

	WM_INITDIALOG - initialize dialog box
	WM_COMMAND    - Input received

****************************************************************************/

BOOL APIENTRY About(
	HWND hDlg,
	UINT message,
	UINT wParam,
	LONG lParam)
{
    switch (message) {
	case WM_INITDIALOG:
	    return (TRUE);

	case WM_COMMAND:
	    if (LOWORD(wParam) == IDOK) {
		EndDialog(hDlg, TRUE);
		return (TRUE);
	    }
	    break;
    }
    return (FALSE);
	UNREFERENCED_PARAMETER(lParam);
}

void main_ClearDisplay(void)
{
	int i;
	for(i=0; i<HW_NLINES; i++) {
		/* make the line an empty line */
		main_screenText[i][0] = 0;
	}
}

void main_ForceDisplay(HANDLE hWnd)
{
	HDC hDC;
	HGDIOBJ hBrush;
	int i;

        hDC = GetDC(hWnd);
	hBrush = GetStockObject(WHITE_BRUSH);
	FillRect(hDC, &screenRect, hBrush);
	DeleteObject(hBrush);
	for(i=0; i<HW_NLINES; i++) {
	    TextOut(hDC, screenRect.left, screenRect.top + i*lineHeight,
		main_screenText[i], strlen(main_screenText[i]));
	}
	ReleaseDC(hWnd, hDC);
}

