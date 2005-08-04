/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */
/* AFSIFS Portions copyright (c) 2005
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any purpose,
 * so long as the name of the university of michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.  if
 * the above copyright notice or any other identification of the
 * university of michigan is included in any copy of any portion of
 * this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the
 * university of michigan as to its fitness for any purpose, and without
 * warranty by the university of michigan of any kind, either express 
 * or implied, including without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  the regents
 * of the university of michigan shall not be liable for any damages,   
 * including special, indirect, incidental, or consequential damages, 
 * with respect to any claim arising out or in connection with the use
 * of the software, even if it has been or is hereafter advised of the
 * possibility of such damages.
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <string.h>
#include <nb30.h>

#include <osi.h>
#include "afsd.h"
#include "afsd_init.h"
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>

#ifdef _DEBUG
#include <crtdbg.h>
#endif

#include "afsdifs.h"

HANDLE main_inst;
HWND main_wnd;
char main_statusText[100];
RECT main_rect;
osi_log_t *afsd_logp;

HANDLE hAFSDWorkerThread[WORKER_THREADS], DoTerminate;

extern int traceOnPanic;

extern void afsd_DbgBreakAllocInit();
extern void afsd_DbgBreakAdd(DWORD requestNumber);

HANDLE WaitToTerminate = NULL;

/*
 * Notifier function for use by osi_panic
 */
void afsd_notifier(char *msgp, char *filep, long line)
{
	char tbuffer[100];
	if (filep)
		sprintf(tbuffer, "Error at file %s, line %d", filep, line);
	else
		strcpy(tbuffer, "Error at unknown location");

	if (!msgp)
		msgp = "Assertion failure";

	MessageBox(NULL, tbuffer, msgp, MB_OK|MB_ICONSTOP|MB_SETFOREGROUND);

	afsd_ForceTrace(TRUE);
        buf_ForceTrace(TRUE);

	if (traceOnPanic) {
		_asm int 3h;
	}

	exit(1);
}

/* Init function called when window application starts.  Inits instance and
 * application together, since in Win32 they're essentially the same.
 *
 * Function then goes into a loop handling user interface messages.  Most are
 * used to handle redrawing the icon.
 */
int WINAPI WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	char *lpCmdLine,
	int nCmdShow)
{
	MSG msg;
	int i;
	
    afsd_SetUnhandledExceptionFilter();
       
#ifdef _DEBUG
    afsd_DbgBreakAllocInit();
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF /* | _CRTDBG_CHECK_ALWAYS_DF */ | _CRTDBG_CHECK_CRT_DF | _CRTDBG_DELAY_FREE_MEM_DF );
    if (lpCmdLine)
    {
        char *allocRequest = strtok(lpCmdLine, " \t");
        while (allocRequest)
        {
            afsd_DbgBreakAdd(atoi(allocRequest));
            allocRequest = strtok(NULL, " \t");
        }
    }
#endif 

    if (!InitClass(hInstance))
        return (FALSE);

    if (!InitInstance(hInstance, nCmdShow))
        return (FALSE);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

#ifdef AFSIFS
    WaitForMultipleObjects(WORKER_THREADS, hAFSDWorkerThread, TRUE, INFINITE);
    for (i = 0; i < WORKER_THREADS; i++)
        CloseHandle(hAFSDWorkerThread[i]);
    //CloseHandle(hAFSDMainThread);
    RpcMgmtStopServerListening(NULL);
#endif

    return (msg.wParam);
}


/* create the window type for our main window */
BOOL InitClass(HANDLE hInstance)
{
	WNDCLASS  wc;

	wc.style = CS_DBLCLKS;          /* double-click messages */
	wc.lpfnWndProc = (WNDPROC) MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(hInstance, "AFSDIcon");
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = GetStockObject(WHITE_BRUSH); 
	wc.lpszMenuName =  "AFSDMenu";
	wc.lpszClassName = "AFSDWinClass";

	return (RegisterClass(&wc));
}

/* initialize the process.  Reads the init files to get the appropriate
 * information. */
BOOL InitInstance(
	HANDLE hInstance,
	int nCmdShow)
{
	HWND hWnd;
	HDC hDC;
	TEXTMETRIC textmetric;
	INT nLineHeight;
    long code, cnt;
	char *reason;
 
	/* remember this, since it is a useful thing for some of the Windows
	 * calls */
	main_inst = hInstance;

	/* create our window */
	hWnd = CreateWindow(
		"AFSDWinClass",
	        "AFSD",
	        WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL,
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

	/* lookup text dimensions */
	hDC = GetDC(hWnd);
	GetTextMetrics(hDC, &textmetric);
	nLineHeight = textmetric.tmExternalLeading + textmetric.tmHeight;
	
	main_rect.left   = GetDeviceCaps(hDC, LOGPIXELSX) / 4;   /* 1/4 inch */
	main_rect.right  = GetDeviceCaps(hDC, HORZRES);
	main_rect.top    = GetDeviceCaps(hDC, LOGPIXELSY) / 4;   /* 1/4 inch */
	ReleaseDC(hWnd, hDC);
	main_rect.bottom = main_rect.top + nLineHeight;

	osi_InitPanic(afsd_notifier);

	afsi_start();

        code = afsd_InitCM(&reason);
	if (code != 0)
		osi_panic(reason, __FILE__, __LINE__);

	code = afsd_InitDaemons(&reason);
	if (code != 0)
		osi_panic(reason, __FILE__, __LINE__);

#ifndef AFSIFS
        code = afsd_InitSMB(&reason, MessageBox);
#else
	code = ifs_Init(&reason);
#endif

	if (code != 0)
            osi_panic(reason, __FILE__, __LINE__);

#ifdef AFSIFS
	DoTerminate = CreateEvent(NULL, TRUE, FALSE, TEXT("afsd_service_DoTerminate"));
        if ( GetLastError() == ERROR_ALREADY_EXISTS )
            afsi_log("Event Object Already Exists: %s", TEXT("afsd_service_DoTerminate"));
	for (cnt = 0; cnt < WORKER_THREADS; cnt++)
            hAFSDWorkerThread[cnt] = CreateThread(NULL, 0, ifs_MainLoop, 0, 0, NULL);
#endif

	ShowWindow(hWnd, SW_SHOWMINNOACTIVE);
	UpdateWindow(hWnd);
	return (TRUE);
}

/* called with no locks with translated messages */
LONG APIENTRY MainWndProc(
	HWND hWnd,
	unsigned int message,
	unsigned int wParam,
	long lParam)
{
	HDC hDC;                         /* display-context variable     */
	PAINTSTRUCT ps;                  /* paint structure              */

	main_wnd = hWnd;

	switch (message) {
	    case WM_QUERYOPEN:
		/* block attempts to open the window */
		return 0;

	    case WM_COMMAND:
		/* LOWORD(wParam) is command */
		return (DefWindowProc(hWnd, message, wParam, lParam));

	    case WM_CREATE:
		break;

	    case WM_PAINT:
		hDC = BeginPaint (hWnd, &ps);
		/* nothing to print, but this clears invalidated rectangle flag */
		EndPaint(hWnd, &ps);
		break;

	    case WM_DESTROY:
#ifdef AFSIFS
		SetEvent(DoTerminate);
#endif
		RpcMgmtStopServerListening(NULL);
		PostQuitMessage(0);
		break;

	    default:
		return (DefWindowProc(hWnd, message, wParam, lParam));
	}
	return (0);
}
