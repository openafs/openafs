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
#include <WINNT/subclass.h>
#include <WINNT/ctl_spinner.h>
#include <WINNT/TaLocale.h>

#ifndef cchRESOURCE
#define cchRESOURCE 256
#endif


/*
 * MISCELLANEOUS ______________________________________________________________
 *
 */

#define ishexdigit(_ch) ( (((_ch) >= 'a') && ((_ch) <= 'f')) || \
                          (((_ch) >= 'A') && ((_ch) <= 'F')) )

#define digitval(_ch) (isdigit(_ch) \
                       ? ((DWORD)(_ch) - (DWORD)TEXT('0')) \
                       : (((_ch) >= 'a') && ((_ch) <= 'f')) \
                         ? ((DWORD)(_ch) - (DWORD)TEXT('a') + 10) \
                         : (((_ch) >= 'A') && ((_ch) <= 'F')) \
                           ? ((DWORD)(_ch) - (DWORD)TEXT('A') + 10) \
                           : 0)

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) SpinnerReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
BOOL SpinnerReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
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
 * SPINNERS ___________________________________________________________________
 *
 */

typedef struct
   {
   HWND hSpinner;
   HWND hBuddy;
   RECT rReq;

   DWORD dwMin;
   DWORD dwMax;
   WORD  wBase;
   BOOL  fSigned;
   DWORD dwPos;

   BOOL  fNewText;
   BOOL  fCallingBack;
   BOOL  fCanCallBack;
   LPTSTR pszFormat;
   } SpinnerInfo;

static CRITICAL_SECTION csSpinners;
static SpinnerInfo     *aSpinners = NULL;
static size_t           cSpinners = 0;
static LONG             oldSpinnerProc = 0;

#define cszSPINNERCLASS TEXT("Spinner")

BOOL CALLBACK SpinnerProc (HWND hSpin, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK SpinnerDialogProc (HWND hSpin, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK SpinnerBuddyProc (HWND hBuddy, UINT msg, WPARAM wp, LPARAM lp);

void SpinnerSendCallback (SpinnerInfo *psi, WORD spm, LPARAM lp);

void Spinner_OnCreate (SpinnerInfo *psi);
BOOL Spinner_OnGetRange (SpinnerInfo *psi, WPARAM wp, LPARAM lp);
BOOL Spinner_OnSetRange (SpinnerInfo *psi, WPARAM wp, LPARAM lp);
BOOL Spinner_OnGetPos (SpinnerInfo *psi, WPARAM wp, LPARAM lp);
BOOL Spinner_OnSetPos (SpinnerInfo *psi, WPARAM wp, LPARAM lp);
BOOL Spinner_OnGetBase (SpinnerInfo *psi, WPARAM wp, LPARAM lp);
BOOL Spinner_OnSetBase (SpinnerInfo *psi, WPARAM wp, LPARAM lp);
BOOL Spinner_OnReattach (SpinnerInfo *psi, WPARAM wp, LPARAM lp);
BOOL Spinner_OnSetRect (SpinnerInfo *psi, WPARAM wp, LPARAM lp);
BOOL Spinner_OnGetSpinner (SpinnerInfo *psi, WPARAM wp, LPARAM lp);
BOOL Spinner_OnSetFormat (SpinnerInfo *psi, WPARAM wp, LPARAM lp);
BOOL Spinner_OnSetBuddy (SpinnerInfo *psi, WPARAM wp, LPARAM lp);

void Spinner_GetNewText (SpinnerInfo *psi);
void Spinner_SetNewText (SpinnerInfo *psi, BOOL fCallback);


BOOL RegisterSpinnerClass (void)
{
   static BOOL fRegistered = FALSE;

   if (!fRegistered)
      {
      WNDCLASS wc;

      InitializeCriticalSection (&csSpinners);

      if (GetClassInfo (THIS_HINST, TEXT("scrollbar"), &wc))
         {
         oldSpinnerProc = (LONG)wc.lpfnWndProc;

         wc.lpfnWndProc = (WNDPROC)SpinnerProc;
         wc.style = CS_GLOBALCLASS;
         wc.hInstance = THIS_HINST;
         wc.lpszClassName = cszSPINNERCLASS;

         if (RegisterClass (&wc))
            fRegistered = TRUE;
         }
      }

   return fRegistered;
}


SpinnerInfo *Spinner_FindSpinnerInfo (HWND hSpinner, HWND hBuddy)
{
   SpinnerInfo *psi = NULL;

   EnterCriticalSection (&csSpinners);

   for (size_t ii = 0; ii < cSpinners; ++ii)
      {
      if ( (hSpinner && (aSpinners[ ii ].hSpinner == hSpinner)) ||
           (hBuddy   && (aSpinners[ ii ].hBuddy == hBuddy)) )
         {
         psi = &aSpinners[ ii ];
         break;
         }
      }

   LeaveCriticalSection (&csSpinners);
   return psi;
}


BOOL CreateSpinner (HWND hBuddy,
                    WORD wBase, BOOL fSigned,
                    DWORD dwMin, DWORD dwPos, DWORD dwMax,
                    LPRECT prTarget)
{
   if (!RegisterSpinnerClass())
      return FALSE;

   EnterCriticalSection (&csSpinners);

   size_t ii;
   for (ii = 0; ii < cSpinners; ++ii)
      {
      if (!aSpinners[ ii ].hSpinner)
         break;
      }
   if (ii >= cSpinners)
      {
      if (!REALLOC (aSpinners, cSpinners, 1+ii, 4))
         {
         LeaveCriticalSection (&csSpinners);
         return FALSE;
         }
      }

   memset (&aSpinners[ ii ], 0x00, sizeof(SpinnerInfo));

   aSpinners[ ii ].hBuddy = hBuddy;
   aSpinners[ ii ].dwMin = dwMin;
   aSpinners[ ii ].dwMax = dwMax;
   aSpinners[ ii ].wBase = wBase;
   aSpinners[ ii ].fSigned = fSigned;
   aSpinners[ ii ].dwPos = dwPos;

   if (prTarget != NULL)
      aSpinners[ ii ].rReq = *prTarget;

   aSpinners[ ii ].hSpinner = CreateWindowEx (
                0,	// extended window style
                cszSPINNERCLASS,	// pointer to registered class name
                TEXT(""),	// pointer to window name
                WS_CHILD | SBS_VERT,	// window style
                0, 0, 1, 1,	// spinner moves/resizes itself
                GetParent(hBuddy),	// handle to parent or owner window
                (HMENU)-1,	// handle to menu, or child-window identifier
                THIS_HINST,	// handle to application instance
                (LPVOID)ii); 	// pointer to window-creation data

   LeaveCriticalSection (&csSpinners);

   if (aSpinners[ ii ].hSpinner == NULL)
      return FALSE;

   ShowWindow (aSpinners[ ii ].hSpinner, SW_SHOW);

   if (!IsWindowEnabled (aSpinners[ ii ].hBuddy))
      EnableWindow (aSpinners[ ii ].hSpinner, FALSE);

   return TRUE;
}


BOOL fHasSpinner (HWND hBuddy)
{
   if (!RegisterSpinnerClass())
      return FALSE;

   return (Spinner_FindSpinnerInfo (NULL, hBuddy) == NULL) ? FALSE : TRUE;
}


void Spinner_OnDestroy (SpinnerInfo *psi)
{
   Subclass_RemoveHook (GetParent (psi->hSpinner), SpinnerDialogProc);
   Subclass_RemoveHook (psi->hBuddy, SpinnerBuddyProc);

   if (psi->pszFormat)
      {
      Free (psi->pszFormat);
      psi->pszFormat = NULL;
      }
   psi->hSpinner = NULL;
   psi->hBuddy = NULL;
}


BOOL CALLBACK SpinnerProc (HWND hSpinner, UINT msg, WPARAM wp, LPARAM lp)
{
   EnterCriticalSection (&csSpinners);

   if (msg == WM_CREATE)
      {
      aSpinners[ (int)((LPCREATESTRUCT)lp)->lpCreateParams ].hSpinner = hSpinner;
      }

   SpinnerInfo *psi = Spinner_FindSpinnerInfo (hSpinner, NULL);

   LeaveCriticalSection (&csSpinners);

   if (psi != NULL)
      {
      switch (msg)
         {
         case WM_CREATE:
            Spinner_OnCreate (psi);
            break;

         case WM_SETFOCUS:
            PostMessage (GetParent(psi->hSpinner), WM_NEXTDLGCTL, (WPARAM)psi->hBuddy, TRUE);
            break;

         case WM_DESTROY:
            Spinner_OnDestroy (psi);
            break;

         case WM_SYSCHAR:
         case WM_CHAR:
            switch (wp)
               {
               case VK_UP:
                  PostMessage (GetParent(psi->hSpinner), WM_VSCROLL, SB_LINEUP, (LPARAM)psi->hSpinner);
                  break;

               case VK_DOWN:
                  PostMessage (GetParent(psi->hSpinner), WM_VSCROLL, SB_LINEDOWN, (LPARAM)psi->hSpinner);
                  break;

               case VK_PRIOR:
                  PostMessage (GetParent(psi->hSpinner), WM_VSCROLL, SB_PAGEUP, (LPARAM)psi->hSpinner);
                  break;

               case VK_NEXT:
                  PostMessage (GetParent(psi->hSpinner), WM_VSCROLL, SB_PAGEDOWN, (LPARAM)psi->hSpinner);
                  break;

               case VK_HOME:
                  PostMessage (GetParent(psi->hSpinner), WM_VSCROLL, SB_TOP, (LPARAM)psi->hSpinner);
                  break;

               case VK_END:
                  PostMessage (GetParent(psi->hSpinner), WM_VSCROLL, SB_BOTTOM, (LPARAM)psi->hSpinner);
                  break;
               }
            break;
         }
      }

   if (oldSpinnerProc == 0)
      return DefWindowProc (hSpinner, msg, wp, lp);
   else
      return CallWindowProc ((WNDPROC)oldSpinnerProc, hSpinner, msg, wp, lp);
}


BOOL CALLBACK SpinnerDialogProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hDlg, SpinnerDialogProc);
   SpinnerInfo *psi;

   switch (msg)
      {
      case WM_COMMAND:
         if ((psi = Spinner_FindSpinnerInfo (NULL, (HWND)lp)) != NULL)
            {
            switch (HIWORD(wp))
               {
               // case CBN_SELCHANGE: --same value as LBN_SELCHANGE
               case LBN_SELCHANGE:
               case EN_UPDATE:
                  Spinner_GetNewText (psi);

                  if (psi->fCanCallBack == TRUE)
                     SpinnerSendCallback (psi, SPN_UPDATE, (LPARAM)psi->dwPos);
                  else
                     oldProc = NULL; // don't forward this notification message
                  break;

               default:
                  oldProc = NULL; // don't forward this notification message
                  break;
               }
            }
         break;

      case WM_VSCROLL:
         if ((psi = Spinner_FindSpinnerInfo ((HWND)lp, NULL)) != NULL)
            {
            if (psi->fNewText)
               {
               WORD wBaseOld = psi->wBase;
               Spinner_GetNewText (psi);
               if ((wBaseOld != psi->wBase) && !(psi->pszFormat))
                  Spinner_OnSetBase (psi, psi->wBase, psi->fSigned);
               }

            switch (LOWORD(wp))
               {
               case SB_LINEUP:
                  {
                  DWORD dw = psi->dwPos;
                  SpinnerSendCallback (psi, SPN_CHANGE_UP, (LPARAM)&dw);
                  if (dw == psi->dwPos)
                     psi->dwPos ++;
                  else if (dw != SPVAL_UNCHANGED)
                     psi->dwPos = dw;

                  if (psi->wBase == 10 && psi->fSigned)
                     psi->dwPos = (DWORD)limit ((signed long)psi->dwMin, (signed long)psi->dwPos, (signed long)psi->dwMax);
                  else
                     psi->dwPos = (DWORD)limit (psi->dwMin, psi->dwPos, psi->dwMax);

                  Spinner_SetNewText (psi, TRUE);
                  PostMessage (GetParent(psi->hSpinner), WM_NEXTDLGCTL, (WPARAM)psi->hBuddy, TRUE);
                  break;
                  }

               case SB_LINEDOWN:
                  {
                  DWORD dw = psi->dwPos;
                  SpinnerSendCallback (psi, SPN_CHANGE_DOWN, (LPARAM)&dw);
                  if (dw == psi->dwPos)
                     {
                     if ((psi->dwPos > 0) || (psi->wBase == 10 && psi->fSigned))
                        psi->dwPos --;
                     }
                  else if (dw != SPVAL_UNCHANGED)
                     psi->dwPos = dw;

                  if (psi->wBase == 10 && psi->fSigned)
                     psi->dwPos = (DWORD)limit ((signed long)psi->dwMin, (signed long)psi->dwPos, (signed long)psi->dwMax);
                  else
                     psi->dwPos = (DWORD)limit (psi->dwMin, psi->dwPos, psi->dwMax);

                  Spinner_SetNewText (psi, TRUE);
                  PostMessage (GetParent(psi->hSpinner), WM_NEXTDLGCTL, (WPARAM)psi->hBuddy, TRUE);
                  break;
                  }
               }
            }
         break;
      }

   if (oldProc == 0)
      return DefWindowProc (hDlg, msg, wp, lp);
   else
      return CallWindowProc ((WNDPROC)oldProc, hDlg, msg, wp, lp);
}



BOOL CALLBACK SpinnerBuddyProc (HWND hBuddy, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hBuddy, SpinnerBuddyProc);

   SpinnerInfo *psi;
   if ((psi = Spinner_FindSpinnerInfo (NULL, hBuddy)) != NULL)
      {
      switch (msg)
         {
         case WM_KEYDOWN:
         case WM_KEYUP:
            switch (wp)
               {
               case VK_HOME:
               case VK_END:
               case VK_PRIOR:
               case VK_NEXT:
               case VK_UP:
               case VK_DOWN:
                  SendMessage (psi->hSpinner, msg, wp, lp);
                  return FALSE;
               }
            break;

         case WM_CHAR:
            psi->fNewText = TRUE;
            break;

         case WM_MOVE:
         case WM_SIZE:
            PostMessage (hBuddy, SPM_REATTACH, 0, 0);
            break;

         case WM_ENABLE:
            EnableWindow (psi->hSpinner, wp);
            break;

         case WM_KILLFOCUS:
            Spinner_GetNewText (psi);
            Spinner_OnSetBase (psi, psi->wBase, psi->fSigned);
            break;

         case SPM_GETRANGE:
            return Spinner_OnGetRange (psi, wp, lp);

         case SPM_SETRANGE:
            return Spinner_OnSetRange (psi, wp, lp);

         case SPM_GETPOS:
            return Spinner_OnGetPos (psi, wp, lp);

         case SPM_SETPOS:
            return Spinner_OnSetPos (psi, wp, lp);

         case SPM_GETBASE:
            return Spinner_OnGetBase (psi, wp, lp);

         case SPM_SETBASE:
            return Spinner_OnSetBase (psi, wp, lp);

         case SPM_REATTACH:
            return Spinner_OnReattach (psi, wp, lp);

         case SPM_SETRECT:
            return Spinner_OnSetRect (psi, wp, lp);

         case SPM_GETSPINNER:
            return Spinner_OnGetSpinner (psi, wp, lp);

         case SPM_SETFORMAT:
            return Spinner_OnSetFormat (psi, wp, lp);

         case SPM_SETBUDDY:
            return Spinner_OnSetBuddy (psi, wp, lp);
         }
      }

   if (oldProc)
      return CallWindowProc ((WNDPROC)oldProc, hBuddy, msg, wp, lp);
   else
      return DefWindowProc (hBuddy, msg, wp, lp);
}


void SpinnerSendCallback (SpinnerInfo *psi, WORD spm, LPARAM lp)
{
   if ((psi->fCanCallBack == TRUE) && !psi->fCallingBack)
      {
      psi->fCallingBack = TRUE;

      SendMessage (GetParent (psi->hSpinner),
                   WM_COMMAND,
                   MAKELONG ((WORD)GetWindowLong (psi->hBuddy, GWL_ID), spm),
                   lp);

      psi->fCallingBack = FALSE;
      }
}


void Spinner_OnCreate (SpinnerInfo *psi)
{
   Subclass_AddHook (GetParent(psi->hSpinner), SpinnerDialogProc);
   Subclass_AddHook (psi->hBuddy, SpinnerBuddyProc);

   Spinner_OnReattach (psi, 0, 0);
   Spinner_SetNewText (psi, FALSE);

   psi->fCanCallBack = TRUE;
}


BOOL Spinner_OnGetRange (SpinnerInfo *psi, WPARAM wp, LPARAM lp)
{
   if (wp != 0)
      *(LPDWORD)wp = psi->dwMin;
   if (lp != 0)
      *(LPDWORD)lp = psi->dwMax;
   return TRUE;
}

BOOL Spinner_OnSetRange (SpinnerInfo *psi, WPARAM wp, LPARAM lp)
{
   psi->dwMin = (DWORD)wp;
   psi->dwMax = (DWORD)lp;
   Spinner_SetNewText (psi, FALSE);
   return TRUE;
}

BOOL Spinner_OnGetPos (SpinnerInfo *psi, WPARAM wp, LPARAM lp)
{
   if (psi->fNewText)
      {
      Spinner_GetNewText (psi);
      Spinner_OnSetBase (psi, psi->wBase, psi->fSigned);
      }
   return (BOOL)psi->dwPos;
}

BOOL Spinner_OnSetPos (SpinnerInfo *psi, WPARAM wp, LPARAM lp)
{
   psi->dwPos = (DWORD)lp;
   Spinner_SetNewText (psi, FALSE);
   return TRUE;
}

BOOL Spinner_OnGetBase (SpinnerInfo *psi, WPARAM wp, LPARAM lp)
{
   if (psi->fNewText)
      {
      Spinner_GetNewText (psi);
      Spinner_OnSetBase (psi, psi->wBase, psi->fSigned);
      }

   if (wp != 0)
      *(WORD *)wp = psi->wBase;
   if (lp != 0)
      *(BOOL *)lp = psi->fSigned;
   return TRUE;
}

BOOL Spinner_OnSetBase (SpinnerInfo *psi, WPARAM wp, LPARAM lp)
{
   if (psi->fNewText)
      Spinner_GetNewText (psi);

   switch ((WORD)wp)
      {
      case  2:
      case  8:
      case 10:
      case 16:
         psi->wBase = (WORD)wp;
         break;

      default:
         psi->wBase = 10;
         break;
      }

   if (psi->wBase != 10)
      psi->fSigned = FALSE;
   else
      psi->fSigned = (BOOL)lp;

   Spinner_SetNewText (psi, FALSE);
   return TRUE;
}


BOOL Spinner_OnReattach (SpinnerInfo *psi, WPARAM wp, LPARAM lp)
{
   RECT  rSpinner;
   if (psi->rReq.right != 0)
      {
      rSpinner = psi->rReq;
      }
   else
      {
      RECT  rBuddyInParent;
      POINT pt = { 0, 0 };

      ClientToScreen (GetParent (psi->hBuddy), &pt);

      GetWindowRect (psi->hBuddy, &rBuddyInParent);
      rBuddyInParent.left -= pt.x;
      rBuddyInParent.right -= pt.x;
      rBuddyInParent.top -= pt.y;
      rBuddyInParent.bottom -= pt.y;

      rSpinner.top = rBuddyInParent.top;
      rSpinner.bottom = rBuddyInParent.bottom -2; // just like Win95 does
      rSpinner.left = rBuddyInParent.right;
      rSpinner.right = rBuddyInParent.right +GetSystemMetrics (SM_CXVSCROLL);
      }

   SetWindowPos (psi->hSpinner, NULL,
                 rSpinner.left, rSpinner.top,
                 rSpinner.right-rSpinner.left,
                 rSpinner.bottom-rSpinner.top,
                 SWP_NOACTIVATE | SWP_NOZORDER);
   return TRUE;
}


BOOL Spinner_OnSetRect (SpinnerInfo *psi, WPARAM wp, LPARAM lp)
{
   LPRECT prTarget;
   if ((prTarget = (LPRECT)lp) == NULL)
      SetRectEmpty (&psi->rReq);
   else
      psi->rReq = *prTarget;

   Spinner_OnReattach (psi, 0, 0);
   return TRUE;
}


BOOL Spinner_OnGetSpinner (SpinnerInfo *psi, WPARAM wp, LPARAM lp)
{
   return (BOOL)psi->hSpinner;
}


BOOL Spinner_OnSetFormat (SpinnerInfo *psi, WPARAM wp, LPARAM lp)
{
   if (psi->pszFormat)
      {
      Free (psi->pszFormat);
      psi->pszFormat = NULL;
      }
   if (lp != 0)
      {
      psi->pszFormat = (LPTSTR)Allocate (sizeof(TCHAR) * (1+lstrlen((LPTSTR)lp)));
      lstrcpy (psi->pszFormat, (LPTSTR)lp);
      }
   Spinner_SetNewText (psi, FALSE);
   return TRUE;
}


BOOL Spinner_OnSetBuddy (SpinnerInfo *psi, WPARAM wp, LPARAM lp)
{
   HWND hBuddyNew = (HWND)wp;
   BOOL fMove = (BOOL)lp;

   // First un-subclass our buddy.
   // Then subclass the new buddy.
   //
   Subclass_RemoveHook (psi->hBuddy, SpinnerBuddyProc);
   psi->hBuddy = hBuddyNew;
   Subclass_AddHook (psi->hBuddy, SpinnerBuddyProc);

   // Update our SpinnerInfo structure, and move if requested.
   //
   Spinner_GetNewText (psi);

   if (fMove)
      {
      SetRectEmpty (&psi->rReq);
      Spinner_OnReattach (psi, 0, 0);
      }

   return TRUE;
}


void Spinner_GetNewText (SpinnerInfo *psi)
{
   // First find out what kind of buddy we have.
   // That will determine what we do here.
   //
   TCHAR szBuddyClass[256];
   GetClassName (psi->hBuddy, szBuddyClass, 256);

   // For comboboxes and listboxes, the dwPos value is actually
   // the selected item's index.
   //
   if (!lstrcmpi (szBuddyClass, TEXT("listbox")))
      {
      psi->dwPos = (DWORD)LB_GetSelected (psi->hBuddy);
      }

   if (!lstrcmpi (szBuddyClass, TEXT("combobox")))
      {
      psi->dwPos = (DWORD)CB_GetSelected (psi->hBuddy);
      }

   // For edit controls, the dwPos value is actually
   // the control's text's value.
   //
   if (!lstrcmpi (szBuddyClass, TEXT("edit")))
      {
      TCHAR szText[256];
      LPTSTR pszText = szText;
      BOOL fNegative = FALSE;

      psi->fNewText = FALSE;
      psi->dwPos = 0;
      psi->wBase = 10;

      GetWindowText (psi->hBuddy, szText, 256);

      while (*pszText == TEXT(' ') || *pszText == TEXT('\t'))
         ++pszText;

      if (*pszText == TEXT('0'))
         {
         if ((*(1+pszText) == 'x') || (*(1+pszText) == 'X'))
            {
            psi->wBase = 16;
            ++pszText;
            ++pszText;
            }
         else if ((*(1+pszText) == 'b') || (*(1+pszText) == 'B') || (*(1+pszText) == '!'))
            {
            psi->wBase = 2;
            ++pszText;
            ++pszText;
            }
         else if (*(1+pszText) != '\0')
            {
            // psi->wBase = 8; // ignore octal--time controls use "4:08" etc
            ++pszText;
            }
         }

      for ( ; *pszText == TEXT('-'); ++pszText)
         {
         fNegative = !fNegative;
         }

      for ( ; *pszText; ++pszText)
         {
         if (!isdigit( *pszText ) &&
             !(psi->wBase == 16 && ishexdigit( *pszText )))
            {
            break;
            }

         psi->dwPos *= psi->wBase;

         if ((DWORD)digitval(*pszText) < (DWORD)psi->wBase)
            psi->dwPos += digitval( *pszText );
         }

      if (fNegative && psi->wBase == 10 && psi->fSigned)
         {
         psi->dwPos = (DWORD)(0 - (signed long)psi->dwPos);
         }
      }

   if (psi->wBase == 10 && psi->fSigned)
      psi->dwPos = (DWORD)limit ((signed long)psi->dwMin, (signed long)psi->dwPos, (signed long)psi->dwMax);
   else
      psi->dwPos = (DWORD)limit (psi->dwMin, psi->dwPos, psi->dwMax);
}


void Spinner_SetNewText (SpinnerInfo *psi, BOOL fCallBack)
{
   TCHAR szText[256];

   // First find out what kind of buddy we have.
   // That will determine what we do here.
   //
   TCHAR szBuddyClass[256];
   GetClassName (psi->hBuddy, szBuddyClass, 256);

   // Be sure to notify the parent window that the selection may be changing.
   //
   if (fCallBack)
      {
      DWORD dw = psi->dwPos;
      SpinnerSendCallback (psi, SPN_CHANGE, (LPARAM)&dw);
      if (dw != SPVAL_UNCHANGED)
         psi->dwPos = dw;
      }

   if (psi->wBase == 10 && psi->fSigned)
      psi->dwPos = (DWORD)limit ((signed long)psi->dwMin, (signed long)psi->dwPos, (signed long)psi->dwMax);
   else
      psi->dwPos = (DWORD)limit (psi->dwMin, psi->dwPos, psi->dwMax);

   if (!fCallBack)
      psi->fCanCallBack --;

   // For comboboxes and listboxes, select the item specified by the
   // given index.
   //
   if (!lstrcmpi (szBuddyClass, TEXT("listbox")))
      {
      LB_SetSelected (psi->hBuddy, psi->dwPos);
      }

   if (!lstrcmpi (szBuddyClass, TEXT("combobox")))
      {
      CB_SetSelected (psi->hBuddy, psi->dwPos);
      }

   // For edit controls, fill in the new value as text--expressed in the
   // requested base, using the requested format.
   //
   if (!lstrcmpi (szBuddyClass, TEXT("edit")))
      {
      switch (psi->wBase)
         {
         case 10:
            if (psi->pszFormat)
               wsprintf (szText, psi->pszFormat, (unsigned long)psi->dwPos);
            else if (psi->fSigned)
               wsprintf (szText, TEXT("%ld"), (signed long)psi->dwPos);
            else
               wsprintf (szText, TEXT("%lu"), (unsigned long)psi->dwPos);
            break;

         case 16:
            wsprintf (szText, TEXT("0x%lX"), (unsigned long)psi->dwPos);
            break;

         default:
            TCHAR szTemp[256];
            LPTSTR pszTemp = szTemp;
            LPTSTR pszText = szText;

            if (psi->dwPos == 0)
               *pszTemp++ = TEXT('0');
            else
               {
               for (DWORD dwRemainder = psi->dwPos; dwRemainder != 0; dwRemainder /= (DWORD)psi->wBase)
                  {
                  DWORD dw = (dwRemainder % (DWORD)psi->wBase);
                  *pszTemp++ = TEXT('0') + (TCHAR)dw;
                  }
               }

            if (psi->wBase == 8)
               {
               *pszText++ = TEXT('0');
               }
            else if (psi->wBase == 2)
               {
               *pszText++ = TEXT('0');
               *pszText++ = TEXT('b');
               }

            for (--pszTemp; pszTemp >= szTemp; )
               {
               *pszText++ = *pszTemp--;
               }
            *pszText = TEXT('\0');
            break;
         }

      SetWindowText (psi->hBuddy, szText);
      }

   if (!fCallBack)
      psi->fCanCallBack ++;
   else
      SpinnerSendCallback (psi, SPN_UPDATE, (LPARAM)psi->dwPos);
}

