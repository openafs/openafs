/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * I've FINALLY stuck this in a separate file; I must have twenty different
 * implementations floating around these days.  Anyway, here's my generic
 * let-your-dialog-resize-and-still-look-cool code; an example of how to use
 * it:
 *
 * if dialog looks like:
 *
 * +-----------------------------------------------------+
 * |                                                     |
 * | [ TXT_HEADER (can resize horizontally, but should ] |
 * | [ TXT_HEADER  retain same vertical size.  The     ] |
 * | [_TXT_HEADER__upper-left corner should not move)__] |
 * |                                                     |
 * | [ ID_LIST_BOX (should resize horizontally and     ] |
 * | [ ID_LIST_BOX  vertically to match changes in     ] |
 * | [_ID_LIST_BOX__parent window; UL corner no move)__] |
 * |                                                     |
 * | (upper-left moves; don't resize ->)   [OK] [CANCEL] |
 * |                                                     |
 * +-----------------------------------------------------+
 *
 * then define something like this as a global variable:
 *
 *    rwWindowData awdDialog[] = {
 *       { TXT_HEADER,    raSizeX           },
 *       { ID_LIST_BOX,   raSizeX | raSizeY },
 *       { IDOK,          raMoveX | raMoveY },
 *       { IDCANCEL,      raMoveX | raMoveY },
 *       { idDEFAULT,     raLeaveAlone },
 *       { idENDLIST,     0 }
 *    };
 *
 * in WM_INITDIALOG:
 *
 *    RECT rFromLastSession = GuessWhereWindowShouldGo();
 *    ResizeWindow (hDlg, awdDialog, rwaMoveToHere, &rFromLastSession);
 *
 * in WM_SIZE:
 *
 *    // if (lp==0), we're minimizing--don't call ResizeWindow().
 *    //
 *    if (lp != 0)
 *       ResizeWindow (hDlg, awdDialog, rwaFixupGuts);
 *
 * if you want to, say, add a status bar (which mucks with the client size
 * of the window):
 *
 *    AddStatusBar (hDlg, ...);
 *    ResizeWindow (hDlg, awdDialog, rwaJustResync);
 *
 * if you want to, say, add a tool bar (which doesn't much with the client
 * location but should):
 *
 *    AddToolbar (hDlg, ID_TOOLBAR, ...);
 *    GetWindowRect (GetDlgItem (hDlg, ID_TOOLBAR), &rToolBar);
 *    ResizeWindow (hDlg, awdDialog, rwaNewClientArea, &rToolBar);
 *
 *
 */

#ifndef RESIZE_H
#define RESIZE_H

#include <windows.h>

#ifndef EXPORTED
#define EXPORTED
#endif


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

         // rwAction types:
         //
typedef enum
   {
   rwaMoveToHere,	// resize window to {pr}, then fix guts
   rwaFixupGuts,	// reposition guts to match window size
   rwaJustResync,	// recognize new size but don't fix guts
   rwaNewClientArea	// pretend the client area changed
   } rwAction;

         // ra flags:
         //
#define raLeaveAlone   0x0000	// don't modify window during resize

#define raSizeX        0x0001	// increase width if resizing X+
#define raSizeY        0x0002	// increase height if resizing Y+
#define raMoveX        0x0004	// move right if resizing X+
#define raMoveY        0x0008	// move down if resizing Y+

#define raSizeXB       0x0010	// decrease width if resizing X+
#define raSizeYB       0x0020	// decrease height if resizing Y+
#define raMoveXB       0x0040	// move left if resizing X+
#define raMoveYB       0x0080	// move up if resizing Y+

#define raSizeXC       0x0100	// increase width by X/2 if resizing X+
#define raSizeYC       0x0200	// increase height by Y/2 if resizing Y+
#define raMoveXC       0x0400	// move right by X/2 if resizing X+
#define raMoveYC       0x0800	// move down by Y/2 if resizing Y+

#define raRepaint      0x1000	// force repaint whenever resized
#define raNotify       0x2000	// notify window with WM_SIZE afterwards

         // rwWindowData types:
         //
typedef struct
   {
   int     id;	// child ID (-1=end list, 0=default)
   DWORD   ra;	// what to do with this child window
   DWORD   cMinimum;	// minimum size (0=no limit, LO=x,HI=y)
   DWORD   cMaximum;	// maximum size (0=no limit, LO=x,HI=y)
   } rwWindowData;

#define idDEFAULT  (-2)
#define idENDLIST  (-3)


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

EXPORTED HWND CreateSplitter (HWND, int, int, int, LONG *, rwWindowData *, BOOL);
EXPORTED void DeleteSplitter (HWND, int);

EXPORTED void ResizeWindow (HWND, rwWindowData *, rwAction, RECT * = NULL);

EXPORTED void GetRectInParent (HWND hWnd, RECT *pr);

#endif

