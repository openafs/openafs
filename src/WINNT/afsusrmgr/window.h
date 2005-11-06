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
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Main_DialogProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Main_PrepareTabChild (size_t iTabNew = -1);

void Main_SetMenus (void);
void Main_SetViewMenus (HMENU hm);


#endif

