/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef CTL_ELAPSED_H
#define CTL_ELAPSED_H

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


#define csec1SECOND    (1L)
#define csec1MINUTE    (60L * csec1SECOND)
#define csec1HOUR      (60L * csec1MINUTE)
#define csec1DAY       (24L * csec1HOUR)
#define csec1WEEK      ( 7L * csec1DAY)

#define SET_ELAPSED_TIME(_pst,_h,_m,_s) \
                  memset (_pst, 0x00, sizeof(SYSTEMTIME)); \
                  (_pst)->wDay = (WORD)( (_h) / 24L ); \
                  (_pst)->wHour = (WORD)( (_h) % 24L ); \
                  (_pst)->wMinute = (WORD)(_m); \
                  (_pst)->wSecond = (WORD)(_s);

#define SET_ELAPSED_TIME_FROM_SECONDS(_pst,_s) \
                  memset (_pst, 0x00, sizeof(SYSTEMTIME)); \
                  (_pst)->wSecond = (WORD)((_s) % 60L);  \
                  (_s) /= 60L; \
                  (_pst)->wMinute = (WORD)((_s) % 60L); \
                  (_s) /= 60L; \
                  (_pst)->wHour = (WORD)((_s) % 24L); \
                  (_s) /= 24L; \
                  (_pst)->wDay = (WORD)(_s);

#define GET_SECONDS_FROM_ELAPSED_TIME(_pst) \
                  ( ((ULONG)((_pst)->wSecond)) + \
                    ((ULONG)((_pst)->wMinute) * 60L) + \
                    ((ULONG)((_pst)->wHour) * 60L * 60L) + \
                    ((ULONG)((_pst)->wDay) * 60L * 60L * 24L) )


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

EXPORTED BOOL RegisterElapsedClass (void);

#define ELM_GETRANGE   (WM_USER+301) // SYSTEMTIME *pTime = wp, lp;
#define ELM_SETRANGE   (WM_USER+302) // SYSTEMTIME *pTime = wp, lp;
#define ELM_GETTIME    (WM_USER+307) // SYSTEMTIME *pTime = lp;
#define ELM_SETTIME    (WM_USER+308) // SYSTEMTIME *pTime = lp;

#define ELN_CHANGE     0x1003  // SYSTEMTIME *pTime = lp;
#define ELN_UPDATE     0x1004  // SYSTEMTIME *pTime = lp;

/*
 * void EL_GetRange (HWND hElapsed, SYSTEMTIME *pTimeMin, SYSTEMTIME *pTimeMax)
 * void EL_SetRange (HWND hElapsed, SYSTEMTIME *pTimeMin, SYSTEMTIME *pTimeMax)
 *
 */
#define EL_GetRange(_hel,_pTimeMin,_pTimeMax) \
        SendMessage(_hel,ELM_GETRANGE,(WPARAM)_pTimeMin,(LPARAM)_pTimeMax)
#define EL_SetRange(_hel,_pTimeMin,_pTimeMax) \
        SendMessage(_hel,ELM_SETRANGE,(WPARAM)_pTimeMin,(LPARAM)_pTimeMax)

/*
 * void EL_GetTime (HWND hElapsed, SYSTEMTIME *pTime)
 * void EL_SetTime (HWND hElapsed, SYSTEMTIME *pTime)
 *
 */
#define EL_GetTime(_hel,_pTime) \
        SendMessage(_hel,ELM_GETTIME,(WPARAM)0,(LPARAM)_pTime)
#define EL_SetTime(_hel,_pTime) \
        SendMessage(_hel,ELM_SETTIME,(WPARAM)0,(LPARAM)_pTime)


#endif

