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
 * PROTOTYPES _________________________________________________________________
 *
 */

void cdecl vErrorDialog (BOOL fFatal, DWORD dwStatus, LONG idError, LPTSTR pszFmt, va_list arg);

HRESULT CALLBACK Error_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void OnCreateErrorDialog (WPARAM wp, LPARAM lp);


/*
 * ERROR DIALOGS ______________________________________________________________
 *
 */

typedef struct
   {
   BOOL fFatal;
   LPTSTR pszError;
   DWORD dwError;
   } ERRORPARAMS;

void cdecl ErrorDialog (DWORD dwStatus, LPTSTR pszError, LPTSTR pszFmt, ...)
{
   va_list arg;
   va_start (arg, pszFmt);
   vErrorDialog (FALSE, dwStatus, (LONG)(LONG_PTR)pszError, pszFmt, arg);
}

void cdecl ErrorDialog (DWORD dwStatus, int idsError, LPTSTR pszFmt, ...)
{
   va_list arg;
   va_start (arg, pszFmt);
   vErrorDialog (FALSE, dwStatus, (LONG)idsError, pszFmt, arg);
}

void cdecl FatalErrorDialog (DWORD dwStatus, LPTSTR pszError, LPTSTR pszFmt, ...)
{
   va_list arg;
   va_start (arg, pszFmt);
   vErrorDialog (TRUE, dwStatus, (LONG)(LONG_PTR)pszError, pszFmt, arg);
}

void cdecl FatalErrorDialog (DWORD dwStatus, int idsError, LPTSTR pszFmt, ...)
{
   va_list arg;
   va_start (arg, pszFmt);
   vErrorDialog (TRUE, dwStatus, (LONG)idsError, pszFmt, arg);
}


void cdecl vErrorDialog (BOOL fFatal, DWORD dwStatus, LONG idError, LPTSTR pszFmt, va_list arg)
{
   ERRORPARAMS *lpep = New (ERRORPARAMS);
   lpep->fFatal = fFatal;
   lpep->pszError = vFormatString (idError, pszFmt, arg);
   lpep->dwError = dwStatus;

   if (!AfsAppLib_GetMainWindow())
      {
      OnCreateErrorDialog (0, (LPARAM)lpep);
      }
   else
      {
      PostMessage (AfsAppLib_GetMainWindow(), WM_CREATE_ERROR_DIALOG, 0, (LPARAM)lpep);
      }
}


void ImmediateErrorDialog (DWORD dwStatus, int idsError, LPTSTR pszFmt, ...)
{
   va_list arg;
   va_start (arg, pszFmt);

   ERRORPARAMS *lpep = New (ERRORPARAMS);
   lpep->fFatal = FALSE;
   lpep->pszError = vFormatString (idsError, pszFmt, arg);
   lpep->dwError = dwStatus;

   ModalDialogParam (IDD_APPLIB_ERROR, NULL, (DLGPROC)Error_DlgProc, (LPARAM)lpep);

   FreeString (lpep->pszError);
   Delete (lpep);
}

void ImmediateErrorDialog (DWORD dwStatus, LPTSTR pszError, LPTSTR pszFmt, ...)
{
   va_list arg;
   va_start (arg, pszFmt);

   ERRORPARAMS *lpep = New (ERRORPARAMS);
   lpep->fFatal = FALSE;
   lpep->pszError = vFormatString (pszError, pszFmt, arg);
   lpep->dwError = dwStatus;

   ModalDialogParam (IDD_APPLIB_ERROR, NULL, (DLGPROC)Error_DlgProc, (LPARAM)lpep);

   FreeString (lpep->pszError);
   Delete (lpep);
}


void OnCreateErrorDialog (WPARAM wp, LPARAM lp)
{
   ERRORPARAMS *lpep = (ERRORPARAMS*)lp;

   ModalDialogParam (IDD_APPLIB_ERROR, NULL, (DLGPROC)Error_DlgProc, (LPARAM)lpep);

   if (lpep->fFatal)
      {
      PostQuitMessage (0);
      }

   FreeString (lpep->pszError);
   Delete (lpep);
}


HRESULT CALLBACK Error_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         TCHAR szApplication[ cchNAME ];
         AfsAppLib_GetAppName (szApplication);
         if (szApplication[0] != TEXT('\0'))
            {
            TCHAR szTitle[ cchRESOURCE ];
            GetWindowText (hDlg, szTitle, cchRESOURCE);
            lstrcat (szTitle, TEXT(" - "));
            lstrcat (szTitle, szApplication);
            SetWindowText (hDlg, szTitle);
            }

         ERRORPARAMS *lpep;
         if ((lpep = (ERRORPARAMS *)lp) == NULL)
            {
            TCHAR szError[ cchRESOURCE ];
            AfsAppLib_TranslateError (szError, ERROR_NOT_ENOUGH_MEMORY);
            SetDlgItemText (hDlg, IDC_ERROR_STATUS, szError);
            }
         else
            {
            SetDlgItemText (hDlg, IDC_ERROR_DESC, lpep->pszError);

            if (lpep->dwError == 0)
               {
               RECT rDesc;
               GetRectInParent (GetDlgItem (hDlg, IDC_ERROR_DESC), &rDesc);

               RECT rStatus;
               GetRectInParent (GetDlgItem (hDlg, IDC_ERROR_STATUS), &rStatus);

               SetWindowPos (GetDlgItem (hDlg, IDC_ERROR_DESC), NULL,
                             0, 0, cxRECT(rDesc), rStatus.bottom -rDesc.top,
                             SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);

               ShowWindow (GetDlgItem (hDlg, IDC_ERROR_STATUS), SW_HIDE);
               }
            else
               {
               TCHAR szCode[ cchRESOURCE ];
               GetDlgItemText (hDlg, IDC_ERROR_STATUS, szCode, cchRESOURCE);

               LPTSTR pszStatus = FormatString (szCode, TEXT("%e"), lpep->dwError);
               SetDlgItemText (hDlg, IDC_ERROR_STATUS, pszStatus);
               FreeString (pszStatus);
               }
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               EndDialog (hDlg, LOWORD(wp));
               break;
            }
         break;
      }

   return FALSE;
}

