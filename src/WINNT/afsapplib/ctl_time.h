/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef CTL_TIME_H
#define CTL_TIME_H

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


EXPORTED BOOL RegisterTimeClass (void);

#define TM_GETTIME    (WM_USER+311) // SYSTEMTIME *pTime = lp;
#define TM_SETTIME    (WM_USER+312) // SYSTEMTIME *pTime = lp;

#define TN_CHANGE     0x1005  // SYSTEMTIME *pTime = lp;
#define TN_UPDATE     0x1006  // SYSTEMTIME *pTime = lp;

/*
 * void TI_GetTime (HWND hTime, SYSTEMTIME *pTime)
 * void TI_SetTime (HWND hTime, SYSTEMTIME *pTime)
 *
 */
#define TI_GetTime(_htime,_pTime) \
        SendMessage(_htime,TM_GETTIME,(WPARAM)0,(LPARAM)_pTime)
#define TI_SetTime(_htime,_pTime) \
        SendMessage(_htime,TM_SETTIME,(WPARAM)0,(LPARAM)_pTime)


#endif

