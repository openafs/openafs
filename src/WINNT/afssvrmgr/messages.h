/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef MESSAGES_H
#define MESSAGES_H

// Timer IDs
//
#define ID_DISPATCH_TIMER 996
#define ID_ACTION_TIMER 997

// WM_NOTIFY_FROM_DISPATCH is posted when the AFSClass library notifies our
// handler that an event has taken place.
//
//    LPNOTIFYSTRUCT lpns = (LPNOTIFYSTRUCT)lParam;
//
// Use NotifyMe() to have notifications sent to your window when a
// particular object changes.
//
#define WM_NOTIFY_FROM_DISPATCH   (WM_USER + 0x100)

// WM_SERVER_CHANGED is sent to the child dialog of a server's window's tab
// control whenever that dialog's selected server changes.  It causes the
// window to be redisplayed.
//
//    LPIDENT lpiServerNew = (LPIDENT)lParam;  // NULL indicates no server
//
// Use Server_ForceRedraw() to post this message.
//
#define WM_SERVER_CHANGED         (WM_USER + 0x101)

// WM_REFRESH_UPDATE is sent to the "Refreshing..." dialog to update its
// displayed percent-complete.
//
//    DWORD dwPercentComplete = (DWORD)wParam;
//    LPIDENT lpiNowRefreshing = (LPIDENT)lParam;
//
#define WM_REFRESH_UPDATE         (WM_USER + 0x102)

// WM_OPEN_SERVERS is used by Display_Servers() to let g.hMain's thread know
// when it is finished updating the list of servers in a cell.  When received,
// all known servers are enumerated and secondary windows opened for them
// as appropriate.
//
#define WM_OPEN_SERVERS           (WM_USER + 0x103)

// WM_COLUMNS_CHANGED is sent by ShowColumnsDialog() if the user modified
// the list-of-columns given as the default selection.
//
#define WM_COLUMNS_CHANGED        (WM_USER + 0x104)

// WM_OPEN_SERVER is posted to g.hMain to cause it to call Server_Open().
//
//    LPIDENT lpiServerToOpen = (LPIDENT)lParam;
//
#define WM_OPEN_SERVER            (WM_USER + 0x105)

// WM_SHOW_CREATEREP_DIALOG is posted to g.hMain from any thread to cause
// the "Create Replica" dialog to be presented to the user.  In particular,
// this mechanism is used if the user right-drags an unreplicated fileset
// onto an aggregate and selects "replicate here": the user is first presented
// with the NOTREP dialog, and if the user is successful in getting the set
// replicated from there, the NOTREP_APPLY task posts this message to cause
// the "Create Replica" confirmation dialog to appear next.
//
//    LPIDENT lpiRW = (LPIDENT)wParam;
//    LPIDENT lpiTarget = (LPIDENT)lParam;  // may be NULL
//
#define WM_SHOW_CREATEREP_DIALOG  (WM_USER + 0x106)

// WM_SHOW_YOURSELF is posted to g.hMain to cause the main window to be
// displayed iff a cell has already been selected.
//
//    BOOL fForce = (BOOL)lp; // if !fForce, only show if g.lpiCell set
//
#define WM_SHOW_YOURSELF          (WM_USER + 0x107)

// WM_OPEN_ACTIONS is used by taskOPENCELL() to open the Operations In
// Progress window when appropriate.  The message is posted to g.hMain.
//
#define WM_OPEN_ACTIONS           (WM_USER + 0x108)

// WM_REFRESH_SETSECTION is sent to the "Refreshing..." dialog to tell
// the window what ID to pass to AfsClass_SkipRefresh() when the "Skip"
// button is clicked.
//
//    BOOL fStart = (BOOL)wParam;
//    int idSection = (int)lParam;
//
#define WM_REFRESH_SETSECTION     (WM_USER + 0x109)


#endif

