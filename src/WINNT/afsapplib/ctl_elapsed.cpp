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
#include <WINNT/dialog.h>
#include <WINNT/resize.h>	// for GetRectInParent()
#include <WINNT/subclass.h>
#include <WINNT/ctl_spinner.h>
#include <WINNT/ctl_elapsed.h>
#include <WINNT/TaLocale.h>

#ifndef cchRESOURCE
#define cchRESOURCE 256
#endif


/*
 * MISCELLANEOUS ______________________________________________________________
 *
 */

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) ElapsedReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
BOOL ElapsedReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
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
 * ELAPSEDS ___________________________________________________________________
 *
 */

typedef struct
   {
   HWND hElapsed;
   HWND hHours;
   HWND hSep1;
   HWND hMinutes;
   HWND hSep2;
   HWND hSeconds;
   HWND hSpinner;
   HWND hSpinnerBuddy;

   WORD idHours;
   WORD idMinutes;
   WORD idSeconds;

   SYSTEMTIME timeMin;  // only hour, minute, second fields are relevant
   SYSTEMTIME timeNow;  // only hour, minute, second fields are relevant
   SYSTEMTIME timeMax;  // only hour, minute, second fields are relevant

   BOOL  fCallingBack;
   BOOL  fCanCallBack;
   } ElapsedInfo;

static CRITICAL_SECTION csElapsed;
static ElapsedInfo     *aElapsed = NULL;
static size_t           cElapsed = 0;

#define cszELAPSEDCLASS TEXT("Elapsed")

BOOL CALLBACK ElapsedProc (HWND hElapsed, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK ElapsedEditProc (HWND hEdit, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK ElapsedDlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Elapsed_SendCallback (ElapsedInfo *pei, WORD eln, LPARAM lp);

void Elapsed_OnCreate (ElapsedInfo *psi);
void Elapsed_OnDestroy (ElapsedInfo *psi);
void Elapsed_OnButtonDown (ElapsedInfo *pei, UINT msg, WPARAM wp, LPARAM lp);
BOOL Elapsed_OnGetRange (ElapsedInfo *psi, WPARAM wp, LPARAM lp);
BOOL Elapsed_OnSetRange (ElapsedInfo *psi, WPARAM wp, LPARAM lp);
BOOL Elapsed_OnGetTime (ElapsedInfo *psi, WPARAM wp, LPARAM lp);
BOOL Elapsed_OnSetTime (ElapsedInfo *psi, WPARAM wp, LPARAM lp);

void Elapsed_Edit_OnSetFocus (ElapsedInfo *pei, HWND hEdit);
void Elapsed_Edit_GetSpinnerRange (ElapsedInfo *pei, HWND hEdit, DWORD *pdwMin, DWORD *pdwPos, DWORD *pdwMax);
void Elapsed_Edit_OnUpdate (ElapsedInfo *pei, HWND hEdit, DWORD dwNew);
void Elapsed_Edit_OnEnforceRange (ElapsedInfo *pei, HWND hEdit);
void Elapsed_Edit_SetText (ElapsedInfo *pei, HWND hEdit);


BOOL RegisterElapsedClass (void)
{
   static BOOL fRegistered = FALSE;

   if (!fRegistered)
      {
      InitializeCriticalSection (&csElapsed);

      WNDCLASS wc;
      memset (&wc, 0x00, sizeof(wc));
      wc.style = CS_GLOBALCLASS;
      wc.lpfnWndProc = (WNDPROC)ElapsedProc;
      wc.hInstance = THIS_HINST;
      wc.hCursor = LoadCursor (NULL, MAKEINTRESOURCE (IDC_ARROW));
      wc.hbrBackground = CreateSolidBrush (GetSysColor (COLOR_BTNFACE));
      wc.lpszClassName = cszELAPSEDCLASS;

      if (RegisterClass (&wc))
         fRegistered = TRUE;
      }

   return fRegistered;
}


void Elapsed_SendCallback (ElapsedInfo *pei, WORD eln, LPARAM lp)
{
   if ((pei->fCanCallBack == TRUE) && !pei->fCallingBack)
      {
      pei->fCallingBack = TRUE;

      SendMessage (GetParent (pei->hElapsed),
                   WM_COMMAND,
                   MAKELONG ((WORD)GetWindowLong (pei->hElapsed, GWL_ID), eln),
                   lp);

      pei->fCallingBack = FALSE;
      }
}


BOOL CALLBACK ElapsedProc (HWND hElapsed, UINT msg, WPARAM wp, LPARAM lp)
{
   ElapsedInfo *pei = NULL;

   EnterCriticalSection (&csElapsed);

   if (msg == WM_CREATE)
      {
      size_t iElapsed;
      for (iElapsed = 0; iElapsed < cElapsed; ++iElapsed)
         {
         if (aElapsed[ iElapsed ].hElapsed == NULL)
            break;
         }
      if (iElapsed >= cElapsed)
         {
         if (!REALLOC (aElapsed, cElapsed, 1+iElapsed, 4))
            return FALSE;
         }

      memset (&aElapsed[ iElapsed ], 0x00, sizeof(ElapsedInfo));
      aElapsed[ iElapsed ].hElapsed = hElapsed;

      pei = &aElapsed[ iElapsed ];
      }
   else
      {
      for (size_t iElapsed = 0; !pei && iElapsed < cElapsed; ++iElapsed)
         {
         if (aElapsed[ iElapsed ].hElapsed == hElapsed)
            pei = &aElapsed[ iElapsed ];
         }
      }

   LeaveCriticalSection (&csElapsed);

   if (pei != NULL)
      {
      switch (msg)
         {
         case WM_CREATE:
            Elapsed_OnCreate (pei);
            break;

         case WM_DESTROY:
            Elapsed_OnDestroy (pei);
            break;

         case WM_RBUTTONDOWN:
         case WM_LBUTTONDOWN:
            Elapsed_OnButtonDown (pei, msg, wp, lp);
            break;

         case WM_SETFOCUS:
            PostMessage (GetParent(hElapsed), WM_NEXTDLGCTL, (WPARAM)pei->hHours, TRUE);
            break;

         case WM_ENABLE:
            EnableWindow (pei->hHours,   IsWindowEnabled (hElapsed));
            EnableWindow (pei->hSep1,    IsWindowEnabled (hElapsed));
            EnableWindow (pei->hMinutes, IsWindowEnabled (hElapsed));
            EnableWindow (pei->hSep2,    IsWindowEnabled (hElapsed));
            EnableWindow (pei->hSeconds, IsWindowEnabled (hElapsed));
            EnableWindow (pei->hSpinner, IsWindowEnabled (hElapsed));

            RECT rElapsed;
            GetRectInParent (hElapsed, &rElapsed);
            InvalidateRect (GetParent(hElapsed), &rElapsed, TRUE);
            UpdateWindow (GetParent(hElapsed));
            break;

         case WM_SYSCHAR:
         case WM_CHAR:
            switch (wp)
               {
               case VK_UP:
                  PostMessage (GetParent(pei->hSpinner), WM_VSCROLL, SB_LINEUP, (LPARAM)pei->hSpinner);
                  break;

               case VK_DOWN:
                  PostMessage (GetParent(pei->hSpinner), WM_VSCROLL, SB_LINEDOWN, (LPARAM)pei->hSpinner);
                  break;

               case VK_PRIOR:
                  PostMessage (GetParent(pei->hSpinner), WM_VSCROLL, SB_PAGEUP, (LPARAM)pei->hSpinner);
                  break;

               case VK_NEXT:
                  PostMessage (GetParent(pei->hSpinner), WM_VSCROLL, SB_PAGEDOWN, (LPARAM)pei->hSpinner);
                  break;

               case VK_HOME:
                  PostMessage (GetParent(pei->hSpinner), WM_VSCROLL, SB_TOP, (LPARAM)pei->hSpinner);
                  break;

               case VK_END:
                  PostMessage (GetParent(pei->hSpinner), WM_VSCROLL, SB_BOTTOM, (LPARAM)pei->hSpinner);
                  break;
               }
            break;

         case ELM_GETRANGE:
            return Elapsed_OnGetRange (pei, wp, lp);

         case ELM_SETRANGE:
            return Elapsed_OnSetRange (pei, wp, lp);

         case ELM_GETTIME:
            return Elapsed_OnGetTime (pei, wp, lp);

         case ELM_SETTIME:
            return Elapsed_OnSetTime (pei, wp, lp);
         }
      }

   return DefWindowProc (hElapsed, msg, wp, lp);
}


void Elapsed_OnCreate (ElapsedInfo *pei)
{
   Subclass_AddHook (GetParent(pei->hElapsed), ElapsedDlgProc);

   TCHAR szTimeSep[ cchRESOURCE ];
   if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_STIME, szTimeSep, cchRESOURCE))
      lstrcpy (szTimeSep, TEXT(":"));

   RECT rElapsed;
   GetClientRect (pei->hElapsed, &rElapsed);

   POINT ptElapsed = {0,0};
   ClientToScreen (pei->hElapsed, &ptElapsed);
   ScreenToClient (GetParent (pei->hElapsed), &ptElapsed);

   SIZE s88; // size of widest likely double-digit number
   SIZE s888; // maximum width to which we'll go for second- and minute- fields
   SIZE sTimeSep; // size of ":"

   HDC hdc = GetDC (GetParent (pei->hElapsed));
   GetTextExtentPoint (hdc, TEXT("88"), lstrlen(TEXT("88")), &s88);
   GetTextExtentPoint (hdc, szTimeSep, lstrlen(szTimeSep), &sTimeSep);
   ReleaseDC (GetParent (pei->hElapsed), hdc);

   s888 = s88;
   s888.cx = (LONG)( (double)(s88.cx) * 1.2 );

   LONG cxNumbers = cxRECT(rElapsed) - (2 * sTimeSep.cx) - GetSystemMetrics (SM_CXVSCROLL);
   LONG cxSeconds = min( max( cxNumbers/3, s88.cx ), s888.cx );
   LONG cxMinutes = min( max( cxNumbers/3, s88.cx ), s888.cx );
   LONG cxHours   = cxNumbers - cxSeconds - cxMinutes;
   LONG yy = ptElapsed.y;
   LONG cy = cyRECT(rElapsed);

   pei->idHours = 100+NextControlID (GetParent (pei->hElapsed));
   pei->hHours = CreateWindow (
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_TABSTOP | ES_RIGHT | ES_NUMBER,
                ptElapsed.x, yy, cxHours, cy,
                GetParent(pei->hElapsed),
                (HMENU)pei->idHours,
                THIS_HINST,
                0);

   pei->hSep1 = CreateWindow (
                TEXT("STATIC"),
                szTimeSep,
                WS_CHILD,
                ptElapsed.x + cxHours, yy, sTimeSep.cx, cy,
                GetParent(pei->hElapsed),
                (HMENU)-1,
                THIS_HINST,
                0);

   pei->idMinutes = 100+NextControlID (GetParent (pei->hElapsed));
   pei->hMinutes = CreateWindow (
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_TABSTOP | ES_RIGHT | ES_NUMBER,
                ptElapsed.x + cxHours + sTimeSep.cx, yy, cxMinutes, cy,
                GetParent(pei->hElapsed),
                (HMENU)pei->idMinutes,
                THIS_HINST,
                0);

   pei->hSep2 = CreateWindow (
                TEXT("STATIC"),
                szTimeSep,
                WS_CHILD,
                ptElapsed.x + cxHours + cxMinutes + sTimeSep.cx, yy, sTimeSep.cx, cy,
                GetParent(pei->hElapsed),
                (HMENU)-1,
                THIS_HINST,
                0);

   pei->idSeconds = 100+NextControlID (GetParent (pei->hElapsed));
   pei->hSeconds = CreateWindow (
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_TABSTOP | ES_RIGHT | ES_NUMBER,
                ptElapsed.x + cxHours + cxMinutes + 2 * sTimeSep.cx, yy, cxSeconds, cy,
                GetParent(pei->hElapsed),
                (HMENU)pei->idSeconds,
                THIS_HINST,
                0);

   Subclass_AddHook (pei->hHours,   ElapsedEditProc);
   Subclass_AddHook (pei->hMinutes, ElapsedEditProc);
   Subclass_AddHook (pei->hSeconds, ElapsedEditProc);

   HFONT hf = (HFONT)SendMessage (GetParent (pei->hElapsed), WM_GETFONT, 0, 0);
   SendMessage (pei->hHours,   WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (pei->hSep1,    WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (pei->hMinutes, WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (pei->hSep2,    WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (pei->hSeconds, WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));

   EnableWindow (pei->hHours,   IsWindowEnabled (pei->hElapsed));
   EnableWindow (pei->hSep1,    IsWindowEnabled (pei->hElapsed));
   EnableWindow (pei->hMinutes, IsWindowEnabled (pei->hElapsed));
   EnableWindow (pei->hSep2,    IsWindowEnabled (pei->hElapsed));
   EnableWindow (pei->hSeconds, IsWindowEnabled (pei->hElapsed));

   ShowWindow (pei->hHours,   TRUE);
   ShowWindow (pei->hSep1,    TRUE);
   ShowWindow (pei->hMinutes, TRUE);
   ShowWindow (pei->hSep2,    TRUE);
   ShowWindow (pei->hSeconds, TRUE);

   RECT rWindow;
   GetWindowRect (pei->hElapsed, &rWindow);
   rWindow.right += (cxHours + cxMinutes + cxSeconds + 2 * sTimeSep.cx) - cxRECT(rElapsed);

   SetWindowPos (pei->hElapsed, NULL, 0, 0, cxRECT(rWindow), cyRECT(rWindow),
                 SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);

   SET_ELAPSED_TIME(&pei->timeMin,  0, 0, 0);
   SET_ELAPSED_TIME(&pei->timeMax, 24, 0, 0);

   Elapsed_Edit_OnEnforceRange (pei, pei->hHours);
   Elapsed_Edit_OnEnforceRange (pei, pei->hMinutes);
   Elapsed_Edit_OnEnforceRange (pei, pei->hSeconds);

   Elapsed_Edit_OnSetFocus (pei, pei->hHours);

   pei->fCanCallBack = TRUE;
}


void Elapsed_OnDestroy (ElapsedInfo *pei)
{
   Subclass_RemoveHook (GetParent(pei->hElapsed), ElapsedDlgProc);
   pei->hElapsed = NULL;
}


void Elapsed_OnButtonDown (ElapsedInfo *pei, UINT msg, WPARAM wp, LPARAM lp)
{
   DWORD dw = GetMessagePos();
   POINT pt;
   pt.x = LOWORD(dw);
   pt.y = HIWORD(dw);  // in screen coordinates

   RECT rTarget;
   HWND hTarget = 0;

   GetWindowRect (pei->hHours, &rTarget);
   if (PtInRect (&rTarget, pt))
      hTarget = pei->hHours;

   GetWindowRect (pei->hMinutes, &rTarget);
   if (PtInRect (&rTarget, pt))
      hTarget = pei->hMinutes;

   GetWindowRect (pei->hSeconds, &rTarget);
   if (PtInRect (&rTarget, pt))
      hTarget = pei->hSeconds;

   if (hTarget != 0)
      {
      PostMessage (hTarget, msg, wp, lp);
      }
}


BOOL Elapsed_OnGetRange (ElapsedInfo *pei, WPARAM wp, LPARAM lp)
{
   SYSTEMTIME *ptimeMin = (SYSTEMTIME*)wp;
   SYSTEMTIME *ptimeMax = (SYSTEMTIME*)lp;

   *ptimeMin = pei->timeMin;
   *ptimeMax = pei->timeMax;
   return TRUE;
}


BOOL Elapsed_OnSetRange (ElapsedInfo *pei, WPARAM wp, LPARAM lp)
{
   SYSTEMTIME *ptimeMin = (SYSTEMTIME*)wp;
   SYSTEMTIME *ptimeMax = (SYSTEMTIME*)lp;

   pei->timeMin = *ptimeMin;
   pei->timeMax = *ptimeMax;

   Elapsed_Edit_OnEnforceRange (pei, pei->hHours);
   Elapsed_Edit_OnEnforceRange (pei, pei->hMinutes);
   Elapsed_Edit_OnEnforceRange (pei, pei->hSeconds);

   return TRUE;
}


BOOL Elapsed_OnGetTime (ElapsedInfo *pei, WPARAM wp, LPARAM lp)
{
   SYSTEMTIME *ptime = (SYSTEMTIME*)lp;
   *ptime = pei->timeNow;
   return TRUE;
}


BOOL Elapsed_OnSetTime (ElapsedInfo *pei, WPARAM wp, LPARAM lp)
{
   SYSTEMTIME *ptime = (SYSTEMTIME*)lp;
   pei->timeNow = *ptime;
   Elapsed_Edit_SetText (pei, pei->hHours);
   Elapsed_Edit_SetText (pei, pei->hMinutes);
   Elapsed_Edit_SetText (pei, pei->hSeconds);
   return TRUE;
}


BOOL CALLBACK ElapsedDlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hDlg, ElapsedDlgProc);
   size_t iElapsed;

   switch (msg)
      {
      case WM_CTLCOLOREDIT:
      case WM_CTLCOLORSTATIC:
         for (iElapsed = 0; iElapsed < cElapsed; ++iElapsed)
            {
            if (aElapsed[ iElapsed ].hElapsed == NULL)
               continue;
            if ( (aElapsed[ iElapsed ].hHours   == (HWND)lp) ||
                 (aElapsed[ iElapsed ].hSep1    == (HWND)lp) ||
                 (aElapsed[ iElapsed ].hMinutes == (HWND)lp) ||
                 (aElapsed[ iElapsed ].hSep2    == (HWND)lp) ||
                 (aElapsed[ iElapsed ].hSeconds == (HWND)lp) )
               {
               COLORREF clr;
               if (IsWindowEnabled (aElapsed[ iElapsed ].hElapsed))
                  clr = GetSysColor (COLOR_WINDOW);
               else
                  clr = GetSysColor (COLOR_BTNFACE);
               SetBkColor ((HDC)wp, clr);
               return (BOOL)CreateSolidBrush (clr);
               }
            }
         break;

      case WM_COMMAND:
         for (iElapsed = 0; iElapsed < cElapsed; ++iElapsed)
            {
            if (aElapsed[ iElapsed ].hElapsed == NULL)
               continue;
            if ( (aElapsed[ iElapsed ].idHours   == LOWORD(wp)) ||
                 (aElapsed[ iElapsed ].idMinutes == LOWORD(wp)) ||
                 (aElapsed[ iElapsed ].idSeconds == LOWORD(wp)) )
               {
               if (HIWORD(wp) == SPN_UPDATE)
                  {
                  Elapsed_Edit_OnUpdate (&aElapsed[ iElapsed ], GetDlgItem (hDlg, LOWORD(wp)), (DWORD)lp);
                  }
               break;
               }
            }
         break;
      }

   if (oldProc)
      return CallWindowProc ((WNDPROC)oldProc, hDlg, msg, wp, lp);
   else
      return DefWindowProc (hDlg, msg, wp, lp);
}


BOOL CALLBACK ElapsedEditProc (HWND hEdit, UINT msg, WPARAM wp, LPARAM lp)
{
   ElapsedInfo *pei = NULL;

   EnterCriticalSection (&csElapsed);

   for (size_t iElapsed = 0; !pei && iElapsed < cElapsed; ++iElapsed)
      {
      if ( (aElapsed[ iElapsed ].hHours   == hEdit) ||
           (aElapsed[ iElapsed ].hMinutes == hEdit) ||
           (aElapsed[ iElapsed ].hSeconds == hEdit) )
         {
         pei = &aElapsed[ iElapsed ];
         }
      }

   LeaveCriticalSection (&csElapsed);

   if (pei)
      {
      switch (msg)
         {
         case WM_SETFOCUS:
            Elapsed_Edit_OnSetFocus (pei, hEdit);
            break;
         }
      }

   PVOID oldProc = Subclass_FindNextHook (hEdit, ElapsedEditProc);
   if (oldProc)
      return CallWindowProc ((WNDPROC)oldProc, hEdit, msg, wp, lp);
   else
      return DefWindowProc (hEdit, msg, wp, lp);
}


void Elapsed_Edit_OnSetFocus (ElapsedInfo *pei, HWND hEdit)
{
   pei->fCanCallBack --;

   RECT rSpinner;
   GetRectInParent (pei->hElapsed, &rSpinner);
   rSpinner.left = rSpinner.right;
   rSpinner.right = rSpinner.left + GetSystemMetrics (SM_CXVSCROLL);
   rSpinner.bottom -= 2; // just like Win95 does

   DWORD dwMin;
   DWORD dwPos;
   DWORD dwMax;
   Elapsed_Edit_GetSpinnerRange (pei, hEdit, &dwMin, &dwPos, &dwMax);

   CreateSpinner (hEdit, 10, FALSE, dwMin, dwPos, dwMax, &rSpinner);
   if (pei->hSpinner)
      DestroyWindow (pei->hSpinner);

   if (hEdit == pei->hHours)
      SP_SetFormat (hEdit, TEXT("%lu"));
   else
      SP_SetFormat (hEdit, TEXT("%02lu"));

   pei->hSpinner = SP_GetSpinner (hEdit);
   pei->hSpinnerBuddy = hEdit;

   SendMessage (hEdit, EM_SETSEL, (WPARAM)0, (LPARAM)-1);  // select all

   pei->fCanCallBack ++;
}


void Elapsed_Edit_GetSpinnerRange (ElapsedInfo *pei, HWND hEdit, DWORD *pdwMin, DWORD *pdwNow, DWORD *pdwMax)
{
   if (hEdit == pei->hHours)
      {
      *pdwMin = pei->timeMin.wHour + pei->timeMin.wDay * 24;
      *pdwNow = pei->timeNow.wHour + pei->timeNow.wDay * 24;
      *pdwMax = pei->timeMax.wHour + pei->timeMax.wDay * 24;
      }
   else if (hEdit == pei->hMinutes)
      {
      if ( (pei->timeNow.wDay == pei->timeMin.wDay) &&
           (pei->timeNow.wHour == pei->timeMin.wHour) )
         *pdwMin = pei->timeMin.wMinute;
      else
         *pdwMin = 0;

      *pdwNow = pei->timeNow.wMinute;

      if ( (pei->timeNow.wDay == pei->timeMax.wDay) &&
           (pei->timeNow.wHour == pei->timeMax.wHour) )
         *pdwMax = pei->timeMax.wMinute;
      else
         *pdwMax = 59;
      }
   else if (hEdit == pei->hSeconds)
      {
      if ( (pei->timeNow.wDay == pei->timeMin.wDay) &&
           (pei->timeNow.wHour == pei->timeMin.wHour) &&
           (pei->timeNow.wMinute == pei->timeMin.wMinute) )
         *pdwMin = pei->timeMin.wSecond;
      else
         *pdwMin = 0;

      *pdwNow = pei->timeNow.wSecond;

      if ( (pei->timeNow.wDay == pei->timeMax.wDay) &&
           (pei->timeNow.wHour == pei->timeMax.wHour) &&
           (pei->timeNow.wMinute == pei->timeMax.wMinute) )
         *pdwMax = pei->timeMax.wSecond;
      else
         *pdwMax = 59;
      }
}


void Elapsed_Edit_OnUpdate (ElapsedInfo *pei, HWND hEdit, DWORD dwNew)
{
   DWORD dwMin;
   DWORD dwNow;
   DWORD dwMax;

   Elapsed_Edit_GetSpinnerRange (pei, hEdit, &dwMin, &dwNow, &dwMax);

   dwNow = limit( dwMin, dwNew, dwMax );

   if (hEdit == pei->hHours)
      {
      pei->timeNow.wDay = (WORD)dwNow / 24;
      pei->timeNow.wHour = (WORD)dwNow % 24;
      Elapsed_Edit_OnEnforceRange (pei, pei->hMinutes);
      Elapsed_Edit_OnEnforceRange (pei, pei->hSeconds);
      }
   else if (hEdit == pei->hMinutes)
      {
      pei->timeNow.wMinute = (WORD)dwNow;
      Elapsed_Edit_OnEnforceRange (pei, pei->hSeconds);
      }
   else if (hEdit == pei->hSeconds)
      {
      pei->timeNow.wSecond = (WORD)dwNow;
      }

   SYSTEMTIME st = pei->timeNow;
   Elapsed_SendCallback (pei, ELN_UPDATE, (LPARAM)&st);
}


void Elapsed_Edit_OnEnforceRange (ElapsedInfo *pei, HWND hEdit)
{
   DWORD dwMin;
   DWORD dwNow;
   DWORD dwMax;
   Elapsed_Edit_GetSpinnerRange (pei, hEdit, &dwMin, &dwNow, &dwMax);

   if (!inlimit( dwMin, dwNow, dwMax ))
      {
      dwNow = limit( dwMin, dwNow, dwMax );

      if (hEdit == pei->hHours)
         {
         pei->timeNow.wDay = (WORD)dwNow / 24;
         pei->timeNow.wHour = (WORD)dwNow % 24;
         }
      else if (hEdit == pei->hMinutes)
         {
         pei->timeNow.wMinute = (WORD)dwNow;
         }
      else if (hEdit == pei->hSeconds)
         {
         pei->timeNow.wSecond = (WORD)dwNow;
         }

      if (pei->hSpinnerBuddy == hEdit)
         SP_SetRange (hEdit, dwMin, dwMax);
      Elapsed_Edit_SetText (pei, hEdit);
      }
   else if (pei->hSpinnerBuddy == hEdit)
      {
      SP_SetRange (hEdit, dwMin, dwMax);
      }
}


void Elapsed_Edit_SetText (ElapsedInfo *pei, HWND hEdit)
{
   DWORD dwMin;
   DWORD dwNow;
   DWORD dwMax;
   Elapsed_Edit_GetSpinnerRange (pei, hEdit, &dwMin, &dwNow, &dwMax);

   if (pei->hSpinnerBuddy == hEdit)
      {
      SP_SetPos (hEdit, dwNow);
      }
   else
      {
      TCHAR szText[ cchRESOURCE ];

      if (hEdit == pei->hHours)
         wsprintf (szText, TEXT("%lu"), dwNow);
      else
         wsprintf (szText, TEXT("%02lu"), dwNow);

      SetWindowText (hEdit, szText);
      }
}

