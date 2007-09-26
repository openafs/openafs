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
#include <WINNT/ctl_date.h>
#include <WINNT/TaLocale.h>

#ifndef cchRESOURCE
#define cchRESOURCE 256
#endif


/*
 * MISCELLANEOUS ______________________________________________________________
 *
 */

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) DateReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
BOOL DateReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
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
 * DATE _______________________________________________________________________
 *
 */

typedef struct
   {
   HWND hDate;
   HWND hFirst; // one of hYear, hMonth, hDay
   HWND hYear;
   HWND hSep1;
   HWND hMonth;
   HWND hSep2;
   HWND hDay;
   HWND hSpinner;
   HWND hSpinnerBuddy;

   WORD idYear;
   WORD idMonth;
   WORD idDay;

   SYSTEMTIME dateNow;  // only wYear,wMonth,wDay fields are relevant

   DWORD dwFormat;      // 0=M/D/Y, 1=D/M/Y, 2=Y/M/D
   BOOL  fCallingBack;
   } DateInfo;

static CRITICAL_SECTION csDate;
static DateInfo        *aDate = NULL;
static size_t           cDate = 0;

#define cszDATECLASS TEXT("Date")

HRESULT CALLBACK DateProc (HWND hDate, UINT msg, WPARAM wp, LPARAM lp);
HRESULT CALLBACK DateEditProc (HWND hEdit, UINT msg, WPARAM wp, LPARAM lp);
HRESULT CALLBACK DateDlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Date_SendCallback (DateInfo *pdi, WORD eln, LPARAM lp);

void Date_OnCreate (DateInfo *pdi);
void Date_OnDestroy (DateInfo *pdi);
void Date_OnButtonDown (DateInfo *pdi, UINT msg, WPARAM wp, LPARAM lp);
BOOL Date_OnGetDate (DateInfo *pdi, WPARAM wp, LPARAM lp);
BOOL Date_OnSetDate (DateInfo *pdi, WPARAM wp, LPARAM lp);

void Date_Edit_OnSetFocus (DateInfo *pdi, HWND hEdit);
void Date_Edit_OnUpdate (DateInfo *pdi, HWND hEdit);
void Date_Edit_SetText (DateInfo *pdi, HWND hEdit);


BOOL RegisterDateClass (void)
{
   static BOOL fRegistered = FALSE;

   if (!fRegistered)
      {
      InitializeCriticalSection (&csDate);

      WNDCLASS wc;
      memset (&wc, 0x00, sizeof(wc));
      wc.style = CS_GLOBALCLASS;
      wc.lpfnWndProc = (WNDPROC)DateProc;
      wc.hInstance = THIS_HINST;
      wc.hCursor = LoadCursor (NULL, MAKEINTRESOURCE (IDC_ARROW));
      wc.hbrBackground = CreateSolidBrush (GetSysColor (COLOR_BTNFACE));
      wc.lpszClassName = cszDATECLASS;

      if (RegisterClass (&wc))
         fRegistered = TRUE;
      }

   return fRegistered;
}


void Date_SendCallback (DateInfo *pdi, WORD eln, LPARAM lp)
{
   if (!pdi->fCallingBack)
      {
      pdi->fCallingBack = TRUE;

      SendMessage (GetParent (pdi->hDate),
                   WM_COMMAND,
                   MAKELONG ((WORD)GetWindowLong (pdi->hDate, GWL_ID), eln),
                   lp);

      pdi->fCallingBack = FALSE;
      }
}


HRESULT CALLBACK DateProc (HWND hDate, UINT msg, WPARAM wp, LPARAM lp)
{
   DateInfo *pdi = NULL;

   EnterCriticalSection (&csDate);

   if (msg == WM_CREATE)
      {
      size_t iDate;
      for (iDate = 0; iDate < cDate; ++iDate)
         {
         if (aDate[ iDate ].hDate == NULL)
            break;
         }
      if (iDate >= cDate)
         {
         if (!REALLOC (aDate, cDate, 1+iDate, 4))
            return FALSE;
         }

      memset (&aDate[ iDate ], 0x00, sizeof(DateInfo));
      aDate[ iDate ].hDate = hDate;

      pdi = &aDate[ iDate ];
      }
   else
      {
      for (size_t iDate = 0; !pdi && iDate < cDate; ++iDate)
         {
         if (aDate[ iDate ].hDate == hDate)
            pdi = &aDate[ iDate ];
         }
      }

   LeaveCriticalSection (&csDate);

   if (pdi != NULL)
      {
      switch (msg)
         {
         case WM_CREATE:
            Date_OnCreate (pdi);
            break;

         case WM_DESTROY:
            Date_OnDestroy (pdi);
            break;

         case WM_RBUTTONDOWN:
         case WM_LBUTTONDOWN:
            Date_OnButtonDown (pdi, msg, wp, lp);
            break;

         case WM_SETFOCUS:
            PostMessage (GetParent(hDate), WM_NEXTDLGCTL, (WPARAM)pdi->hFirst, TRUE);
            break;

         case WM_ENABLE:
            EnableWindow (pdi->hYear,    IsWindowEnabled (hDate));
            EnableWindow (pdi->hSep1,    IsWindowEnabled (hDate));
            EnableWindow (pdi->hMonth,   IsWindowEnabled (hDate));
            EnableWindow (pdi->hSep2,    IsWindowEnabled (hDate));
            EnableWindow (pdi->hDay,     IsWindowEnabled (hDate));

            RECT rDate;
            GetRectInParent (hDate, &rDate);
            InvalidateRect (GetParent(hDate), &rDate, TRUE);
            UpdateWindow (GetParent(hDate));
            break;

         case WM_SYSCHAR:
         case WM_CHAR:
            switch (wp)
               {
               case VK_UP:
                  PostMessage (GetParent(pdi->hSpinner), WM_VSCROLL, SB_LINEUP, (LPARAM)pdi->hSpinner);
                  break;

               case VK_DOWN:
                  PostMessage (GetParent(pdi->hSpinner), WM_VSCROLL, SB_LINEDOWN, (LPARAM)pdi->hSpinner);
                  break;

               case VK_PRIOR:
                  PostMessage (GetParent(pdi->hSpinner), WM_VSCROLL, SB_PAGEUP, (LPARAM)pdi->hSpinner);
                  break;

               case VK_NEXT:
                  PostMessage (GetParent(pdi->hSpinner), WM_VSCROLL, SB_PAGEDOWN, (LPARAM)pdi->hSpinner);
                  break;

               case VK_HOME:
                  PostMessage (GetParent(pdi->hSpinner), WM_VSCROLL, SB_TOP, (LPARAM)pdi->hSpinner);
                  break;

               case VK_END:
                  PostMessage (GetParent(pdi->hSpinner), WM_VSCROLL, SB_BOTTOM, (LPARAM)pdi->hSpinner);
                  break;
               }
            break;

         case DM_GETDATE:
            return Date_OnGetDate (pdi, wp, lp);

         case DM_SETDATE:
            return Date_OnSetDate (pdi, wp, lp);
         }
      }

   return DefWindowProc (hDate, msg, wp, lp);
}


void Date_OnCreate (DateInfo *pdi)
{
   Subclass_AddHook (GetParent(pdi->hDate), DateDlgProc);

   TCHAR szDateSep[ cchRESOURCE ];
   if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_SDATE, szDateSep, cchRESOURCE))
      lstrcpy (szDateSep, TEXT("/"));

   TCHAR szDateFormat[ cchRESOURCE ];
   if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_IDATE, szDateFormat, cchRESOURCE))
      pdi->dwFormat = 0; // 0=M/D/Y, 1=D/M/Y, 2=Y/M/D
   else
      pdi->dwFormat = atoi(szDateFormat);

   RECT rDate;
   GetClientRect (pdi->hDate, &rDate);

   POINT ptDate = {0,0};
   ClientToScreen (pdi->hDate, &ptDate);
   ScreenToClient (GetParent (pdi->hDate), &ptDate);

   SIZE s88; // size of widest likely double-digit number
   SIZE sDateSep; // size of ":"

   HDC hdc = GetDC (GetParent (pdi->hDate));
   GetTextExtentPoint (hdc, TEXT("88"), lstrlen(TEXT("88")), &s88);
   GetTextExtentPoint (hdc, szDateSep, lstrlen(szDateSep), &sDateSep);
   sDateSep.cx ++; // we have to disable this; that draws slightly wider
   ReleaseDC (GetParent (pdi->hDate), hdc);

   LONG cxNumbers = cxRECT(rDate) - GetSystemMetrics(SM_CXVSCROLL) - (sDateSep.cx)*2;
   LONG cxYear    = max( cxNumbers/2, s88.cx*2 );
   LONG cxMonth   = max( cxNumbers/4, s88.cx );
   LONG cxDay     = max( cxNumbers/4, s88.cx );

   cxYear    = min (cxYear,  (LONG)( 1.2 * s88.cx * 2 ));
   cxMonth   = min (cxMonth, (LONG)( 1.2 * s88.cx ));
   cxDay     = min (cxDay,   (LONG)( 1.2 * s88.cx ));  // don't be TOO wide.

   pdi->idYear  = 100+NextControlID (GetParent (pdi->hDate));
   pdi->idMonth = pdi->idYear +1;
   pdi->idDay   = pdi->idYear +2;

   LONG xx = ptDate.x;
   LONG yy = ptDate.y;
   LONG cy = cyRECT(rDate);

   LONG cx;
   int id;
   HWND hWnd;
   int iiFirst;

   // 0=M/D/Y, 1=D/M/Y, 2=Y/M/D --so, create edit box: 0=M,1=D,2=Y
   //
   switch (pdi->dwFormat)
      {
      case 0:  cx = cxMonth;  id = pdi->idMonth;  break;
      case 1:  cx = cxDay;    id = pdi->idDay;    break;
      case 2:  cx = cxYear;   id = pdi->idYear;   break;
      }

   hWnd = CreateWindow (
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_TABSTOP | ES_RIGHT | ES_NUMBER | ES_MULTILINE,
                xx, yy, cx, cy,
                GetParent(pdi->hDate),
                (HMENU)(INT_PTR)id,
                THIS_HINST,
                0);
   xx += cx;

   switch (pdi->dwFormat)
      {
      case 0:  pdi->hMonth = hWnd; iiFirst = pdi->dateNow.wMonth; break;
      case 1:  pdi->hDay   = hWnd; iiFirst = pdi->dateNow.wDay;   break;
      case 2:  pdi->hYear  = hWnd; iiFirst = pdi->dateNow.wYear;  break;
      }
   pdi->hFirst = hWnd;

   // Create a separator
   //
   pdi->hSep1 = CreateWindow (
                TEXT("STATIC"),
                szDateSep,
                WS_CHILD | SS_CENTER,
                xx, yy, sDateSep.cx, cy,
                GetParent(pdi->hDate),
                (HMENU)-1,
                THIS_HINST,
                0);
   xx += sDateSep.cx;

   // 0=M/D/Y, 1=D/M/Y, 2=Y/M/D --so, create edit box: 0=D,1=M,2=M
   //
   switch (pdi->dwFormat)
      {
      case 0:  cx = cxDay;    id = pdi->idDay;    break;
      case 1:  cx = cxMonth;  id = pdi->idMonth;  break;
      case 2:  cx = cxMonth;  id = pdi->idMonth;  break;
      }

   hWnd = CreateWindow (
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_TABSTOP | ES_RIGHT | ES_NUMBER | ES_MULTILINE,
                xx, yy, cx, cy,
                GetParent(pdi->hDate),
                (HMENU)(INT_PTR)id,
                THIS_HINST,
                0);
   xx += cx;

   switch (pdi->dwFormat)
      {
      case 0:  pdi->hDay    = hWnd;  break;
      case 1:  pdi->hMonth  = hWnd;  break;
      case 2:  pdi->hMonth  = hWnd;  break;
      }

   // Create a separator
   //
   pdi->hSep2 = CreateWindow (
                TEXT("STATIC"),
                szDateSep,
                WS_CHILD | SS_CENTER,
                xx, yy, sDateSep.cx, cy,
                GetParent(pdi->hDate),
                (HMENU)-1,
                THIS_HINST,
                0);
   xx += sDateSep.cx;

   // 0=M/D/Y, 1=D/M/Y, 2=Y/M/D --so, create edit box: 0=Y,1=Y,2=D
   //
   switch (pdi->dwFormat)
      {
      case 0:  cx = cxYear;   id = pdi->idYear;   break;
      case 1:  cx = cxYear;   id = pdi->idYear;   break;
      case 2:  cx = cxDay;    id = pdi->idDay;    break;
      }

   hWnd = CreateWindow (
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_TABSTOP | ES_RIGHT | ES_NUMBER | ES_MULTILINE,
                xx, yy, cx, cy,
                GetParent(pdi->hDate),
                (HMENU)(INT_PTR)id,
                THIS_HINST,
				0);
   xx += cx;

   switch (pdi->dwFormat)
      {
      case 0:  pdi->hYear   = hWnd;  break;
      case 1:  pdi->hYear   = hWnd;  break;
      case 2:  pdi->hDay    = hWnd;  break;
      }

   // Subclass the edit controls
   //
   Subclass_AddHook (pdi->hYear,  DateEditProc);
   Subclass_AddHook (pdi->hMonth, DateEditProc);
   Subclass_AddHook (pdi->hDay,   DateEditProc);

   HFONT hf = (HFONT)SendMessage (GetParent (pdi->hDate), WM_GETFONT, 0, 0);
   SendMessage (pdi->hYear,   WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (pdi->hSep1,   WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (pdi->hMonth,  WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (pdi->hSep2,   WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (pdi->hDay,    WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));

   EnableWindow (pdi->hYear,    IsWindowEnabled (pdi->hDate));
   EnableWindow (pdi->hSep1,    IsWindowEnabled (pdi->hDate));
   EnableWindow (pdi->hMonth,   IsWindowEnabled (pdi->hDate));
   EnableWindow (pdi->hSep2,    IsWindowEnabled (pdi->hDate));
   EnableWindow (pdi->hDay,     IsWindowEnabled (pdi->hDate));

   ShowWindow (pdi->hYear,    TRUE);
   ShowWindow (pdi->hSep1,    TRUE);
   ShowWindow (pdi->hMonth,   TRUE);
   ShowWindow (pdi->hSep2,    TRUE);
   ShowWindow (pdi->hDay,     TRUE);

   RECT rWindow;
   GetWindowRect (pdi->hDate, &rWindow);
   rWindow.right += (cxYear + cxMonth + cxDay + sDateSep.cx*2) - cxRECT(rDate);

   SetWindowPos (pdi->hDate, NULL, 0, 0, cxRECT(rWindow), cyRECT(rWindow),
                 SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);

   RECT rSpinner;
   GetRectInParent (pdi->hDate, &rSpinner);
   rSpinner.left = rSpinner.right;
   rSpinner.right = rSpinner.left + GetSystemMetrics (SM_CXVSCROLL);
   rSpinner.bottom -= 2; // just like Win95 does
   CreateSpinner (pdi->hFirst, 10, FALSE, 0, iiFirst, 3000, &rSpinner);
   pdi->hSpinner = SP_GetSpinner (pdi->hFirst);
   pdi->hSpinnerBuddy = pdi->hFirst;

   Date_Edit_SetText (pdi, pdi->hYear);
   Date_Edit_SetText (pdi, pdi->hMonth);
   Date_Edit_SetText (pdi, pdi->hDay);
   Date_Edit_OnSetFocus (pdi, pdi->hFirst);
}


void Date_OnDestroy (DateInfo *pdi)
{
   Subclass_RemoveHook (GetParent (pdi->hDate), DateDlgProc);
   pdi->hDate = NULL;
}


void Date_OnButtonDown (DateInfo *pdi, UINT msg, WPARAM wp, LPARAM lp)
{
   DWORD dw = GetMessagePos();
   POINT pt;
   pt.x = LOWORD(dw);
   pt.y = HIWORD(dw);  // in screen coordinates

   RECT rTarget;
   HWND hTarget = 0;

   GetWindowRect (pdi->hYear, &rTarget);
   if (PtInRect (&rTarget, pt))
      hTarget = pdi->hYear;

   GetWindowRect (pdi->hMonth, &rTarget);
   if (PtInRect (&rTarget, pt))
      hTarget = pdi->hMonth;

   GetWindowRect (pdi->hDay, &rTarget);
   if (PtInRect (&rTarget, pt))
      hTarget = pdi->hDay;

   if (hTarget != 0)
      {
      PostMessage (hTarget, msg, wp, lp);
      }
}


BOOL Date_OnGetDate (DateInfo *pdi, WPARAM wp, LPARAM lp)
{
   SYSTEMTIME *pdate = (SYSTEMTIME*)lp;
   pdate->wYear = pdi->dateNow.wYear;
   pdate->wMonth = pdi->dateNow.wMonth;
   pdate->wDayOfWeek = pdi->dateNow.wDayOfWeek;
   pdate->wDay = pdi->dateNow.wDay;
   return TRUE;
}


BOOL Date_OnSetDate (DateInfo *pdi, WPARAM wp, LPARAM lp)
{
   SYSTEMTIME *pdate = (SYSTEMTIME*)lp;
   pdi->dateNow = *pdate;
   Date_Edit_SetText (pdi, pdi->hYear);
   Date_Edit_SetText (pdi, pdi->hMonth);
   Date_Edit_SetText (pdi, pdi->hDay);
   return TRUE;
}


HRESULT CALLBACK DateDlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hDlg, DateDlgProc);
   size_t iDate;

   switch (msg)
      {
      case WM_CTLCOLOREDIT:
      case WM_CTLCOLORSTATIC:
         for (iDate = 0; iDate < cDate; ++iDate)
            {
            if (aDate[ iDate ].hDate == NULL)
               continue;
            if ( (aDate[ iDate ].hYear  == (HWND)lp) ||
                 (aDate[ iDate ].hSep1  == (HWND)lp) ||
                 (aDate[ iDate ].hMonth == (HWND)lp) ||
                 (aDate[ iDate ].hSep2  == (HWND)lp) ||
                 (aDate[ iDate ].hDay   == (HWND)lp) )
               {
               COLORREF clr;
               if (IsWindowEnabled (aDate[ iDate ].hDate))
                  clr = GetSysColor (COLOR_WINDOW);
               else
                  clr = GetSysColor (COLOR_BTNFACE);
               SetBkColor ((HDC)wp, clr);
               return (BOOL)(INT_PTR)CreateSolidBrush (clr);
               }
            }
         break;

      case WM_COMMAND:
         for (iDate = 0; iDate < cDate; ++iDate)
            {
            if (aDate[ iDate ].hDate == NULL)
               continue;
            if ( (aDate[ iDate ].idYear   == LOWORD(wp)) ||
                 (aDate[ iDate ].idMonth  == LOWORD(wp)) ||
                 (aDate[ iDate ].idDay    == LOWORD(wp)) )
               {
               if (HIWORD(wp) == SPN_UPDATE)
                  {
                  Date_Edit_OnUpdate (&aDate[ iDate ], GetDlgItem(hDlg,LOWORD(wp)));
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


HRESULT CALLBACK DateEditProc (HWND hEdit, UINT msg, WPARAM wp, LPARAM lp)
{
   DateInfo *pdi = NULL;

   EnterCriticalSection (&csDate);

   for (size_t iDate = 0; !pdi && iDate < cDate; ++iDate)
      {
      if ( (aDate[ iDate ].hYear  == hEdit) ||
           (aDate[ iDate ].hMonth == hEdit) ||
           (aDate[ iDate ].hDay   == hEdit) )
         {
         pdi = &aDate[ iDate ];
         }
      }

   LeaveCriticalSection (&csDate);

   if (pdi)
      {
      switch (msg)
         {
         case WM_SETFOCUS:
            Date_Edit_OnSetFocus (pdi, hEdit);
            break;
         }
      }

   PVOID oldProc = Subclass_FindNextHook (hEdit, DateEditProc);
   if (oldProc)
      return CallWindowProc ((WNDPROC)oldProc, hEdit, msg, wp, lp);
   else
      return DefWindowProc (hEdit, msg, wp, lp);
}


void Date_Edit_OnSetFocus (DateInfo *pdi, HWND hEdit)
{
   DWORD dwMin;
   DWORD dwNow;
   DWORD dwMax;

   if (hEdit == pdi->hYear)
      {
      dwMin = 1970;
      dwNow = pdi->dateNow.wYear;
      dwMax = 2999;
      }
   else if (hEdit == pdi->hMonth)
      {
      dwMin = 1;
      dwNow = pdi->dateNow.wMonth;
      dwMax = 12;
      }
   else // (hEdit == pdi->hDay)
      {
      dwMin = 1;
      dwNow = pdi->dateNow.wDay;
      dwMax = 31;
      }

   if (pdi->hSpinnerBuddy != hEdit)
      {
      SP_SetBuddy (pdi->hSpinnerBuddy, hEdit, FALSE);
      pdi->hSpinnerBuddy = hEdit;
      }

   SP_SetRange (hEdit, dwMin, dwMax);
   SP_SetPos (hEdit, dwNow);

   SendMessage (hEdit, EM_SETSEL, (WPARAM)0, (LPARAM)-1);  // select all
}


void Date_Edit_OnUpdate (DateInfo *pdi, HWND hEdit)
{
   TCHAR szText[ cchRESOURCE ];

   if (hEdit == pdi->hYear)
      {
      GetWindowText (pdi->hYear, szText, 256);
      pdi->dateNow.wYear = (WORD)atol (szText);
      }

   if (hEdit == pdi->hMonth)
      {
      GetWindowText (pdi->hMonth, szText, 256);
      pdi->dateNow.wMonth = (WORD)atol (szText);
      }

   if (hEdit == pdi->hDay)
      {
      GetWindowText (pdi->hDay, szText, 256);
      pdi->dateNow.wDay = (WORD)atol (szText);
      }

   SYSTEMTIME st = pdi->dateNow;
   Date_SendCallback (pdi, DN_UPDATE, (LPARAM)&st);
}


void Date_Edit_SetText (DateInfo *pdi, HWND hEdit)
{
   DWORD dwNow;

   if (hEdit == pdi->hYear)
      dwNow = pdi->dateNow.wYear;
   else if (hEdit == pdi->hMonth)
      dwNow = pdi->dateNow.wMonth;
   else if (hEdit == pdi->hDay)
      dwNow = pdi->dateNow.wDay;

   if (pdi->hSpinnerBuddy == hEdit)
      {
      SP_SetPos (hEdit, dwNow);
      }
   else
      {
      TCHAR szText[ cchRESOURCE ];
      wsprintf (szText, TEXT("%lu"), dwNow);
      SetWindowText (hEdit, szText);
      }
}

