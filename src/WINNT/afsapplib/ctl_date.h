/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef CTL_DATE_H
#define CTL_DATE_H

/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifndef THIS_HINST
#define THIS_HINST  (HINSTANCE)GetModuleHandle(NULL)
#endif

#ifndef limit
#define limit(_a,_x,_b)  min( max( (_x), (_a) ), (_b) )
#endif

#ifndef inlimit
#define inlimit(_a,_x,_b)  ( (((_x)>=(_a)) && ((_x)<=(_b))) ? TRUE : FALSE )
#endif

#ifndef cxRECT
#define cxRECT(_r)  ((_r).right - (_r).left)
#endif

#ifndef cyRECT
#define cyRECT(_r)  ((_r).bottom - (_r).top)
#endif


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL RegisterDateClass (void);

#define DM_GETDATE    (WM_USER+313) // SYSTEMTIME *pTime = lp;
#define DM_SETDATE    (WM_USER+314) // SYSTEMTIME *pTime = lp;

#define DN_CHANGE     0x1005  // SYSTEMTIME *pTime = lp;
#define DN_UPDATE     0x1006  // SYSTEMTIME *pTime = lp;

/*
 * void DA_GetDate (HWND hDate, SYSTEMTIME *pDate)
 * void DA_SetDate (HWND hDate, SYSTEMTIME *pDate)
 *
 */
#define DA_GetDate(_hdate,_pDate) \
        SendMessage(_hdate,DM_GETDATE,(WPARAM)0,(LPARAM)_pDate)
#define DA_SetDate(_hdate,_pDate) \
        SendMessage(_hdate,DM_SETDATE,(WPARAM)0,(LPARAM)_pDate)


#endif

