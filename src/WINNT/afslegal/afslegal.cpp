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
#include <WINNT/talocale.h>
#include "resource.h"

         // The following definition specifies the length of time that the
         // stupid lawyer message will remain on the screen
         //
#define cmsecSHOW_LAWYER_MESSAGE  5000

         // Individual string resources in .RC files can't be more than 256
         // characters long.
         //
#define cchRESOURCE   256


         // Lawyer_OnInitDialog - Populates the lawyer message window
         //
void Lawyer_OnInitDialog (HWND hDlg)
{
   // Make the title item boldfaced
   //
   HFONT hfOld = (HFONT)SendDlgItemMessage (hDlg, IDC_TITLE, WM_GETFONT, 0, 0);

   LOGFONT lf;
   GetObject (hfOld, sizeof(lf), &lf);

   lf.lfWeight = FW_BOLD;

   SendDlgItemMessage (hDlg, IDC_TITLE, WM_SETFONT, (WPARAM)CreateFontIndirect (&lf), 0);

   // Allocate a buffer, load the string, and shove the text
   // in the main window.
   //
   LPTSTR pszMessage = FormatString (TEXT("%1"), TEXT("%m"), IDS_MESSAGE_1);

   SetDlgItemText (hDlg, IDC_MESSAGE, pszMessage);

   FreeString (pszMessage);
}


         // Lawyer_DlgProc - The window procedure for the lawyer message
         //
BOOL CALLBACK Lawyer_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         Lawyer_OnInitDialog (hDlg);
         SetTimer (hDlg, 0, cmsecSHOW_LAWYER_MESSAGE, NULL);
         break;

      case WM_DESTROY:
         PostQuitMessage (0);
         break;

      case WM_TIMER:
         DestroyWindow (hDlg);
         KillTimer (hDlg, 0);
         break;

      case WM_CTLCOLORSTATIC:
         if ((HWND)lp == GetDlgItem (hDlg, IDC_MESSAGE))
            {
            static HBRUSH hbrBackground = CreateSolidBrush (GetSysColor (COLOR_BTNFACE));
            SetBkColor ((HDC)wp, GetSysColor (COLOR_BTNFACE));
            SetTextColor ((HDC)wp, GetSysColor (COLOR_WINDOWTEXT));
            return (BOOL)hbrBackground;
            }
         break;

      case WM_COMMAND:
         if (LOWORD(wp) ==  IDCANCEL)
            DestroyWindow (hDlg);
         break;
      }

   return FALSE;
}


         // WinMain - Creates the window, waits for it to terminate
         //
int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrev, LPSTR pCmdLine, int nCmdShow)
{
   TaLocale_LoadCorrespondingModule (hInst);

   HWND hDlg = ModelessDialog (IDD_LAWYER, NULL, (DLGPROC)Lawyer_DlgProc);
   SetWindowPos (hDlg, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

   MSG msg;
   while (GetMessage (&msg, NULL, 0, 0))
      {
      if (!IsDialogMessage (hDlg, &msg))
         {
         TranslateMessage (&msg);
         DispatchMessage (&msg);
         }
      }

   return 0;
}

