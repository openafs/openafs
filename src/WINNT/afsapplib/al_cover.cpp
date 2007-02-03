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

#include <WINNT/afsapplib.h>


/*
 * RESIZING WINDOWS ___________________________________________________________
 *
 */

rwWindowData awdCover[] = {
    { IDC_COVER_BORDER, raSizeX | raSizeY,		0,	0 },
    { IDC_COVER_DESC,   raSizeX | raSizeY | raRepaint,	0,	0 },
    { IDC_COVER_BUTTON, raMoveX | raMoveY | raRepaint,	0,	0 },
    { idENDLIST,        0,				0,	0 }
 };

#define WS_EX_HIDDENBYCOVER       0x10000000L


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define dwCOVER_SIGNATURE  0xC0E0C0E0  // SetWindowLongPtr(hDlgCover,DWLP_USER,#)

typedef struct // COVERPARAMS
   {
   BOOL fClient;
   HWND hWnd;
   LPTSTR pszDesc;
   LPTSTR pszButton;
   } COVERPARAMS, *LPCOVERPARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void OnCoverWindow (WPARAM wp, LPARAM lp);

HRESULT CALLBACK Cover_DialogProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void AfsAppLib_CoverClient (HWND hWnd, LPTSTR pszDesc, LPTSTR pszButton)
{
   AfsAppLib_Uncover (hWnd);

   LPCOVERPARAMS lpcp = New (COVERPARAMS);

   lpcp->fClient = TRUE;
   lpcp->hWnd = hWnd;
   lpcp->pszDesc = CloneString (pszDesc);
   lpcp->pszButton = CloneString (pszButton);

   if (!AfsAppLib_GetMainWindow())
      OnCoverWindow (0, (LPARAM)lpcp);
   else
      PostMessage (AfsAppLib_GetMainWindow(), WM_COVER_WINDOW, 0, (LPARAM)lpcp);
}


void AfsAppLib_CoverWindow (HWND hWnd, LPTSTR pszDesc, LPTSTR pszButton)
{
   AfsAppLib_Uncover (hWnd);

   LPCOVERPARAMS lpcp = New (COVERPARAMS);

   lpcp->fClient = FALSE;
   lpcp->hWnd = hWnd;
   lpcp->pszDesc = CloneString (pszDesc);
   lpcp->pszButton = CloneString (pszButton);

   if (!AfsAppLib_GetMainWindow())
      OnCoverWindow (0, (LPARAM)lpcp);
   else
      PostMessage (AfsAppLib_GetMainWindow(), WM_COVER_WINDOW, 0, (LPARAM)lpcp);
}


void AfsAppLib_Uncover (HWND hDlg)
{
   if (!AfsAppLib_GetMainWindow())
      OnCoverWindow ((WPARAM)hDlg, 0);
   else
      PostMessage (AfsAppLib_GetMainWindow(), WM_COVER_WINDOW, (WPARAM)hDlg, 0);
}


void OnCoverWindow (WPARAM wp, LPARAM lp)
{
   LPCOVERPARAMS lpcp;
   if ((lpcp = (LPCOVERPARAMS)lp) == NULL)
      {
      HWND hDlg = (HWND)wp;

      if (!IsWindowVisible (hDlg))	// did we create it as a sibling?
         {
         ShowWindow (hDlg, TRUE);
         EnableWindow (hDlg, TRUE);
         hDlg = GetParent(hDlg);
         }

      for (HWND hChild = GetWindow (hDlg, GW_CHILD);
           hChild != NULL;
           hChild = GetWindow (hChild, GW_HWNDNEXT))
         {
         TCHAR szClassName[ cchRESOURCE ];

         if (GetClassName (hChild, szClassName, cchRESOURCE))
            {
            if (!lstrcmp (szClassName, TEXT("#32770"))) // WC_DIALOG
               {
               if (GetWindowLongPtr (hChild, DWLP_USER) == dwCOVER_SIGNATURE)
                  {
                  DestroyWindow (hChild);
                  break;
                  }
               }
            }
         }
      }
   else
      {
      HWND hCover = ModelessDialogParam (IDD_APPLIB_COVER,
                                         (lpcp->fClient) ? lpcp->hWnd : GetParent(lpcp->hWnd),
                                         (DLGPROC)Cover_DialogProc,
                                         (LPARAM)lp);

      ShowWindow (hCover, TRUE);
      SetWindowLongPtr (hCover, DWLP_USER, dwCOVER_SIGNATURE);

      FreeString (lpcp->pszDesc);
      FreeString (lpcp->pszButton);
      Delete (lpcp);
      }
}


HRESULT CALLBACK Cover_DialogProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         LPCOVERPARAMS lpcp;
         lpcp = (LPCOVERPARAMS)lp;

         SetDlgItemText (hDlg, IDC_COVER_DESC, (lpcp && lpcp->pszDesc) ? lpcp->pszDesc : TEXT(""));

         if (lpcp && lpcp->pszButton)
            {
            SetDlgItemText (hDlg, IDC_COVER_BUTTON, lpcp->pszButton);
            }
         else
            {
            DestroyWindow (GetDlgItem (hDlg, IDC_COVER_BUTTON));
            }

         RECT rCover;
         if (lpcp->fClient)
            {
            // If we're covering a window's client area, we've been created
            // as a child of that window.  Hide its other children, and we'll
            // be done.
            //
            GetClientRect (lpcp->hWnd, &rCover);

            for (HWND hChild = GetWindow (lpcp->hWnd, GW_CHILD);
                 hChild != NULL;
                 hChild = GetWindow (hChild, GW_HWNDNEXT))
               {
               if (hChild == hDlg)
                  continue;

               if (IsWindowVisible (hChild))
                  {
                  LONG dwStyleEx = GetWindowLong (hChild, GWL_EXSTYLE);
                  SetWindowLong (hChild, GWL_EXSTYLE, dwStyleEx | WS_EX_HIDDENBYCOVER);
                  ShowWindow (hChild, FALSE);
                  }
               }
            }
         else
            {
            // If we're covering a window's complete area, we've been created
            // as a sibling of that window.  Hide that window.
            //
            GetRectInParent (lpcp->hWnd, &rCover);
            ShowWindow (lpcp->hWnd, FALSE);
            EnableWindow (lpcp->hWnd, FALSE);
            }

         if (lpcp && lpcp->pszButton)
            {
            RECT rDesc;  // give the button's space to the Description window
            RECT rButton;
            GetRectInParent (GetDlgItem (hDlg, IDC_COVER_DESC), &rDesc);
            GetRectInParent (GetDlgItem (hDlg, IDC_COVER_BUTTON), &rButton);

            // If the window is higher than it is wide, put the description
            // above the button. Otherwise, put it to the right of the button.
            //
            if (cyRECT(rCover) > cxRECT(rCover))
               {
               // shrink description vertically
               SetWindowPos (GetDlgItem (hDlg, IDC_COVER_DESC), NULL,
                             0, 0,
                             cxRECT(rDesc),
                             cyRECT(rDesc) - rDesc.bottom + rButton.top -5,
                             SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
               }
            else
               {
               // shrink description horizontally
               SetWindowPos (GetDlgItem (hDlg, IDC_COVER_DESC), NULL,
                             0, 0,
                             cxRECT(rDesc) - rDesc.right + rButton.left -5,
                             cyRECT(rDesc),
                             SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
               }
            }

         ResizeWindow (hDlg, awdCover, rwaMoveToHere, &rCover);
         break;

      case WM_DESTROY:
         // Show all of our parent's other children, in preparation for
         // our going away.
         //
         HWND hChild;
         for (hChild = GetWindow (GetParent (hDlg), GW_CHILD);
              hChild != NULL;
              hChild = GetWindow (hChild, GW_HWNDNEXT))
            {
            if (hChild == hDlg)
               continue;

            LONG dwStyleEx = GetWindowLong (hChild, GWL_EXSTYLE);
            if (dwStyleEx & WS_EX_HIDDENBYCOVER)
               {
               SetWindowLong (hChild, GWL_EXSTYLE, dwStyleEx & (~WS_EX_HIDDENBYCOVER));
               ShowWindow (hChild, TRUE);
               }
            }
         break;

      case WM_SIZE:
         // if (lp==0), we're minimizing--don't call ResizeWindow().
         //
         if (lp != 0)
            ResizeWindow (hDlg, awdCover, rwaFixupGuts);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_COVER_BUTTON:
               PostMessage (GetParent(hDlg), WM_COMMAND, wp, lp);
               break;
            }
         break;
      }

   return FALSE;
}

