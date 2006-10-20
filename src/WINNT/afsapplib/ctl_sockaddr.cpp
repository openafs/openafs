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
#include <winsock2.h>
#include <stdlib.h>
#include <WINNT/dialog.h>
#include <WINNT/resize.h>	// for GetRectInParent()
#include <WINNT/subclass.h>
#include <WINNT/ctl_spinner.h>
#include <WINNT/ctl_sockaddr.h>
#include <WINNT/TaLocale.h>

#ifndef cchRESOURCE
#define cchRESOURCE 256
#endif


/*
 * MISCELLANEOUS ______________________________________________________________
 *
 */

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) SockAddrReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
BOOL SockAddrReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
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
 * SOCKADDRS __________________________________________________________________
 *
 */

typedef struct
   {
   HWND hSockAddr;
   HWND hE1;
   HWND hSep1;
   HWND hE2;
   HWND hSep2;
   HWND hE3;
   HWND hSep3;
   HWND hE4;
   HWND hMinutes;

   WORD idE1;
   WORD idE2;
   WORD idE3;
   WORD idE4;

   SOCKADDR_IN addr;

   BOOL  fCallingBack;
   } SockAddrInfo;

static CRITICAL_SECTION  csSockAddr;
static SockAddrInfo     *aSockAddr = NULL;
static size_t            cSockAddr = 0;

#define cszSOCKADDRCLASS TEXT("SockAddr")

BOOL CALLBACK SockAddrProc (HWND hSockAddr, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK SockAddrEditProc (HWND hEdit, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK SockAddrDlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void SockAddr_SendCallback (SockAddrInfo *psai, WORD eln, LPARAM lp);

void SockAddr_OnCreate (SockAddrInfo *psai);
void SockAddr_OnDestroy (SockAddrInfo *psai);
void SockAddr_OnButtonDown (SockAddrInfo *psai, UINT msg, WPARAM wp, LPARAM lp);
BOOL SockAddr_OnGetAddr (SockAddrInfo *psai, WPARAM wp, LPARAM lp);
BOOL SockAddr_OnSetAddr (SockAddrInfo *psai, WPARAM wp, LPARAM lp);

void SockAddr_Edit_OnChange (SockAddrInfo *psai, HWND hEdit);
void SockAddr_Edit_OnUpdate (SockAddrInfo *psai, HWND hEdit);
void SockAddr_Edit_SetText (SockAddrInfo *psai, HWND hEdit);

#define cszSOCKSEP TEXT(".")

BOOL RegisterSockAddrClass (void)
{
   static BOOL fRegistered = FALSE;

   if (!fRegistered)
      {
      InitializeCriticalSection (&csSockAddr);

      WNDCLASS wc;
      memset (&wc, 0x00, sizeof(wc));
      wc.style = CS_GLOBALCLASS;
      wc.lpfnWndProc = (WNDPROC)SockAddrProc;
      wc.hInstance = THIS_HINST;
      wc.hCursor = LoadCursor (NULL, MAKEINTRESOURCE (IDC_ARROW));
      wc.hbrBackground = CreateSolidBrush (GetSysColor (COLOR_BTNFACE));
      wc.lpszClassName = cszSOCKADDRCLASS;

      if (RegisterClass (&wc))
         fRegistered = TRUE;
      }

   return fRegistered;
}


void SockAddr_SendCallback (SockAddrInfo *psai, WORD san, LPARAM lp)
{
   if (!psai->fCallingBack)
      {
      psai->fCallingBack = TRUE;

      SendMessage (GetParent (psai->hSockAddr),
                   WM_COMMAND,
                   MAKELONG ((WORD)GetWindowLong (psai->hSockAddr, GWL_ID), san),
                   lp);

      psai->fCallingBack = FALSE;
      }
}


BOOL CALLBACK SockAddrProc (HWND hSockAddr, UINT msg, WPARAM wp, LPARAM lp)
{
   SockAddrInfo *psai = NULL;

   EnterCriticalSection (&csSockAddr);

   if (msg == WM_CREATE)
      {
      size_t iSockAddr;
      for (iSockAddr = 0; iSockAddr < cSockAddr; ++iSockAddr)
         {
         if (aSockAddr[ iSockAddr ].hSockAddr == NULL)
            break;
         }
      if (iSockAddr >= cSockAddr)
         {
         if (!REALLOC (aSockAddr, cSockAddr, 1+iSockAddr, 4))
            return FALSE;
         }

      memset (&aSockAddr[ iSockAddr ], 0x00, sizeof(SockAddrInfo));
      aSockAddr[ iSockAddr ].hSockAddr = hSockAddr;

      psai = &aSockAddr[ iSockAddr ];
      }
   else
      {
      for (size_t iSockAddr = 0; !psai && iSockAddr < cSockAddr; ++iSockAddr)
         {
         if (aSockAddr[ iSockAddr ].hSockAddr == hSockAddr)
            psai = &aSockAddr[ iSockAddr ];
         }
      }

   LeaveCriticalSection (&csSockAddr);

   if (psai != NULL)
      {
      switch (msg)
         {
         case WM_CREATE:
            SockAddr_OnCreate (psai);
            break;

         case WM_DESTROY:
            SockAddr_OnDestroy (psai);
            break;

         case WM_RBUTTONDOWN:
         case WM_LBUTTONDOWN:
            SockAddr_OnButtonDown (psai, msg, wp, lp);
            break;

         case WM_SETFOCUS:
            PostMessage (GetParent(hSockAddr), WM_NEXTDLGCTL, (WPARAM)psai->hE1, TRUE);
            break;

         case WM_ENABLE:
            EnableWindow (psai->hE1,   IsWindowEnabled (hSockAddr));
            EnableWindow (psai->hSep1, IsWindowEnabled (hSockAddr));
            EnableWindow (psai->hE2,   IsWindowEnabled (hSockAddr));
            EnableWindow (psai->hSep2, IsWindowEnabled (hSockAddr));
            EnableWindow (psai->hE3,   IsWindowEnabled (hSockAddr));
            EnableWindow (psai->hSep3, IsWindowEnabled (hSockAddr));
            EnableWindow (psai->hE4,   IsWindowEnabled (hSockAddr));

            RECT rSockAddr;
            GetRectInParent (hSockAddr, &rSockAddr);
            InvalidateRect (GetParent(hSockAddr), &rSockAddr, TRUE);
            UpdateWindow (GetParent(hSockAddr));
            break;

         case SAM_GETADDR:
            return SockAddr_OnGetAddr (psai, wp, lp);

         case SAM_SETADDR:
            return SockAddr_OnSetAddr (psai, wp, lp);
         }
      }

   return DefWindowProc (hSockAddr, msg, wp, lp);
}


void SockAddr_OnCreate (SockAddrInfo *psai)
{
   Subclass_AddHook (GetParent(psai->hSockAddr), SockAddrDlgProc);

   RECT rSockAddr;
   GetClientRect (psai->hSockAddr, &rSockAddr);

   POINT ptSockAddr = {0,0};
   ClientToScreen (psai->hSockAddr, &ptSockAddr);
   ScreenToClient (GetParent (psai->hSockAddr), &ptSockAddr);

   SIZE s888; // size of widest likely triple-digit number
   SIZE sSockSep; // size of "."

   HDC hdc = GetDC (GetParent (psai->hSockAddr));
   GetTextExtentPoint (hdc, TEXT("888"), lstrlen(TEXT("888")), &s888);
   GetTextExtentPoint (hdc, cszSOCKSEP, lstrlen(cszSOCKSEP), &sSockSep);
   ReleaseDC (GetParent (psai->hSockAddr), hdc);

   LONG cxNumbers = cxRECT(rSockAddr) - (3 * sSockSep.cx);
   LONG cxE2 = cxNumbers/4;
   LONG cxE1 = cxNumbers - cxE2 * 3;
   LONG yy = ptSockAddr.y;
   LONG cy = cyRECT(rSockAddr);

   cxE2 = min (cxE2, (LONG)( 1.4 * s888.cx ));
   cxE1 = min (cxE1, (LONG)( 1.4 * s888.cx ));  // don't be TOO wide.

   psai->idE1 = 100+NextControlID (GetParent (psai->hSockAddr));
   psai->hE1 = CreateWindow (
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_TABSTOP | ES_RIGHT | ES_NUMBER | ES_MULTILINE,
                ptSockAddr.x, yy, cxE1, cy,
                GetParent(psai->hSockAddr),
                (HMENU)psai->idE1,
                THIS_HINST,
                0);

   psai->hSep1 = CreateWindow (
                TEXT("STATIC"),
                cszSOCKSEP,
                WS_CHILD | SS_CENTER,
                ptSockAddr.x + cxE1, yy, sSockSep.cx, cy,
                GetParent(psai->hSockAddr),
                (HMENU)-1,
                THIS_HINST,
                0);

   psai->idE2 = 1 + psai->idE1;
   psai->hE2 = CreateWindow (
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_TABSTOP | ES_RIGHT | ES_NUMBER | ES_MULTILINE,
                ptSockAddr.x + cxE1 + sSockSep.cx, yy, cxE2, cy,
                GetParent(psai->hSockAddr),
                (HMENU)psai->idE2,
                THIS_HINST,
                0);

   psai->hSep2 = CreateWindow (
                TEXT("STATIC"),
                cszSOCKSEP,
                WS_CHILD | SS_CENTER,
                ptSockAddr.x + cxE1 + sSockSep.cx + cxE2, yy, sSockSep.cx, cy,
                GetParent(psai->hSockAddr),
                (HMENU)-1,
                THIS_HINST,
                0);

   psai->idE3 = 1 + psai->idE2;
   psai->hE3 = CreateWindow (
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_TABSTOP | ES_RIGHT | ES_NUMBER | ES_MULTILINE,
                ptSockAddr.x + cxE1 + sSockSep.cx*2 + cxE2, yy, cxE2, cy,
                GetParent(psai->hSockAddr),
                (HMENU)psai->idE3,
                THIS_HINST,
                0);

   psai->hSep3 = CreateWindow (
                TEXT("STATIC"),
                cszSOCKSEP,
                WS_CHILD | SS_CENTER,
                ptSockAddr.x + cxE1 + sSockSep.cx*2 + cxE2*2, yy, sSockSep.cx, cy,
                GetParent(psai->hSockAddr),
                (HMENU)-1,
                THIS_HINST,
                0);

   psai->idE4 = 1 + psai->idE3;
   psai->hE4 = CreateWindow (
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_TABSTOP | ES_RIGHT | ES_NUMBER | ES_MULTILINE,
                ptSockAddr.x + cxE1 + sSockSep.cx*3 + cxE2*2, yy, cxE2, cy,
                GetParent(psai->hSockAddr),
                (HMENU)psai->idE4,
                THIS_HINST,
                0);

   Subclass_AddHook (psai->hE1, SockAddrEditProc);
   Subclass_AddHook (psai->hE2, SockAddrEditProc);
   Subclass_AddHook (psai->hE3, SockAddrEditProc);
   Subclass_AddHook (psai->hE4, SockAddrEditProc);

   HFONT hf = (HFONT)SendMessage (GetParent (psai->hSockAddr), WM_GETFONT, 0, 0);
   SendMessage (psai->hE1,   WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (psai->hSep1, WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (psai->hE2,   WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (psai->hSep2, WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (psai->hE3,   WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (psai->hSep3, WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));
   SendMessage (psai->hE4,   WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));

   EnableWindow (psai->hE1,   IsWindowEnabled (psai->hSockAddr));
   EnableWindow (psai->hSep1, IsWindowEnabled (psai->hSockAddr));
   EnableWindow (psai->hE2,   IsWindowEnabled (psai->hSockAddr));
   EnableWindow (psai->hSep2, IsWindowEnabled (psai->hSockAddr));
   EnableWindow (psai->hE3,   IsWindowEnabled (psai->hSockAddr));
   EnableWindow (psai->hSep3, IsWindowEnabled (psai->hSockAddr));
   EnableWindow (psai->hE4,   IsWindowEnabled (psai->hSockAddr));

   ShowWindow (psai->hE1,   TRUE);
   ShowWindow (psai->hSep1, TRUE);
   ShowWindow (psai->hE2,   TRUE);
   ShowWindow (psai->hSep2, TRUE);
   ShowWindow (psai->hE3,   TRUE);
   ShowWindow (psai->hSep3, TRUE);
   ShowWindow (psai->hE4,   TRUE);

   RECT rWindow;
   GetWindowRect (psai->hSockAddr, &rWindow);
   rWindow.right += (cxE1 + cxE2*3 + sSockSep.cx*3) - cxRECT(rSockAddr);

   SetWindowPos (psai->hSockAddr, NULL, 0, 0, cxRECT(rWindow), cyRECT(rWindow),
                 SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);

   SOCKADDR_IN addrNew;
   memset (&addrNew, 0x00, sizeof(addrNew));
   SockAddr_OnSetAddr (psai, 0, (LPARAM)&addrNew);
}


void SockAddr_OnDestroy (SockAddrInfo *psai)
{
   Subclass_RemoveHook (GetParent(psai->hSockAddr), SockAddrDlgProc);
   psai->hSockAddr = NULL;
}


void SockAddr_OnButtonDown (SockAddrInfo *psai, UINT msg, WPARAM wp, LPARAM lp)
{
   DWORD dw = GetMessagePos();
   POINT pt;
   pt.x = LOWORD(dw);
   pt.y = HIWORD(dw);  // in screen coordinates

   RECT rTarget;
   HWND hTarget = 0;

   GetWindowRect (psai->hE1, &rTarget);
   if (PtInRect (&rTarget, pt))
      hTarget = psai->hE1;

   GetWindowRect (psai->hE2, &rTarget);
   if (PtInRect (&rTarget, pt))
      hTarget = psai->hE2;

   GetWindowRect (psai->hE3, &rTarget);
   if (PtInRect (&rTarget, pt))
      hTarget = psai->hE3;

   GetWindowRect (psai->hE4, &rTarget);
   if (PtInRect (&rTarget, pt))
      hTarget = psai->hE4;

   if (hTarget != 0)
      {
      PostMessage (hTarget, msg, wp, lp);
      }
}


BOOL SockAddr_OnGetAddr (SockAddrInfo *psai, WPARAM wp, LPARAM lp)
{
   SOCKADDR_IN *pAddr = (SOCKADDR_IN*)lp;
   *pAddr = psai->addr;
   return TRUE;
}


BOOL SockAddr_OnSetAddr (SockAddrInfo *psai, WPARAM wp, LPARAM lp)
{
   SOCKADDR_IN *pAddr = (SOCKADDR_IN*)lp;
   psai->addr = *pAddr;
   SockAddr_Edit_SetText (psai, psai->hE1);
   SockAddr_Edit_SetText (psai, psai->hE2);
   SockAddr_Edit_SetText (psai, psai->hE3);
   SockAddr_Edit_SetText (psai, psai->hE4);
   return TRUE;
}


BOOL CALLBACK SockAddrDlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hDlg, SockAddrDlgProc);
   size_t iSockAddr;

   switch (msg)
      {
      case WM_CTLCOLOREDIT:
      case WM_CTLCOLORSTATIC:
         for (iSockAddr = 0; iSockAddr < cSockAddr; ++iSockAddr)
            {
            if (aSockAddr[ iSockAddr ].hSockAddr == NULL)
               continue;
            if ( (aSockAddr[ iSockAddr ].hE1   == (HWND)lp) ||
                 (aSockAddr[ iSockAddr ].hSep1 == (HWND)lp) ||
                 (aSockAddr[ iSockAddr ].hE2   == (HWND)lp) ||
                 (aSockAddr[ iSockAddr ].hSep2 == (HWND)lp) ||
                 (aSockAddr[ iSockAddr ].hE3   == (HWND)lp) ||
                 (aSockAddr[ iSockAddr ].hSep3 == (HWND)lp) ||
                 (aSockAddr[ iSockAddr ].hE4   == (HWND)lp) )
               {
               COLORREF clr;
               if (IsWindowEnabled (aSockAddr[ iSockAddr ].hSockAddr))
                  clr = GetSysColor (COLOR_WINDOW);
               else
                  clr = GetSysColor (COLOR_BTNFACE);
               SetBkColor ((HDC)wp, clr);
               return (BOOL)CreateSolidBrush (clr);
               }
            }
         break;

      case WM_COMMAND:
         for (iSockAddr = 0; iSockAddr < cSockAddr; ++iSockAddr)
            {
            if (aSockAddr[ iSockAddr ].hSockAddr == NULL)
               continue;
            if ( (aSockAddr[ iSockAddr ].idE1 == LOWORD(wp)) ||
                 (aSockAddr[ iSockAddr ].idE2 == LOWORD(wp)) ||
                 (aSockAddr[ iSockAddr ].idE3 == LOWORD(wp)) ||
                 (aSockAddr[ iSockAddr ].idE4 == LOWORD(wp)) )
               {
               static BOOL fInHere = FALSE;
               if (!fInHere)
                  {
                  fInHere = TRUE;

                  if (HIWORD(wp) == EN_CHANGE)
                     {
                     SockAddr_Edit_OnChange (&aSockAddr[ iSockAddr ], GetDlgItem (hDlg, LOWORD(wp)));
                     }
                  else if (HIWORD(wp) == EN_UPDATE)
                     {
                     SockAddr_Edit_OnUpdate (&aSockAddr[ iSockAddr ], GetDlgItem (hDlg, LOWORD(wp)));
                     }

                  fInHere = FALSE;
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


BOOL CALLBACK SockAddrEditProc (HWND hEdit, UINT msg, WPARAM wp, LPARAM lp)
{
   SockAddrInfo *psai = NULL;

   EnterCriticalSection (&csSockAddr);

   for (size_t iSockAddr = 0; !psai && iSockAddr < cSockAddr; ++iSockAddr)
      {
      if ( (aSockAddr[ iSockAddr ].hE1 == hEdit) ||
           (aSockAddr[ iSockAddr ].hE2 == hEdit) ||
           (aSockAddr[ iSockAddr ].hE3 == hEdit) ||
           (aSockAddr[ iSockAddr ].hE4 == hEdit) )
         {
         psai = &aSockAddr[ iSockAddr ];
         }
      }

   LeaveCriticalSection (&csSockAddr);

   if (psai)
      {
      switch (msg)
         {
         case WM_KILLFOCUS:
            SockAddr_Edit_SetText (psai, hEdit);
            break;

         case WM_CHAR:
            if (wp == TEXT('.'))
               {
               // advance to the next field
               PostMessage (GetParent(hEdit), WM_NEXTDLGCTL, 0, 0);
               return FALSE;
               }
            break;
         }
      }

   PVOID oldProc = Subclass_FindNextHook (hEdit, SockAddrEditProc);
   if (oldProc)
      return CallWindowProc ((WNDPROC)oldProc, hEdit, msg, wp, lp);
   else
      return DefWindowProc (hEdit, msg, wp, lp);
}


void SockAddr_Edit_OnChange (SockAddrInfo *psai, HWND hEdit)
{
   TCHAR szText[ cchRESOURCE ];
   GetWindowText (hEdit, szText, cchRESOURCE);

   if ( (hEdit != psai->hE1) ||
        (psai->addr.sin_addr.s_net) ||
        (psai->addr.sin_addr.s_host) ||
        (psai->addr.sin_addr.s_lh) ||
        (psai->addr.sin_addr.s_impno) ||
        (szText[0] != TEXT('\0')) )
      {
      DWORD dwMin = (hEdit == psai->hE1) ? 1 : 0;
      DWORD dwNow = (DWORD)atol(szText);
      DWORD dwMax = (hEdit == psai->hE1) ? 253 : 255;

      dwNow = limit (dwMin, dwNow, dwMax);

      if (hEdit == psai->hE1)
         psai->addr.sin_addr.s_net = (unsigned char)dwNow;
      else if (hEdit == psai->hE2)
         psai->addr.sin_addr.s_host = (unsigned char)dwNow;
      else if (hEdit == psai->hE3)
         psai->addr.sin_addr.s_lh = (unsigned char)dwNow;
      else if (hEdit == psai->hE4)
         psai->addr.sin_addr.s_impno = (unsigned char)dwNow;

      SOCKADDR_IN addrNew = psai->addr;
      SockAddr_SendCallback (psai, SAN_CHANGE, (LPARAM)&addrNew);
      if (memcmp (&addrNew, &psai->addr, sizeof(SOCKADDR_IN)))
         {
         SockAddr_OnSetAddr (psai, (WPARAM)0, (LPARAM)&addrNew);
         }
      }

// SockAddr_Edit_SetText (psai, hEdit);

   if ( (hEdit == psai->hE1) &&
        (psai->addr.sin_addr.s_net) &&
        (!psai->addr.sin_addr.s_host) &&
        (!psai->addr.sin_addr.s_lh) &&
        (!psai->addr.sin_addr.s_impno) )
      {
      SockAddr_Edit_SetText (psai, psai->hE2);
      SockAddr_Edit_SetText (psai, psai->hE3);
      SockAddr_Edit_SetText (psai, psai->hE4);
      }
}


void SockAddr_Edit_OnUpdate (SockAddrInfo *psai, HWND hEdit)
{
   SOCKADDR_IN addrNew = psai->addr;
   SockAddr_SendCallback (psai, SAN_UPDATE, (LPARAM)&addrNew);
}


void SockAddr_Edit_SetText (SockAddrInfo *psai, HWND hEdit)
{
   DWORD dwNow;

   if ( (!psai->addr.sin_addr.s_net) &&
        (!psai->addr.sin_addr.s_host) &&
        (!psai->addr.sin_addr.s_lh) &&
        (!psai->addr.sin_addr.s_impno) )
      {
      SetWindowText (hEdit, TEXT(""));
      }
   else
      {
      if (hEdit == psai->hE1)
         dwNow = (DWORD)psai->addr.sin_addr.s_net;
      else if (hEdit == psai->hE2)
         dwNow = (DWORD)psai->addr.sin_addr.s_host;
      else if (hEdit == psai->hE3)
         dwNow = (DWORD)psai->addr.sin_addr.s_lh;
      else if (hEdit == psai->hE4)
         dwNow = (DWORD)psai->addr.sin_addr.s_impno;

      TCHAR szText[ cchRESOURCE ];
      wsprintf (szText, TEXT("%lu"), dwNow);
      SetWindowText (hEdit, szText);
      }
}

