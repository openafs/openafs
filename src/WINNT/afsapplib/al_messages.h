/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AL_MESSAGES_H
#define AL_MESSAGES_H

// WM_COVER_WINDOW is used by CoverWindow() and CoverClient() to ensure that
// the proper thread creates a cover window when requested (if any thread
// other than g.hMain's thread creates the window, it will be destroyed
// automatically when the thread terminates).
//
// When used to cover a window:
//    LPCOVERPARAMS lpCoverParams = (LPCOVERPARAMS)lParam;
//
// When used to uncover a window:
//    HWND hwndToUncover = (HWND)wParam;
//
#define WM_COVER_WINDOW           (WM_USER + 0x200)

// WM_CREATE_ERROR_DIALOG is sent by ErrorDialog() to tell g.hMain to
// create a dialog; this way, the error dialog is always created by
// the main thread.
//
//    LPERROR_PARAMS = (LPERROR_PARAMS)lParam;
//
#define WM_CREATE_ERROR_DIALOG    (WM_USER + 0x201)

// WM_ENDTASK is posted to a window after a task request is created
// (via StartTask()).  A background routine handles one request at a time
// on an alternate thread, and posts its results to a specified window by
// means of an allocated structure.  That structure should be freed using
// FreeTaskPacket() when this message is received.
//
//    LPTASKPACKET ptp = (LPTASKPACKET)lParam;
//
#define WM_ENDTASK                (WM_USER + 0x202)

// WM_EXPIRED_CREDENTIALS is sent to the main window when a background
// thread detects that the user's credentials have expired.
//
//    BOOL fExpired = (BOOL)wParam;
//
#define WM_EXPIRED_CREDENTIALS    (WM_USER + 0x203)

// WM_CLOSE_DIALOG is sent to a modal dialog when the CloseDialog() routine
// is called.
//
//    HWND hWnd = (HWND)wParam;
//    int rc = (int)lParam;
//
#define WM_CLOSE_DIALOG           (WM_USER + 0x204)

// WM_PERMTAB_REFRESH is sent to a Permissions tab to cause its contents
// to be completely recalculated and redrawn. Any changes the user has
// made so far are discarded.
//
#define WM_PERMTAB_REFRESH        (WM_USER + 0x205)

// WM_REFRESHED_CREDENTIALS is sent to the main window whenever the
// AfsAppLib_SetCredentials to obtain new credentials.
//
//    LPARAM lp = (LPARAM)hCreds
//
#define WM_REFRESHED_CREDENTIALS  (WM_USER + 0x206)


#endif

