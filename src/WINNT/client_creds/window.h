/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef WINDOW_H
#define WINDOW_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define WM_TRAYICON          (WM_USER+100)


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Main_DlgProc (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

void Main_RepopulateTabs (BOOL fDestroyInvalid);
void Main_EnableRemindTimer (BOOL fEnable);
void Main_Show (BOOL fShow);
size_t Main_FindExpiredCreds (void);


#endif

