/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef CTL_SPINNER_H
#define CTL_SPINNER_H

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

EXPORTED BOOL RegisterSpinnerClass (void);

// NOTE: All messages are sent to the buddy control, and all notifications
// come back through WM_COMMAND:LOWORD(wp)==buddy,HIWORD(wp)=SPN_code,lp=param
//
#define SPM_GETRANGE            (WM_USER+301)
#define SPM_SETRANGE            (WM_USER+302)
#define SPM_GETPOS              (WM_USER+303)
#define SPM_SETPOS              (WM_USER+304)
#define SPM_GETBASE             (WM_USER+305)
#define SPM_SETBASE             (WM_USER+306)
#define SPM_REATTACH            (WM_USER+307)
#define SPM_SETRECT             (WM_USER+308)
#define SPM_GETSPINNER          (WM_USER+309)
#define SPM_SETFORMAT           (WM_USER+310)
#define SPM_SETBUDDY            (WM_USER+311)

#define SPN_CHANGE_UP    0x1001  // DWORD *pdwValue = lp;
#define SPN_CHANGE_DOWN  0x1002  // DWORD *pdwValue = lp;
#define SPN_CHANGE       0x1003  // DWORD *pdwValue = lp;
#define SPN_UPDATE       0x1004  // DWORD dwNew = lp;

#define SPVAL_UNCHANGED  (DWORD)0x7FFFFFFF

EXPORTED BOOL CreateSpinner (HWND hBuddy,
                             WORD wBase = 10, BOOL fSigned = FALSE,
                             DWORD dwMin = 0, DWORD dwPos = 100, DWORD dwMax = 100,
                             LPRECT prTarget = NULL);

EXPORTED BOOL fHasSpinner (HWND hBuddy);

/*
 * void SP_GetRange (HWND hBuddy, DWORD *pdwMin, DWORD *pdwMax)
 * void SP_SetRange (HWND hBuddy, DWORD dwMin, DWORD dwMax)
 *
 */
#define SP_GetRange(_hb,_pdwMin,_pdwMax) \
        SendMessage(_hb,SPM_GETRANGE,(WPARAM)_pdwMin,(LPARAM)_pdwMax)
#define SP_SetRange(_hb,_dwMin,_dwMax) \
        SendMessage(_hb,SPM_SETRANGE,(WPARAM)_dwMin,(LPARAM)_dwMax)

/*
 * DWORD SP_GetPos (HWND hBuddy)
 * void  SP_SetPos (HWND hBuddy, DWORD dwPos)
 *
 */
#define SP_GetPos(_hb) \
        SendMessage(_hb,SPM_GETPOS,(WPARAM)0,(LPARAM)0)
#define SP_SetPos(_hb,_dwPos) \
        SendMessage(_hb,SPM_SETPOS,(WPARAM)0,(LPARAM)_dwPos)

/*
 * void SP_GetBase (HWND hBuddy, WORD *pwBase, BOOL *pfSigned)
 * void SP_SetBase (HWND hBuddy, WORD wBase, BOOL fSigned)
 *
 */
#define SP_GetBase(_hb,_pdwBase,_pfSigned) \
        SendMessage(_hb,SPM_GETBASE,(WPARAM)_pdwBase,(LPARAM)_pfSigned)
#define SP_SetBase(_hb,_wBase,_fSigned) \
        SendMessage(_hb,SPM_SETBASE,(WPARAM)_wBase,(LPARAM)_fSigned)

/*
 * HWND SP_GetSpinner (HWND hBuddy)
 *
 */
#define SP_GetSpinner(_hb) \
        (HWND)SendMessage(_hb,SPM_GETSPINNER,(WPARAM)0,(LPARAM)0)

/*
 * void SP_SetRect (HWND hBuddy, LPRECT prTarget)
 *
 */
#define SP_SetRect(_hb,_prTarget) \
        SendMessage(_hb,SPM_SETRECT,(WPARAM)0,(LPARAM)_prTarget)

/*
 * void SP_SetFormat (HWND hBuddy, LPTSTR pszFormat)
 * (where pszFormat uses "%lu" or "%ld" as appropriate)
 *
 */
#define SP_SetFormat(_hb,_pszFormat) \
        SendMessage(_hb,SPM_SETFORMAT,(WPARAM)0,(LPARAM)_pszFormat)

/*
 * void SP_SetBuddy (HWND hOld, HWND hBuddyNew, BOOL fMove)
 *
 */
#define SP_SetBuddy(_hb,_hNew,_fMove) \
        SendMessage(_hb,SPM_SETBUDDY,(WPARAM)_hNew,(LPARAM)_fMove)


#endif

