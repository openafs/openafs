/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef CHECKLIST_H
#define CHECKLIST_H

/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifndef THIS_HINST
#define THIS_HINST  (HINSTANCE)GetModuleHandle(NULL)
#endif

#ifndef EXPORTED
#define EXPORTED
#endif


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

EXPORTED BOOL RegisterCheckListClass (void);

#define WC_CHECKLIST  TEXT("CheckList")


#define LB_GETCHECK   (WM_USER+300)   // int iItem=wp  
#define LB_SETCHECK   (WM_USER+301)   // int iItem=wp, BOOL fChecked=lp

#define LBN_CLICKED   BN_CLICKED


/*
 * BOOL LB_GetCheck (HWND hList, int iItem)
 * void LB_SetCheck (HWND hList, int iItem, BOOL fCheck)
 *
 */
#define LB_GetCheck(_hList,_ii) \
        SendMessage(_hList,LB_GETCHECK,(WPARAM)_ii,(LPARAM)0)
#define LB_SetCheck(_hList,_ii,_fCheck) \
        SendMessage(_hList,LB_SETCHECK,(WPARAM)_ii,(LPARAM)_fCheck)


#endif

