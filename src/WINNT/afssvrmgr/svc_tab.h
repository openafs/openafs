/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVC_TAB_H
#define SVC_TAB_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Services_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Services_ShowParticularPopupMenu (HWND hParent, POINT ptScreen, LPIDENT lpiService);


#endif

