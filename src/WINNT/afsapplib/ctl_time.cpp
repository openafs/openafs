/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdlib.h>
#include <WINNT/dialog.h>
#include <WINNT/resize.h>	// for GetRectInParent()
#include <WINNT/subclass.h>
#include <WINNT/ctl_spinner.h>
#include <WINNT/ctl_time.h>
#include <WINNT/TaLocale.h>

#ifndef cchRESOURCE
#define cchRESOURCE 256
#endif


/*
 * MISCELLANEOUS ______________________________________________________________
 *
 */

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) TimeReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
BOOL TimeReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
{
   LPVOID pNew;
   size_t cNew;

   if (cReq <= *pcTarget)
      return TRUE;

   if ((cNew = cInc * ((cReq + cInc-1) / cInc)) <= 0)
      return FALSE;

   if ((pNew = (LPVOID)Allocate (cbElement * cNew)) == NULL)
      return FALSE;
   memset (pNew, 0x00, cbElement * cNew);

   if (*pcTarget != 0)
      {
      memcpy (pNew, *ppTarget, cbElement * (*pcTarget));
      Free (*ppTarget);
      }

   *ppTarget = pNew;
   *pcTarget = cNew;
   return TRUE;
}
#endif



/*
 * TIME _______________________________________________________________________
 *
 */

typedef struct
   {
   HWND hTime;
   HWND hHours;
   HWND hSep1;
   HWND hMinutes;
   HWND hSep2;
   HWND hAMPM;
   HWND hSpinner;
   HWND hSpinnerBuddy;

   WORD idHours;
   WORD idMinutes;
   WORD idAMPM;

   SYSTEMTIME timeNow;  // only hour and minute fields are relevant

   BOOL  f24Hour;
   BOOL  f0Hours;
   BOOL  fCallingBack;
   BOOL  fCanCallBack;
   } TimeInfo;

static CRITICAL_SECTION csTime;
static TimeInfo        *aTime = NULL;
static size_t           cTime = 0;

#define cszTIMECLASS TEXT("Time")

BOOL CALLBACK TimeProc (HWND hTime, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK TimeEditProc (HWND hEdit, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK TimeDlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Time_SendCallback (TimeInfo *pti, WORD eln, LPARAM lp);

void Time_OnCreate (TimeInfo *pti);
void Time_OnDestroy (TimeInfo *pti);
void Time_OnButtonDown (TimeInfo *pti, UINT msg, WPARAM wp, LPARAM lp);
BOOL Time_OnGetTime (TimeInfo *pti, WPARAM wp, LPARAM lp);
BOOL Time_OnSetTime (TimeInfo *pti, WPARAM wp, LPARAM lp);

void Time_Edit_OnSetFocus (TimeInfo *pti, HWND hEdit);
void Time_Edit_OnUpdate (TimeInfo *pti, HWND hEdit);
void Time_Edit_SetText (TimeInfo *pti, HWND hEdit);

void Time_GetAMPMSize (HDC hdc, SIZE *pSize, LPTSTR pszAM, LPTSTR pszPM);


BOOL RegisterTimeClass (void)
{
   static BOOL fRegistered = FALSE;

   if (!fRegistered)
      {
      InitializeCriticalSection (&csTime);

      WNDCLASS wc;
      memset (&wc, 0x00, sizeof(wc));
      wc.style = CS_GLOBALCLASS;
      wc.lpfnWndProc = (WNDPROC)TimeProc;
      wc.hInstance = THIS_HINST;
      wc.hCursor = LoadCursor (NULL, MAKEINTRESOURCE (IDC_ARROW));
      wc.hbrBackground = CreateSolidBrush (GetSysColor (COLOR_BTNFACE));
      wc.lpszClassName = cszTIMECLASS;

      if (RegisterClass (&wc))
         fRegistered = TRUE;
      }

   return fRegistered;
}


void Time_SendCallback (TimeInfo *pti, WORD eln, LPARAM lp)
{
   if ((pti->fCanCallBack == TRUE) && !pti->fCallingBack)
      {
      pti->fCallingBack = TRUE;

      SendMessage (GetParent (pti->hTime),
                   WM_COMMAND,
                   MAKELONG ((WORD)GetWindowLong (pti->hTime, GWL_ID), eln),
                   lp);

      pti->fCallingBack = FALSE;
      }
}


BOOL CALLBACK TimeProc (HWND hTime, UINT msg, WPARAM wp, LPARAM lp)
{
   TimeInfo *pti = NULL;

   EnterCriticalSection (&csTime);

   if (msg == WM_CREATE)
      {
      size_t iTime;
      for (iTime = 0; iTime < cTime; ++iTime)
         {
         if (aTime[ iTime ].hTime == NULL)
            break;
         }
      if (iTime >= cTime)
         {
         if (!REALLOC (aTime, cTime, 1+iTime, 4))
            return FALSE;
         }

      memset (&aTime[ iTime ], 0x00, sizeof(TimeInfo));
      aTime[ iTime ].hTime = hTime;

      pti = &aTime[ iTime ];
      }
   else
      {
      for (size_t iTime = 0; !pti && iTime < cTime; ++iTime)
         {
         if (aTime[ iTime ].hTime == hTime)
            pti = &aTime[ iTime ];
         }
      }

   LeaveCriticalSection (&csTime);

   if (pti != NULL)
      {
      switch (msg)
         {
         case WM_CREATE:
            Time_OnCreate (pti);
            break;

         case WM_DESTROY:
            Time_OnDestroy (pti);
            break;

         case WM_RBUTTONDOWN:
         case WM_LBUTTONDOWN:
            Time_OnButtonDown (pti, msg, wp, lp);
            break;

         case WM_SETFOCUS:
            PostMessage (GetParent(hTime), WM_NEXTDLGCTL, (WPARAM)pti->hHours, TRUE);
            break;

         case WM_ENABLE:
            EnableWindow (pti->hHours,   IsWindowEnabled (hTime));
            EnableWindow (pti->hSep1,    IsWindowEnabled (hTime));
            EnableWindow (pti->hMinutes, IsWindowEnabled (hTime));
            EnableWindow (pti->hSpinner, IsWindowEnabled (hTime));

            if (!pti->f24Hour)
               {
               EnableWindow (pti->hSep2, IsWindowEnabled (hTime));
               EnableWindow (pti->hAMPM, IsWindowEnabled (hTime));
               }

            RECT rTime;
            GetRectInParent (hTime, &rTime);
            InvalidateRect (GetParent(hTime), &rTime, TRUE);
            UpdateWindow (GetParent(hTime));
            break;

         case WM_SYSCHAR:
         case WM_CHAR:
            switch (wp)
               {
               case VK_UP:
                  PostMessage (GetParent(pti->hSpinner), WM_VSCROLL, SB_LINEUP, (LPARAM)pti->hSpinner);
                  break;

               case VK_DOWN:
                  PostMessage (GetParent(pti->hSpinner), WM_VSCROLL, SB_LINEDOWN, (LPARAM)pti->hSpinner);
                  break;

               case VK_PRIOR:
                  PostMessage (GetParent(pti->hSpinner), WM_VSCROLL, SB_PAGEUP, (LPARAM)pti->hSpinner);
                  break;

               case VK_NEXT:
                  PostMessage (GetParent(pti->hSpinner), WM_VSCROLL, SB_PAGEDOWN, (LPARAM)pti->hSpinner);
                  break;

               case VK_HOME:
                  PostMessage (GetParent(pti->hSpinner), WM_VSCROLL, SB_TOP, (LPARAM)pti->hSpinner);
                  break;

               case VK_END:
                  PostMessage (GetParent(pti->hSpinner), WM_VSCROLL, SB_BOTTOM, (LPARAM)pti->hSpinner);
                  break;
               }
            break;

         case TM_GETTIME:
            return Time_OnGetTime (pti, wp, lp);

         case TM_SETTIME:
            return Time_OnSetTime (pti, wp, lp);
         }
      }

   return (BOOL)DefWindowProc (hTime, msg, wp, lp);
}


void Time_OnCreate (TimeInfo *pti)
{
   Subclass_AddHook (GetParent(pti->hTime), TimeDlgProc);

   TCHAR szTimeSep[ cchRESOURCE ];
   if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_STIME, szTimeSep, cchRESOURCE))
      lstrcpy (szTimeSep, TEXT(":"));

   TCHAR sz24Hour[ cchRESOURCE ];
   if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_ITIME, sz24Hour, cchRESOURCE))
      pti->f24Hour = FALSE;
   else
      pti->f24Hour = (sz24Hour[0] == TEXT('1')) ? TRUE : FALSE;

   TCHAR sz0Hour[ cchRESOURCE ];
   if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_ITLZERO, sz0Hour, cchRESOURCE))
      pti->f0Hours = FALSE;
   else
      pti->f0Hours = (sz0Hour[0] == TEXT('1')) ? TRUE : FALSE;

   RECT rTime;
   GetClientRect (pti->hTime, &rTime);

   POINT ptTime = {0,0};
   ClientToScreen (pti->hTime, &ptTime);
   ScreenToClient (GetParent (pti->hTime), &ptTime);

   SIZE s88; // size of widest likely double-digit number
   SIZE sTimeSep; // size of ":"
   SIZE sAMPM; // size of "AM/PM" listbox
   TCHAR szAM[ cchRESOURCE ];
   TCHAR szPM[ cchRESOURCE ];

   HDC hdc = GetDC (GetParent (pti->hTime));
   GetTextExtentPoint (hdc, TEXT("88"), lstrlen(TEXT("88")), &s88);
   GetTextExtentPoint (hdc, szTimeSep, lstrlen(szTimeSep), &sTimeSep);
   if (!pti->f24Hour)
      Time_GetAMPMSize (hdc, &sAMPM, szAM, szPM);
   ReleaseDC (GetParent (pti->hTime), hdc);

   LONG cxNumbers = cxRECT(rTime) - GetSystemMetrics (SM_CXVSCROLL) - (sTimeSep.cx);
   if (!pti->f24Hour)
      cxNumbers -= sTimeSep.cx + sAMPM.cx;
   LONG cxMinutes = max( cxNumbers/2, s88.cx );
   LONG cxHours   = cxNumbers - cxMinutes;

   cxMinutes = min (cxMinutes, (LONG)( 1.2 * s88.cx ));
   cxHours   = min (cxHours,   (LONG)( 1.2 * s88.cx ));  // don't be TOO wide.

   LONG cy = cyRECT(rTime);
   LONG yy = ptTime.y;

   pti->idHours = 100+NextControlID (GetParent (pti->hTime));
   pti->hHours = CreateWindow (
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_TABSTOP | ES_MULTILINE | ES_RIGHT | ES_NUMBER,
                ptTime.x, yy, cxHours, cy,
                GetParent(pti->hTime),
                (HMENU)pti->idHours,
                THIS_HINST,
                0);

   pti->hSep1 = CreateWindow (
                TEXT("STATIC"),
                szTimeSep,
                WS_CHILD | SS_CENTER,
                ptTime.x + cxHours, yy, sTimeSep.cx, cy,
                GetParent(pti->hTime),
                (HMENU)-1,
                THIS_HINST,
                0);

   pti->idMinutes = 100+NextControlID (GetParent (pti->hTime));
   pti->hMinutes = CreateWindow (
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_TABSTOP | ES_MULTILINE | ES_RIGHT | ES_NUMBER,
                ptTime.x + cxHours + sTimeSep.cx, yy, cxMinutes, cy,
                GetParent(pti->hTime),
                (HMENU)pti->idMinutes,
                THIS_HINST,
                0);

   if (!pti->f24Hour)
      {
      pti->hSep2 = CreateWindow (
                   TEXT("STATIC"),
                   TEXT(""),
                   WS_CHILD | SS_CENTER,
                   ptTime.x + cxHours + cxMinutes + sTimeSep.cx, yy, sTimeSep.cx, cy,
                   GetParent(pti->hTime),
                   (HMENU)-1,
                   THIS_HINST,
                   0);

      pti->idAMPM = 100+NextControlID (GetParent (pti->hTime));
      pti->hAMPM = CreateWindow (
                   TEXT("LISTBOX"),
                   TEXT(""),
                   WS_CHILD | WS_TABSTOP | LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED | LBS_NOTIFY,
                   ptTime.x + cxHours + cxMinutes + 2 * sTimeSep.cx, yy, sAMPM.cx, cy,
                   GetParent(pti->hTime),
                   (HMENU)pti->idAMPM,
                   THIS_HINST,
                   0);
      }

   Subclass_AddHook (pti->hHours, TimeEditProc);
   Subclass_AddHook (pti->hMinutes, TimeEditProc);

   if (!pti->f24Hour)
      Subclass_AddHook (pti->hAMPM, TimeEditProc);

   HFONT hf = (HFONT)SendMessage (GetParent (pti->hTime), WM_GETFONT, 0, 0);
   SendMessage (pti->hHours,   WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (pti->hSep1,    WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (pti->hMinutes, WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));

   if (!pti->f24Hour)
      {
      SendMessage (pti->hSep2, WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
      SendMessage (pti->hAMPM, WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
      }

   EnableWindow (pti->hHours,   IsWindowEnabled (pti->hTime));
   EnableWindow (pti->hSep1,    IsWindowEnabled (pti->hTime));
   EnableWindow (pti->hMinutes, IsWindowEnabled (pti->hTime));

   if (!pti->f24Hour)
      {
      EnableWindow (pti->hSep2, IsWindowEnabled (pti->hTime));
      EnableWindow (pti->hAMPM, IsWindowEnabled (pti->hTime));

      LB_StartChange (pti->hAMPM, TRUE);
      LB_AddItem (pti->hAMPM, szAM, 0);
      LB_AddItem (pti->hAMPM, szPM, 1);
      LB_EndChange (pti->hAMPM);
      LB_SetSelected (pti->hAMPM, 1);
      }

   ShowWindow (pti->hHours,   TRUE);
   ShowWindow (pti->hSep1,    TRUE);
   ShowWindow (pti->hMinutes, TRUE);

   if (!pti->f24Hour)
      {
      ShowWindow (pti->hSep2, TRUE);
      ShowWindow (pti->hAMPM, TRUE);
      }

   RECT rWindow;
   GetWindowRect (pti->hTime, &rWindow);
   rWindow.right += (cxHours + cxMinutes + sTimeSep.cx) - cxRECT(rTime);
   if (!pti->f24Hour)
      rWindow.right += sTimeSep.cx + sAMPM.cx;

   SetWindowPos (pti->hTime, NULL, 0, 0, cxRECT(rWindow), cyRECT(rWindow),
                 SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);

   RECT rSpinner;
   GetRectInParent (pti->hTime, &rSpinner);
   rSpinner.left = rSpinner.right;
   rSpinner.right = rSpinner.left + GetSystemMetrics (SM_CXVSCROLL);
   rSpinner.bottom -= 2; // just like Win95 does
   CreateSpinner (pti->hHours, 10, FALSE, 0, pti->timeNow.wHour, 24, &rSpinner);
   pti->hSpinner = SP_GetSpinner (pti->hHours);
   pti->hSpinnerBuddy = pti->hHours;

   Time_Edit_SetText (pti, pti->hHours);
   Time_Edit_SetText (pti, pti->hMinutes);
   Time_Edit_OnSetFocus (pti, pti->hHours);
   pti->fCanCallBack = TRUE;
}


void Time_OnDestroy (TimeInfo *pti)
{
   Subclass_RemoveHook (GetParent(pti->hTime), TimeDlgProc);
   pti->hTime = NULL;
}


void Time_OnButtonDown (TimeInfo *pti, UINT msg, WPARAM wp, LPARAM lp)
{
   DWORD dw = GetMessagePos();
   POINT pt;
   pt.x = LOWORD(dw);
   pt.y = HIWORD(dw);  // in screen coordinates

   RECT rTarget;
   HWND hTarget = 0;

   GetWindowRect (pti->hHours, &rTarget);
   if (PtInRect (&rTarget, pt))
      hTarget = pti->hHours;

   GetWindowRect (pti->hMinutes, &rTarget);
   if (PtInRect (&rTarget, pt))
      hTarget = pti->hMinutes;

   if (!pti->f24Hour)
      {
      GetWindowRect (pti->hAMPM, &rTarget);
      if (PtInRect (&rTarget, pt))
         hTarget = pti->hAMPM;
      }

   if (hTarget != 0)
      {
      PostMessage (hTarget, msg, wp, lp);
      }
}


BOOL Time_OnGetTime (TimeInfo *pti, WPARAM wp, LPARAM lp)
{
   SYSTEMTIME *ptime = (SYSTEMTIME*)lp;
   ptime->wHour = pti->timeNow.wHour;
   ptime->wMinute = pti->timeNow.wMinute;
   ptime->wSecond = pti->timeNow.wSecond;
   ptime->wMilliseconds = pti->timeNow.wMilliseconds;
   return TRUE;
}


BOOL Time_OnSetTime (TimeInfo *pti, WPARAM wp, LPARAM lp)
{
   SYSTEMTIME *ptime = (SYSTEMTIME*)lp;
   pti->timeNow = *ptime;

   Time_Edit_SetText (pti, pti->hHours);
   Time_Edit_SetText (pti, pti->hMinutes);

   return TRUE;
}


BOOL CALLBACK TimeDlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hDlg, TimeDlgProc);
   size_t iTime;

   switch (msg)
      {
      case WM_CTLCOLOREDIT:
      case WM_CTLCOLORSTATIC:
      case WM_CTLCOLORLISTBOX:
         for (iTime = 0; iTime < cTime; ++iTime)
            {
            if (aTime[ iTime ].hTime == NULL)
               continue;
            if ( (aTime[ iTime ].hHours   == (HWND)lp) ||
                 (aTime[ iTime ].hSep1    == (HWND)lp) ||
                 (aTime[ iTime ].hMinutes == (HWND)lp) ||
                 ((!aTime[ iTime ].f24Hour) && (aTime[ iTime ].hSep2 == (HWND)lp)) ||
                 ((!aTime[ iTime ].f24Hour) && (aTime[ iTime ].hAMPM == (HWND)lp)) )
               {
               COLORREF clr;
               if (IsWindowEnabled (aTime[ iTime ].hTime))
                  clr = GetSysColor (COLOR_WINDOW);
               else
                  clr = GetSysColor (COLOR_BTNFACE);
               SetBkColor ((HDC)wp, clr);
               return (BOOL)(INT_PTR)CreateSolidBrush (clr);
               }
            }
         break;

      case WM_MEASUREITEM: 
         LPMEASUREITEMSTRUCT lpmis;
         lpmis = (LPMEASUREITEMSTRUCT)lp; 
         HDC hdc;
         hdc = GetDC (hDlg);
         SIZE sAMPM;
         Time_GetAMPMSize (hdc, &sAMPM, NULL, NULL);
         ReleaseDC (hDlg, hdc);
         lpmis->itemHeight = sAMPM.cy;
         return TRUE; 

      case WM_DRAWITEM: 
         LPDRAWITEMSTRUCT lpdis;
         lpdis = (LPDRAWITEMSTRUCT)lp; 

         COLORREF clrBack;
         COLORREF clrFore;
         if (!IsWindowEnabled (lpdis->hwndItem))
            {
            clrBack = GetSysColor (COLOR_BTNFACE);
            clrFore = GetSysColor (COLOR_GRAYTEXT);
            }
         else if ((lpdis->itemState & ODS_SELECTED) && (GetFocus() == lpdis->hwndItem))
            {
            clrBack = GetSysColor (COLOR_HIGHLIGHT);
            clrFore = GetSysColor (COLOR_HIGHLIGHTTEXT);
            }
         else
            {
            clrBack = GetSysColor (COLOR_WINDOW);
            clrFore = GetSysColor (COLOR_BTNTEXT);
            }

         TCHAR szText[ cchRESOURCE ];
         SendMessage (lpdis->hwndItem, LB_GETTEXT, lpdis->itemID, (LPARAM)szText); 

         SetTextColor (lpdis->hDC, clrFore);
         SetBkColor (lpdis->hDC, clrBack);
         SetBkMode (lpdis->hDC, OPAQUE);
         TextOut (lpdis->hDC, 0, lpdis->rcItem.top, szText, lstrlen(szText));
         return TRUE;

      case WM_COMMAND:
         for (iTime = 0; iTime < cTime; ++iTime)
            {
            if (aTime[ iTime ].hTime == NULL)
               continue;
            if ( (aTime[ iTime ].idHours   == LOWORD(wp)) ||
                 (aTime[ iTime ].idMinutes == LOWORD(wp)) ||
                 ((!aTime[ iTime ].f24Hour) && (aTime[ iTime ].idAMPM == LOWORD(wp))) )
               {
               if (HIWORD(wp) == SPN_UPDATE)
                  {
                  Time_Edit_OnUpdate (&aTime[ iTime ], GetDlgItem(hDlg,LOWORD(wp)));
                  }
               break;
               }
            }
         break;
      }

   if (oldProc)
      return (BOOL)CallWindowProc ((WNDPROC)oldProc, hDlg, msg, wp, lp);
   else
      return (BOOL)DefWindowProc (hDlg, msg, wp, lp);
}


BOOL CALLBACK TimeEditProc (HWND hEdit, UINT msg, WPARAM wp, LPARAM lp)
{
   TimeInfo *pti = NULL;

   EnterCriticalSection (&csTime);

   for (size_t iTime = 0; !pti && iTime < cTime; ++iTime)
      {
      if ( (aTime[ iTime ].hHours   == hEdit) ||
           (aTime[ iTime ].hMinutes == hEdit) ||
           ((!aTime[ iTime ].f24Hour) && (aTime[ iTime ].hAMPM == hEdit)) )
         {
         pti = &aTime[ iTime ];
         }
      }

   LeaveCriticalSection (&csTime);

   if (pti)
      {
      switch (msg)
         {
         case WM_SETFOCUS:
            Time_Edit_OnSetFocus (pti, hEdit);
            break;
         }
      }

   PVOID oldProc = Subclass_FindNextHook (hEdit, TimeEditProc);
   if (oldProc)
      return (BOOL)CallWindowProc ((WNDPROC)oldProc, hEdit, msg, wp, lp);
   else
      return (BOOL)DefWindowProc (hEdit, msg, wp, lp);
}


void Time_Edit_OnSetFocus (TimeInfo *pti, HWND hEdit)
{
   DWORD dwMin;
   DWORD dwNow;
   DWORD dwMax;

   pti->fCanCallBack --;

   if (hEdit == pti->hHours)
      {
      dwMin = (pti->f24Hour) ? 0 : 1;
      dwNow = (pti->f24Hour) ? pti->timeNow.wHour : (pti->timeNow.wHour % 12);
      dwMax = (pti->f24Hour) ? 24 : 12;

      if (!pti->f24Hour && !dwNow)
         dwNow = 12;
      }
   else if (hEdit == pti->hMinutes)
      {
      dwMin = 0;
      dwNow = pti->timeNow.wMinute;
      dwMax = 59;
      }
   else // (hEdit == pti->hAMPM)
      {
      dwMin = 0;
      dwNow = (pti->timeNow.wHour >= 12) ? 1 : 0;
      dwMax = 1;
      }

   if (pti->hSpinnerBuddy != hEdit)
      {
      SP_SetBuddy (pti->hSpinnerBuddy, hEdit, FALSE);
      pti->hSpinnerBuddy = hEdit;
      }

   SP_SetRange (hEdit, dwMin, dwMax);
   SP_SetPos (hEdit, dwNow);

   if ((hEdit == pti->hHours) && (!pti->f0Hours))
      SP_SetFormat (hEdit, TEXT("%lu"));
   else if ((hEdit == pti->hHours) || (hEdit == pti->hMinutes))
      SP_SetFormat (hEdit, TEXT("%02lu"));

   if ((hEdit == pti->hHours) || (hEdit == pti->hMinutes))
      SendMessage (hEdit, EM_SETSEL, (WPARAM)0, (LPARAM)-1);  // select all

   pti->fCanCallBack ++;
}


void Time_Edit_OnUpdate (TimeInfo *pti, HWND hEdit)
{
   TCHAR szText[ cchRESOURCE ];

   if ((hEdit == pti->hHours) || (hEdit == pti->hAMPM))
      {
      GetWindowText (pti->hHours, szText, 256);
      pti->timeNow.wHour = (WORD)atol (szText);

      if (!pti->f24Hour)
         {
         if ((pti->timeNow.wHour < 12) && (LB_GetSelected (pti->hAMPM) == 1))
            pti->timeNow.wHour += 12;
         else if ((pti->timeNow.wHour >= 12) && (LB_GetSelected (pti->hAMPM) == 0))
            pti->timeNow.wHour -= 12;
         }
      }

   if (hEdit == pti->hMinutes)
      {
      GetWindowText (pti->hMinutes, szText, 256);
      pti->timeNow.wMinute = (WORD)atol (szText);
      }

   if ((pti->timeNow.wHour == 24) && (pti->timeNow.wMinute != 0))
      {
      pti->timeNow.wMinute = 0;
      Time_Edit_SetText (pti, pti->hMinutes);
      }

   SYSTEMTIME st = pti->timeNow;
   Time_SendCallback (pti, TN_UPDATE, (LPARAM)&st);
}


void Time_Edit_SetText (TimeInfo *pti, HWND hEdit)
{
   DWORD dwNow;

   if (hEdit == pti->hHours)
      {
      BOOL fPM = (pti->timeNow.wHour >= 12) ? TRUE : FALSE;

      if (pti->f24Hour)
         dwNow = pti->timeNow.wHour;
      else if ((dwNow = pti->timeNow.wHour % 12) == 0)
         dwNow = 12;

      if (pti->hSpinnerBuddy == hEdit)
         {
         SP_SetPos (hEdit, dwNow);
         }
      else
         {
         TCHAR szText[ cchRESOURCE ];

         if (!pti->f0Hours)
            wsprintf (szText, TEXT("%lu"), dwNow);
         else
            wsprintf (szText, TEXT("%02lu"), dwNow);

         SetWindowText (hEdit, szText);
         }

      if (!pti->f24Hour)
         {
         LB_SetSelected (pti->hAMPM, (int)fPM);
         }
      }
   else if (hEdit == pti->hMinutes)
      {
      if (pti->hSpinnerBuddy == hEdit)
         {
         SP_SetPos (hEdit, pti->timeNow.wMinute);
         }
      else
         {
         TCHAR szText[ cchRESOURCE ];
         wsprintf (szText, TEXT("%02lu"), pti->timeNow.wMinute);
         SetWindowText (hEdit, szText);
         }
      }
}


void Time_GetAMPMSize (HDC hdc, SIZE *pSize, LPTSTR pszAM, LPTSTR pszPM)
{
   TCHAR szAM[ cchRESOURCE ];
   TCHAR szPM[ cchRESOURCE ];
   if (!pszAM)
      pszAM = szAM;
   if (!pszPM)
      pszPM = szPM;

   if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_S1159, pszAM, cchRESOURCE))
      lstrcpy (pszAM, TEXT("AM"));

   if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_S2359, pszPM, cchRESOURCE))
      lstrcpy (pszPM, TEXT("PM"));

   SIZE sAM; // size of "AM"
   GetTextExtentPoint (hdc, pszAM, lstrlen(pszAM), &sAM);

   SIZE sPM; // size of "PM"
   GetTextExtentPoint (hdc, pszPM, lstrlen(pszPM), &sPM);
   pSize->cx = max( sAM.cx, sPM.cx );
   pSize->cy = max( sAM.cy, sPM.cy );
}

