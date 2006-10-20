/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */


#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <rpc.h>
#include <string.h>
#include "osidebug.h"
#include "dbrpc.h"
#include <malloc.h>
#include <assert.h>
#include <stdlib.h>
#include "osiutils.h"

/* global instance for test program */
HANDLE hInst;

/* global window for debugging */
HANDLE hGlobalWnd;

/* binding handle we're connected to */
RPC_BINDING_HANDLE main_remoteHandle;

/* cache of formatting info */
main_formatCache_t *main_allFormatsp;

/* file name from dialog box */
char main_fileName[256];

/* screen display image size */
RECT screenRect;		/* whole screen */
HWND screenWnd;
RECT cb1Rect;			/* button for first command */
HWND cb1Wnd;
RECT cb2Rect;			/* button for 2nd command */
HWND cb2Wnd;
RECT cb3Rect;			/* guess */
HWND cb3Wnd;
RECT cb4Rect;			/* guess */
HWND cb4Wnd;
RECT nameRect;			/* the edit name rectangle */
HWND nameWnd;
RECT typesRect;			/* types rectangle */
HWND typesWnd;
RECT resultsRect;		/* results rectangle */
HWND resultsWnd;
RECT statusRect;		/* status line */
HWND statusWnd;

/* height of a text line */
int lineHeight;

/* function to divide up the screen space among the rectangles */
void main_Layout(HWND hWnd)
{
	long buttonWidth;
	long stripeHeight;

	/* make the screen have 4 buttons on the top, then then name under
	 * that, and the results below that.
	 * NB ALL RECTS store width and height instead of right and bottom
	 */
	GetClientRect(hWnd, &screenRect);
	buttonWidth = screenRect.right / 4;
	stripeHeight = screenRect.bottom / 10;

	cb1Rect.top = 0;
	cb1Rect.bottom = stripeHeight;
	cb1Rect.left = 0;
	cb1Rect.right = buttonWidth;

	cb2Rect.top = 0;
	cb2Rect.bottom = stripeHeight;
	cb2Rect.left = buttonWidth;
	cb2Rect.right = buttonWidth;

	cb3Rect.top = 0;
	cb3Rect.bottom = stripeHeight;
	cb3Rect.left = 2*buttonWidth;
	cb3Rect.right = buttonWidth;

	cb4Rect.top = 0;
	cb4Rect.bottom = stripeHeight;
	cb4Rect.left = 3*buttonWidth;
	cb4Rect.right = buttonWidth;

	nameRect.top = stripeHeight;
	nameRect.bottom = (3*lineHeight)/2;
	nameRect.left = 0;
	nameRect.right = (80*screenRect.right)/100;

	statusRect.top = screenRect.bottom - (3*lineHeight)/2;
	statusRect.bottom = (3*lineHeight)/2;
	statusRect.left = 0;
	statusRect.right = screenRect.right;

	/* what's left goes to the results and type frames */
	resultsRect.top = nameRect.bottom + stripeHeight;
	resultsRect.bottom = screenRect.bottom-resultsRect.top-statusRect.bottom;
	resultsRect.left = screenRect.right/3;
	resultsRect.right = (2*screenRect.right)/3;

	typesRect.top = nameRect.bottom + stripeHeight;
	typesRect.bottom = screenRect.bottom-typesRect.top-statusRect.bottom;
	typesRect.left = 0;
	typesRect.right = screenRect.right/3;
}

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

    hInst = hInstance;

    /* create window itself */
    hWnd = CreateWindow(
        "InputWClass",
        "OSI Remote Debugging Application",
        WS_OVERLAPPEDWINDOW,  /* horz & vert scroll bars */
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

    hDC = GetDC(hWnd);
    GetTextMetrics(hDC, &textmetric);
    lineHeight = textmetric.tmExternalLeading + textmetric.tmHeight;
    ReleaseDC(hWnd, hDC);

    main_allFormatsp = NULL;

    main_Layout(hWnd);

    /* now create children */
    cb1Wnd = CreateWindow("button", "Debug Server",
	WS_CHILD|WS_VISIBLE,
	cb1Rect.left, cb1Rect.top, cb1Rect.right, cb1Rect.bottom,
	hWnd, (void *) IDM_CMD1, hInst, 0);

    cb2Wnd = CreateWindow("button", "Quit",
	WS_CHILD|WS_VISIBLE,
	cb2Rect.left, cb2Rect.top, cb2Rect.right, cb2Rect.bottom,
	hWnd, (void *) IDM_CMD2, hInst, 0);

    cb3Wnd = CreateWindow("button", "Save to file",
	WS_CHILD|WS_VISIBLE,
	cb3Rect.left, cb3Rect.top, cb3Rect.right, cb3Rect.bottom,
	hWnd, (void *) IDM_CMD3, hInst, 0);

    cb4Wnd = CreateWindow("button", "[Future 1]",
	WS_CHILD|WS_VISIBLE,
	cb4Rect.left, cb4Rect.top, cb4Rect.right, cb4Rect.bottom,
	hWnd, (void *) IDM_CMD4, hInst, 0);

    nameWnd = CreateWindow("edit", "milkyway:1",
    	WS_CHILD|WS_VISIBLE|WS_BORDER|ES_LEFT,
    	nameRect.left, nameRect.top, nameRect.right, nameRect.bottom,
    	hWnd, (void *) IDM_NAME, hInst, 0);

    typesWnd = CreateWindow("listbox", "Collections",
    	WS_CHILD|WS_VISIBLE | WS_CAPTION | LBS_STANDARD,
    	typesRect.left, typesRect.top, typesRect.right, typesRect.bottom,
    	hWnd, (void *) IDM_TYPES, hInst, 0);

    resultsWnd = CreateWindow("listbox", "Results",
    	WS_CHILD|WS_VISIBLE | WS_CAPTION | LBS_NOTIFY|WS_VSCROLL|WS_BORDER,
    	resultsRect.left, resultsRect.top, resultsRect.right, resultsRect.bottom,
    	hWnd, (void *) IDM_RESULTS, hInst, 0);

    statusWnd = CreateWindow("edit", "",
    	WS_CHILD|WS_VISIBLE|WS_BORDER,
    	statusRect.left, statusRect.top, statusRect.right, statusRect.bottom,
    	hWnd, (void *) IDM_STATUS, hInst, 0);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return (TRUE);

}

main_formatCache_t *main_GetFormatCache(RPC_BINDING_HANDLE handle, char *typep,
	long region, long index)
{
	main_formatCache_t *fcp;
	long code;
	osi_remFormat_t format;

	for(fcp = main_allFormatsp; fcp; fcp = fcp->nextp) {
		if (strcmp(typep, fcp->typep) == 0 && fcp->region == region
			&& fcp->index == index) {
				/* found info, so return yes or not based
				 * upon whether there's a label.
				 */
				if (fcp->labelp) return fcp;
				else return NULL;
			}
	}

	/* if we get here, we don't have a definitive answer; try to get one */
	RpcTryExcept {
		dbrpc_GetFormat(handle, typep, region, index, &format, &code);
	}
	RpcExcept(1) {
		code = RpcExceptionCode();
	}
	RpcEndExcept;

	/* now, if code is 0 we win, and 1 means we know the entry doesn't exist.
	 * otherwise, we've failed and return such.
	 */
	if (code == 0 || code == 1) {
		fcp = (main_formatCache_t *) malloc(sizeof(*fcp));
		fcp->typep = (char *) malloc(strlen(typep)+1);
		strcpy(fcp->typep, typep);
		fcp->region = region;
		fcp->index = index;
		if (code == 0) {
			fcp->labelp = (char *) malloc(strlen(format.label)+1);
			strcpy(fcp->labelp, format.label);
		}
		else fcp->labelp = NULL;	/* no such entry */
		fcp->format = format.format;

		/* now thread it on */
		fcp->nextp = main_allFormatsp;
		main_allFormatsp = fcp;
		if (fcp->labelp) return fcp;
		else return NULL;
	}
	else return NULL;
}

#ifdef notdef
/* take a name and return a working binding to the server.  Server must be
 * exporting a dbrpc interface.
 */
main_GetBinding(char *name, RPC_BINDING_HANDLE *handlep)
{
	RPC_NS_HANDLE tempContext;	/* context used for iteration */
	RPC_BINDING_HANDLE thp;		/* temp handle we're using now */
	long code;
	int count=0;
	int valid;
	
	code = RpcNsBindingImportBegin(RPC_C_NS_SYNTAX_DCE, name, *dbrpc_v1_0_c_ifspecp,
		(UUID *) 0, &tempContext);
	if (code != RPC_S_OK) return code;

	/* this next line is useful when trying to debug things */
	// RpcNsMgmtHandleSetExpAge(tempContext, 0);

	valid = 0;	/* is thp valid? */
	while (1) {
		code = RpcNsBindingImportNext(tempContext, &thp);
		if (code == RPC_S_NO_MORE_BINDINGS) break;	/* we're done */

		if (code != RPC_S_OK) {
			RpcNsBindingImportDone(&tempContext);
			return code;
		}

		/* otherwise we have a putatively good binding; try it out
		 * with the probe interface and see if it really works
		 */
                code = RpcMgmtSetComTimeout(thp, 2);

		code = 0;
		RpcTryExcept {
			dbrpc_Ping(thp);
		}
		RpcExcept(1) {
			code = RpcExceptionCode();
		}
		RpcEndExcept;

		/* now, if call worked, we're done */
		if (code == 0) break;

		/* else cleanup the binding and try again */
		RpcBindingFree(&thp);
	}

	/* cleanup and return */
	RpcNsBindingImportDone(&tempContext);
	*handlep = thp;
	return code;
}
#endif /* notdef */

main_GetBinding(char *hostNamep, UUID *exportIDp, RPC_BINDING_HANDLE *handlep)
{
	char *stringBindingp;
        unsigned long code;
        RPC_BINDING_HANDLE handle;
        
        code = RpcStringBindingCompose(NULL, "ncacn_ip_tcp", hostNamep, NULL, NULL,
        	&stringBindingp);
	if (code) return code;
        
	code = RpcBindingFromStringBinding(stringBindingp, &handle);
        if (code) goto done;
        
	code = RpcMgmtSetComTimeout(handle, /* short, but not too short, timeout */ 3);
	if (code) {
		RpcBindingFree(&handle);
        	goto done;
	}
        
        code = RpcBindingSetObject(handle, exportIDp);
        if (code) {
		RpcBindingFree(&handle);
        	goto done;
	}
        
        *handlep = handle;
        code = 0;

done:
        RpcStringFree(&stringBindingp);
        return code;
}

main_RetrieveType(char *typep, RPC_BINDING_HANDLE remoteHandle, HWND hlistBox, int drawSeparator)
{
	long code;
	long closeCode;
	osi_remHyper_t fd;
	osi_remGetInfoParms_t status;
	char tstring[100];
	main_formatCache_t *fcp;
	char separator[100];
	int addedAny;
	int i;

	/* setup separator */
	wsprintf(separator, "====");

	/* now lookup available stats collections by opening "type" well-known
	 * info.
	 */
	RpcTryExcept {
		dbrpc_Open(remoteHandle, typep, &fd, &code);
	}
	RpcExcept(1) {
		code = RpcExceptionCode();
	}
	RpcEndExcept;

	if (code != 0) {
		return code;
	}

	/* now iterate over all info */
	addedAny = 0;
	while (1) {
		RpcTryExcept {
			dbrpc_GetInfo(remoteHandle, &fd, &status, &code);
		}
		RpcExcept(1) {
			code = RpcExceptionCode();
		}
		RpcEndExcept;

		if (code != 0) break;

		/* draw separator, if we should.  DrawSeparator tells us if
		 * we generally should do it, and addedAny, as a hack, tell us
		 * also if we're in the first pass or not, due to the fact
		 * that we don't set it until below.
		 */
		if (drawSeparator && addedAny)
			SendMessage(hlistBox, LB_ADDSTRING, 0, (long) separator);

		/* we know there's something there */
		addedAny = 1;

		/* put out all the strings */
		for(i=0; i<status.scount; i++) {
			fcp = main_GetFormatCache(remoteHandle, typep,
				OSI_DBRPC_REGIONSTRING, i);
			if (fcp)
				wsprintf(tstring, "%s: %s", fcp->labelp, status.sdata[i]);
			else
				wsprintf(tstring, "%s", status.sdata[i]);
			SendMessage(hlistBox, LB_ADDSTRING, 0, (long) tstring);
		}

		/* put out all the integers */
		for(i=0; i<status.icount; i++) {
			fcp = main_GetFormatCache(remoteHandle, typep,
				OSI_DBRPC_REGIONINT, i);
			if (fcp) {
				if (fcp->format & OSI_DBRPC_HEX)
					wsprintf(tstring, "%s: 0x%x", fcp->labelp, status.idata[i]);
				else if (fcp->format & OSI_DBRPC_SIGNED)
					wsprintf(tstring, "%s: %d", fcp->labelp, status.idata[i]);
				else
					wsprintf(tstring, "%s: %u", fcp->labelp, status.idata[i]);
			} else
				wsprintf(tstring, "%u", status.idata[i]);
			SendMessage(hlistBox, LB_ADDSTRING, 0, (long) tstring);
		}
	}

	if (!addedAny)
		SendMessage(hlistBox, LB_ADDSTRING, 0, (long) "[No entries]");

	/* finally close the FD */
	RpcTryExcept {
		dbrpc_Close(remoteHandle, &fd, &closeCode);
	}
	RpcExcept(1) {
		closeCode = RpcExceptionCode();
	}
	RpcEndExcept;

	/* now, we succeed if code is 0 or EOF */
	if (code == 0 || code == OSI_DBRPC_EOF) return 0;
	else return code;
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
    PAINTSTRUCT ps;                  /* paint structure              */
    char tbuffer[1000];
    long code;
    long index;
    long junk;
    HANDLE fileHandle;

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
	    if (LOWORD(wParam) == IDM_HELP) {
		WinHelp(hWnd, "osidebug.hlp", HELP_CONTENTS, 0L);
		return 0;
	    }
	    if (LOWORD(wParam) == IDM_CMD1) {
		char hostName[256];
                UUID exportID;
                char *tp;
                long longID;

		hGlobalWnd = hWnd;
		SendMessage(typesWnd, LB_RESETCONTENT, 0, 0);
		SendMessage(resultsWnd, LB_RESETCONTENT, 0, 0);
		GetWindowText(nameWnd, tbuffer, sizeof(tbuffer));
		if (main_remoteHandle != NULL) {
			RpcBindingFree(&main_remoteHandle);
			main_remoteHandle = NULL;
		}
                
                /* parse milkyway:3 into host milkyway, UUID 3 */
                strcpy(hostName, tbuffer);
		tp = strrchr(hostName, ':');
                if (tp) {
			/* parse UID */
                        *tp = 0;
                        longID = atoi(tp+1);
                        osi_LongToUID(longID, &exportID);
                }
                else
                	osi_LongToUID(0, &exportID);

		code = main_GetBinding(hostName, &exportID, &main_remoteHandle);
		if (code == 0) {
			code = main_RetrieveType("type", main_remoteHandle, typesWnd, 0);
			if (code) {
				wsprintf(tbuffer, "Can't obtain collection names, code %d",
					code);
				main_SetStatus(tbuffer);
			}
			else main_SetStatus("Done.");
		}
		else {
			wsprintf(tbuffer, "Bind failed, code %d", code);
			main_SetStatus(tbuffer);
		}
		InvalidateRect(hWnd, &screenRect, TRUE);
	    }
	    else if (LOWORD(wParam) == IDM_CMD2) {
		PostQuitMessage(0);
		break;
	    }
	    else if (LOWORD(wParam) == IDM_CMD3) {
		if (DialogBox(hInst,
		    "FileBox",
		    hWnd,
		    (FARPROC) FileProc) == FALSE) break;

                /* otherwise we have a file name, so go on */
		fileHandle = CreateFile(main_fileName,
                	GENERIC_READ | GENERIC_WRITE,
                        /* share mode */ 0,
                        /* security */ NULL,
                        CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL,
                        /* template */ NULL);
                if (fileHandle == INVALID_HANDLE_VALUE) break;
		for(index=0;;index++) {
			code = SendMessage(resultsWnd, LB_GETTEXT, index, (long) tbuffer);
			if (code == LB_ERR) break;
                        strcat(tbuffer, "\n");
                        WriteFile(fileHandle, tbuffer, strlen(tbuffer), &junk, NULL);
                }	/* for loop writing out data */
                CloseHandle(fileHandle);
		break;
	    }
	    else if (LOWORD(wParam) == IDM_CMD4) {
		MessageBeep(0);
		main_SetStatus("Future 1 not implemented");
	    }
	    else if (LOWORD(wParam) == IDM_TYPES) {
		if (HIWORD(wParam) == LBN_SELCHANGE) {
			/* if we get here, someone selected a collection to retrieve */
			SendMessage(resultsWnd, LB_RESETCONTENT, 0, 0);
			index = SendMessage(typesWnd, LB_GETCURSEL, 0, 0);
			if (index != LB_ERR || main_remoteHandle == NULL) {
				SendMessage(typesWnd, LB_GETTEXT, index, (long) tbuffer);
				code=main_RetrieveType(tbuffer, main_remoteHandle, resultsWnd, 1);
				if (code != 0) {
					wsprintf(tbuffer, "Can't fetch collection, code %d", code);
					main_SetStatus(tbuffer);
				}
				else main_SetStatus("Done.");
			}
			else main_SetStatus("Internal inconsistency finding selection!");
		}
	    }
	    else
		return (DefWindowProc(hWnd, message, wParam, lParam));
	    break;

        case WM_PAINT:
	    /* make kids draw, too */
	    InvalidateRect(hWnd, &screenRect, TRUE);
	    
            hDC = BeginPaint (hWnd, &ps);
	    /* nothing to draw, right? */
            EndPaint(hWnd, &ps);
            break;

	case WM_SIZE:
	    main_Layout(hWnd);
	    MoveWindow(cb1Wnd, cb1Rect.left, cb1Rect.top, cb1Rect.right, cb1Rect.bottom, 1);
	    MoveWindow(cb2Wnd, cb2Rect.left, cb2Rect.top, cb2Rect.right, cb2Rect.bottom, 1);
	    MoveWindow(cb3Wnd, cb3Rect.left, cb3Rect.top, cb3Rect.right, cb3Rect.bottom, 1);
	    MoveWindow(cb4Wnd, cb4Rect.left, cb4Rect.top, cb4Rect.right, cb4Rect.bottom, 1);
	    MoveWindow(nameWnd, nameRect.left, nameRect.top, nameRect.right, nameRect.bottom, 1);
	    MoveWindow(statusWnd, statusRect.left, statusRect.top, statusRect.right,
		statusRect.bottom, 1);
	    MoveWindow(typesWnd, typesRect.left, typesRect.top, typesRect.right,
		typesRect.bottom, 1);
	    MoveWindow(resultsWnd, resultsRect.left, resultsRect.top, resultsRect.right,
		resultsRect.bottom, 1);
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

BOOL APIENTRY FileProc(
	HWND hDlg,
	UINT message,
	UINT wParam,
	LONG lParam)
{
    long code;
    char tname[256];

    switch (message) {
	case WM_INITDIALOG:
	    DlgDirList(hDlg, "*.*", IDM_FILEBOX, IDM_FILENAME, DDL_DIRECTORY|DDL_DRIVES);
	    return (TRUE);

	case WM_COMMAND:
	    if (LOWORD(wParam) == IDOK) {
		GetDlgItemText(hDlg, IDM_FILENAME, main_fileName, sizeof(main_fileName));
		EndDialog(hDlg, TRUE);
		return (TRUE);
	    }
            else if (LOWORD(wParam) == IDCANCEL) {
            	EndDialog(hDlg, FALSE);
                return TRUE;
            }
            else if (LOWORD(wParam) == IDM_FILEBOX) {
		switch (HIWORD(wParam)) {
			case LBN_DBLCLK:
				code = DlgDirSelectEx(hDlg, tname, sizeof(tname), IDM_FILEBOX);
                                if (code == 0) {
					/* have a selection */
					strcpy(main_fileName, tname);
                                        EndDialog(hDlg, TRUE);
                                }
                                else {
                                	/* have a dir */
					DlgDirList(hDlg, tname, IDM_FILEBOX, IDM_FILENAME, DDL_DIRECTORY|DDL_DRIVES);
                                }
				break;
                };	/* switch */
                return TRUE;
            }
	    break;
    }
    return (FALSE);
	UNREFERENCED_PARAMETER(lParam);
}

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

void main_SetStatus(char *status)
{
	SetWindowText(statusWnd, status);
	InvalidateRect(statusWnd, (RECT *) 0, 1);
	UpdateWindow(statusWnd);
}
