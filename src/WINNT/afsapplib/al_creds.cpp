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
#include "al_dynlink.h"
#include <WINNT/TaAfsAdmSvrClient.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


/*
 * VARIABLES __________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void OnExpiredCredentials (WPARAM wp, LPARAM lp);

HRESULT CALLBACK OpenCell_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void OpenCell_OnInitDialog (HWND hDlg, LPOPENCELLDLG_PARAMS lpp);
BOOL OpenCell_OnOK (HWND hDlg, LPOPENCELLDLG_PARAMS lpp);
void OpenCell_OnCell (HWND hDlg);
void OpenCell_Enable (HWND hDlg, BOOL fEnable);
void OpenCell_OnGotCreds (HWND hDlg, LPARAM lp);

HRESULT CALLBACK NewCreds_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void NewCreds_OnInitDialog (HWND hDlg, LPCREDENTIALSDLG_PARAMS lpp);
BOOL NewCreds_OnOK (HWND hDlg, LPCREDENTIALSDLG_PARAMS lpp);
void NewCreds_OnLogin (HWND hDlg);
void NewCreds_Enable (HWND hDlg, BOOL fEnable);
void NewCreds_GetOutParams (HWND hDlg, LPCREDENTIALSDLG_PARAMS lpp);

HRESULT CALLBACK BadCreds_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL AfsAppLib_CrackCredentials (UINT_PTR hCreds, LPTSTR pszCell, LPTSTR pszUser, LPSYSTEMTIME pst, ULONG *pStatus)
{
   BOOL rc = FALSE;
   ULONG status = 0;

   UINT_PTR idClient;
   if ((idClient = AfsAppLib_GetAdminServerClientID()) != 0)
      {
      rc = asc_CredentialsCrack (idClient, (PVOID) hCreds, pszCell, pszUser, pst, &status);
      }
   else 
       if (OpenClientLibrary())
      {
      char szUserA[ cchRESOURCE ], szUser2A[ cchRESOURCE ];
      char szCellA[ cchRESOURCE ];
      unsigned long dateExpire;
      int fHasKasToken;

      if (afsclient_TokenQuery ((PVOID)hCreds, &dateExpire, szUserA, szUser2A, szCellA, &fHasKasToken, (afs_status_p)&status))
         {
         rc = TRUE;
         CopyAnsiToString (pszUser, szUserA);
         CopyAnsiToString (pszCell, szCellA);
         AfsAppLib_UnixTimeToSystemTime (pst, dateExpire);
         }

      CloseClientLibrary();
      }

   if (!hCreds && pStatus)
      *pStatus = status;
   return rc;
}


UINT_PTR AfsAppLib_GetCredentials (LPCTSTR pszCell, ULONG *pStatus)
{
   UINT_PTR hCreds = 0;
   ULONG status = 0;

   UINT_PTR idClient;
   if ((idClient = AfsAppLib_GetAdminServerClientID()) != 0)
      {
      hCreds = asc_CredentialsGet (idClient, pszCell, &status);
      }
   else
       if (OpenClientLibrary())
      {
      LPSTR pszCellA = StringToAnsi (pszCell);

      afsclient_TokenGetExisting (pszCellA, (PVOID *)&hCreds, (afs_status_p)&status);

      FreeString (pszCellA, pszCell);
      CloseClientLibrary();
      }

   if (!hCreds && pStatus)
      *pStatus = status;
   return hCreds;
}


UINT_PTR AfsAppLib_SetCredentials (LPCTSTR pszCell, LPCTSTR pszUser, LPCTSTR pszPassword, ULONG *pStatus)
{
   UINT_PTR hCreds = 0;
   ULONG status = 0;

   UINT_PTR idClient;
   if ((idClient = AfsAppLib_GetAdminServerClientID()) != 0)
      {
      hCreds = asc_CredentialsSet (idClient, pszCell, pszUser, pszPassword, &status);
      }
   else
       if (OpenClientLibrary())
      {
      char szCellA[ cchRESOURCE ];
      char szUserA[ cchRESOURCE ];
      char szPasswordA[ cchRESOURCE ];
      CopyStringToAnsi (szCellA, pszCell);
      CopyStringToAnsi (szUserA, pszUser);
      CopyStringToAnsi (szPasswordA, pszPassword);

      afsclient_TokenGetNew (szCellA, szUserA, szPasswordA, (PVOID *)&hCreds, (afs_status_p)&status);

      CloseClientLibrary();
      }

   if (hCreds)
      {
      PostMessage (AfsAppLib_GetMainWindow(), WM_REFRESHED_CREDENTIALS, 0, (LPARAM)hCreds);
      }

   if (!hCreds && pStatus)
      *pStatus = status;
   return hCreds;
}


/*
 * OPEN CELL DIALOG ___________________________________________________________
 *
 */

BOOL AfsAppLib_ShowOpenCellDialog (LPOPENCELLDLG_PARAMS lpp)
{
   HINSTANCE hInst = APP_HINST;
   if (lpp->idd == 0)
      {
      hInst = APPLIB_HINST;
      lpp->idd = IDD_APPLIB_OPENCELL;
      }
   if (lpp->hCreds == 0)
      {
      if (lpp->szCell[0])
         lpp->hCreds = AfsAppLib_GetCredentials (lpp->szCell);
      else
         lpp->hCreds = AfsAppLib_GetCredentials (NULL);
      }

   INT_PTR rc = ModalDialogParam (lpp->idd, lpp->hParent, (DLGPROC)OpenCell_DlgProc, (LPARAM)lpp);

   return (rc == IDOK) ? TRUE : FALSE;
}


HRESULT CALLBACK OpenCell_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   LPOPENCELLDLG_PARAMS lpp;
   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, lp);

   if ((lpp = (LPOPENCELLDLG_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      if (lpp->hookproc)
         {
         if (CallWindowProc ((WNDPROC)lpp->hookproc, hDlg, msg, wp, lp))
            return TRUE;
         }
      }

   if (lpp != NULL)
      {
      if (AfsAppLib_HandleHelp (lpp->idd, hDlg, msg, wp, lp))
         return TRUE;
      }

   switch (msg)
      {
      case WM_INITDIALOG:
         OpenCell_OnInitDialog (hDlg, lpp);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               return TRUE;

            case IDOK:
               if (OpenCell_OnOK (hDlg, lpp))
                  EndDialog (hDlg, IDOK);
               return TRUE;

            case IDC_OPENCELL_CELL:
               switch (HIWORD(wp))
                  {
                  case CBN_SELCHANGE:
                     TCHAR szCell[ cchNAME ];
                     SendDlgItemMessage (hDlg, IDC_OPENCELL_CELL, CB_GETLBTEXT, CB_GetSelected(GetDlgItem (hDlg, IDC_OPENCELL_CELL)), (LPARAM)szCell);
                     SetDlgItemText (hDlg, IDC_OPENCELL_CELL, szCell);
                     OpenCell_OnCell (hDlg);
                     break;

                  case CBN_EDITCHANGE:
                     OpenCell_OnCell (hDlg);
                     break;
                  }
               break;
            }
         break;

      case WM_REFRESHED_CREDENTIALS:
         OpenCell_OnGotCreds (hDlg, lp);
         break;
      }

   return FALSE;
}


void OpenCell_OnInitDialog (HWND hDlg, LPOPENCELLDLG_PARAMS lpp)
{
   // Fix the title of the dialog (unless the caller has supplied a
   // custom dialog template)
   //
   if (lpp && (lpp->idd == IDD_APPLIB_OPENCELL))
      {
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
      }

   // Fill in the 'Cell:' combobox; we'll list the default cell, and any
   // cell which the user has specified before.
   //
   CB_StartChange (GetDlgItem (hDlg, IDC_OPENCELL_CELL), TRUE);

   if (!lpp->lpcl)
      {
      TCHAR szDefCell[ cchNAME ];
      if (AfsAppLib_GetLocalCell (szDefCell) && *szDefCell)
         {
         CB_AddItem (GetDlgItem (hDlg, IDC_OPENCELL_CELL), szDefCell, 1);
         }
      }
   else for (size_t ii = 0; ii < lpp->lpcl->nCells; ++ii)
      {
      CB_AddItem (GetDlgItem (hDlg, IDC_OPENCELL_CELL), lpp->lpcl->aCells[ii], 1+ii);
      }

   CB_EndChange (GetDlgItem (hDlg, IDC_OPENCELL_CELL), 1);

   // Set up the "Credentials" box; if the user needs credentials to edit
   // this default cell, jump the cursor to the appropriate field
   //
   SetDlgItemText (hDlg, IDC_OPENCELL_ID, TEXT("admin"));

   OpenCell_OnCell (hDlg);
}


BOOL OpenCell_OnOK (HWND hDlg, LPOPENCELLDLG_PARAMS lpp)
{
   BOOL rc = FALSE;
   OpenCell_Enable (hDlg, FALSE);
   StartHourGlass ();

   // Remember what cell the user chose to edit
   //
   GetDlgItemText (hDlg, IDC_OPENCELL_CELL, lpp->szCell, cchNAME);

   // Try to obtain the credentials specified by the user.
   //
   TCHAR szCell[ cchNAME ];
   GetDlgItemText (hDlg, IDC_OPENCELL_CELL, szCell, cchNAME);

   TCHAR szUser[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_OPENCELL_ID, szUser, cchNAME);

   TCHAR szPassword[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_OPENCELL_PASSWORD, szPassword, cchNAME);

   ULONG status;
   if ((lpp->hCreds = AfsAppLib_SetCredentials (lpp->szCell, szUser, szPassword, &status)) == NULL)
      {
      ErrorDialog (status, IDS_ERROR_BAD_CREDENTIALS);
      }
   else
      {
      // See if those credentials are sufficient
      //
      CHECKCREDS_PARAMS pp;
      memset (&pp, 0x00, sizeof(pp));
      memcpy (&pp.bcdp, &lpp->bcdp, sizeof(BADCREDSDLG_PARAMS));
      pp.bcdp.hParent = hDlg;
      pp.hCreds = lpp->hCreds;
      pp.fShowWarning = TRUE;

      if ((rc = AfsAppLib_CheckCredentials (&pp)) == FALSE)
         {
         SetDlgItemText (hDlg, IDC_OPENCELL_ID, TEXT("admin"));
         PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg,IDC_OPENCELL_PASSWORD), TRUE);
         }
      }

   if (!rc)
      OpenCell_Enable (hDlg, TRUE);
   StopHourGlass ();
   return rc;
}


typedef struct
   {
   HWND hDlg;
   TCHAR szCell[ cchNAME ];
   TCHAR szUser[ cchNAME ];
   SYSTEMTIME stExpire;
   BOOL fGotCreds;
   BOOL fValidCreds;
   } OPENCELL_ONCELL_PARAMS, *LPOPENCELL_ONCELL_PARAMS;

DWORD WINAPI OpenCell_OnCell_ThreadProc (PVOID lp)
{
   LPOPENCELL_ONCELL_PARAMS lpp;
   if ((lpp = (LPOPENCELL_ONCELL_PARAMS)lp) != NULL)
      {
      UINT_PTR hCreds = AfsAppLib_GetCredentials (lpp->szCell);
      lpp->fGotCreds = AfsAppLib_CrackCredentials (hCreds, lpp->szCell, lpp->szUser, &lpp->stExpire)?TRUE:FALSE;
      lpp->fValidCreds = FALSE;

      if (lpp->fGotCreds && AfsAppLib_IsTimeInFuture (&lpp->stExpire))
         {
         CHECKCREDS_PARAMS pp;
         memset (&pp, 0x00, sizeof(pp));
         pp.hCreds = hCreds;
         lpp->fValidCreds = AfsAppLib_CheckCredentials(&pp);
         }

      // Post our results; we'll return the same packet we got.
      //
      if (IsWindow (lpp->hDlg))
         PostMessage (lpp->hDlg, WM_REFRESHED_CREDENTIALS, 0, (LPARAM)lpp);
      else
         Delete (lpp);
      }

   return 0;
}


void OpenCell_OnCell (HWND hDlg)
{
   // Fire up a background thread to query our current credentials in the
   // selected cell.
   //
   LPOPENCELL_ONCELL_PARAMS lpp = New (OPENCELL_ONCELL_PARAMS);
   memset (lpp, 0x00, sizeof(lpp));
   GetDlgItemText (hDlg, IDC_OPENCELL_CELL, lpp->szCell, cchNAME);
   lpp->hDlg = hDlg;

   DWORD dwThreadID;
   CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)OpenCell_OnCell_ThreadProc, lpp, 0, &dwThreadID);
}


void OpenCell_OnGotCreds (HWND hDlg, LPARAM lp)
{
   LPOPENCELL_ONCELL_PARAMS lpp;
   if ((lpp = (LPOPENCELL_ONCELL_PARAMS)lp) != NULL)
      {
      // Don't do anything to the dialog if the user has chosen a different
      // cell than that for which we just queried credentials.
      //
      TCHAR szCell[ cchNAME ];
      GetDlgItemText (hDlg, IDC_OPENCELL_CELL, szCell, cchNAME);

      if (!lstrcmpi (szCell, lpp->szCell))
         {
         TCHAR szPrevCreds[ cchRESOURCE ];
         GetDlgItemText (hDlg, IDC_OPENCELL_OLDCREDS, szPrevCreds, cchRESOURCE);

         if (!lpp->fGotCreds)
            {
            LPTSTR psz = FormatString (IDS_CREDS_NONE, TEXT("%s"), lpp->szCell);
            SetDlgItemText (hDlg, IDC_OPENCELL_OLDCREDS, psz);
            FreeString (psz);
            }
         else if (!AfsAppLib_IsTimeInFuture (&lpp->stExpire))
            {
            LPTSTR pszCreds = FormatString (IDS_CREDS_EXPIRED, TEXT("%s%s%t"), lpp->szCell, lpp->szUser, &lpp->stExpire);
            SetDlgItemText (hDlg, IDC_OPENCELL_OLDCREDS, pszCreds);
            FreeString (pszCreds);
            }
         else
            {
            LPTSTR pszCreds = FormatString (IDS_CREDS_VALID, TEXT("%s%s%t"), lpp->szCell, lpp->szUser, &lpp->stExpire);
            SetDlgItemText (hDlg, IDC_OPENCELL_OLDCREDS, pszCreds);
            FreeString (pszCreds);
            }

         SetDlgItemText (hDlg, IDC_OPENCELL_ID, (lpp->fGotCreds) ? lpp->szUser : TEXT("admin"));

         if (!lpp->fValidCreds && !szPrevCreds[0])
            PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg,IDC_OPENCELL_PASSWORD), TRUE);
         }

      Delete (lpp);
      }
}


void OpenCell_Enable (HWND hDlg, BOOL fEnable)
{
   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDCANCEL), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDHELP), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_OPENCELL_CELL), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_OPENCELL_ID), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_OPENCELL_PASSWORD), fEnable);
}


/*
 * NEW CREDENTIALS DIALOG _____________________________________________________
 *
 */

BOOL AfsAppLib_ShowCredentialsDialog (LPCREDENTIALSDLG_PARAMS lpp)
{
   HINSTANCE hInst = APP_HINST;
   if (lpp->idd == 0)
      {
      hInst = APPLIB_HINST;
      lpp->idd = IDD_APPLIB_CREDENTIALS;
      }
   if (lpp->hCreds == 0)
      {
      if (lpp->szCell[0])
         lpp->hCreds = AfsAppLib_GetCredentials (lpp->szCell);
      else
         lpp->hCreds = AfsAppLib_GetCredentials (NULL);
      }

   INT_PTR rc = ModalDialogParam (lpp->idd, lpp->hParent, (DLGPROC)NewCreds_DlgProc, (LPARAM)lpp);

   return (rc == IDOK) ? TRUE : FALSE;
}


HRESULT CALLBACK NewCreds_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   LPCREDENTIALSDLG_PARAMS lpp;
   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, lp);

   if ((lpp = (LPCREDENTIALSDLG_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      if (lpp->hookproc)
         {
         if (CallWindowProc ((WNDPROC)lpp->hookproc, hDlg, msg, wp, lp))
            return TRUE;
         }
      }

   if (lpp != NULL)
      {
      if (AfsAppLib_HandleHelp (lpp->idd, hDlg, msg, wp, lp))
         return TRUE;
      }

   switch (msg)
      {
      case WM_INITDIALOG:
         NewCreds_OnInitDialog (hDlg, lpp);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               return TRUE;

            case IDOK:
                              if (NewCreds_OnOK (hDlg, lpp))
                  EndDialog (hDlg, IDOK);
               return TRUE;

            case IDC_CREDS_LOGIN:
               NewCreds_OnLogin (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void NewCreds_OnInitDialog (HWND hDlg, LPCREDENTIALSDLG_PARAMS lpp)
{
   // Fix the title of the dialog (unless the caller has supplied a
   // custom dialog template)
   //
   if (lpp && (lpp->idd == IDD_APPLIB_CREDENTIALS))
      {
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
      }

   // Set up the "Credentials" boxes
   //
   TCHAR szUser[ cchRESOURCE ];
   SYSTEMTIME st;

   BOOL fValidCreds = TRUE;
   BOOL fShowCreds = TRUE;
   CHECKCREDS_PARAMS pp;
   memset (&pp, 0x00, sizeof(pp));
   pp.hCreds = lpp->hCreds;
   pp.fShowWarning = FALSE;
   memcpy (&pp.bcdp, &lpp->bcdp, sizeof(BADCREDSDLG_PARAMS));
   pp.bcdp.hParent = hDlg;

   if (!AfsAppLib_CrackCredentials (lpp->hCreds, lpp->szCell, szUser, &st))
      {
      fValidCreds = FALSE;
      fShowCreds = FALSE;
      }
   if (!AfsAppLib_IsTimeInFuture (&st))
      {
      fValidCreds = FALSE;
      fShowCreds = FALSE;
      }
   if (!AfsAppLib_CheckCredentials (&pp))
      {
      fValidCreds = FALSE;
      }

   CheckDlgButton (hDlg, IDC_CREDS_LOGIN, !fValidCreds);
   if (!fValidCreds)
      PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg,IDC_CREDS_PASSWORD), TRUE);

   SetDlgItemText (hDlg, IDC_CREDS_CELL, lpp->szCell);

   if (fShowCreds)
      {
      SetDlgItemText (hDlg, IDC_CREDS_CURRENTID, szUser);

      LPTSTR pszDate = FormatString (TEXT("%1"), TEXT("%t"), &st);
      SetDlgItemText (hDlg, IDC_CREDS_EXPDATE, pszDate);
      FreeString (pszDate);
      }

   SetDlgItemText (hDlg, IDC_CREDS_ID, TEXT("admin"));

   NewCreds_OnLogin (hDlg);
}


BOOL NewCreds_OnOK (HWND hDlg, LPCREDENTIALSDLG_PARAMS lpp)
{
   NewCreds_GetOutParams (hDlg, lpp);
   if (!IsDlgButtonChecked (hDlg, IDC_CREDS_LOGIN))
      return TRUE;

   BOOL rc = FALSE;
   StartHourGlass ();
   NewCreds_Enable (hDlg, FALSE);

   // Try to obtain the credentials specified by the user.
   //
   TCHAR szUser[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_CREDS_ID, szUser, cchNAME);

   TCHAR szPassword[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_CREDS_PASSWORD, szPassword, cchNAME);

   ULONG status;
   if ((lpp->hCreds = AfsAppLib_SetCredentials (lpp->szCell, szUser, szPassword, &status)) == NULL)
      {
      ErrorDialog (status, IDS_ERROR_BAD_CREDENTIALS);
      }
   else
      {
      // See if those credentials are sufficient
      //
      CHECKCREDS_PARAMS pp;
      memset (&pp, 0x00, sizeof(pp));
      memcpy (&pp.bcdp, &lpp->bcdp, sizeof(BADCREDSDLG_PARAMS));
      pp.bcdp.hParent = hDlg;
      pp.hCreds = lpp->hCreds;
      pp.fShowWarning = TRUE;

      if ((rc = AfsAppLib_CheckCredentials (&pp)) == FALSE)
         {
         SetDlgItemText (hDlg, IDC_CREDS_ID, TEXT("admin"));
         PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg,IDC_CREDS_PASSWORD), TRUE);
         }
      }

   if (!rc)
      NewCreds_Enable (hDlg, TRUE);
   StopHourGlass ();
   return rc;
}


void NewCreds_OnLogin (HWND hDlg)
{
   BOOL fEnable = IsDlgButtonChecked (hDlg, IDC_CREDS_LOGIN);

   EnableWindow (GetDlgItem (hDlg, IDC_CREDS_ID),       fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_CREDS_PASSWORD), fEnable);
}


void NewCreds_Enable (HWND hDlg, BOOL fEnable)
{
   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDCANCEL), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDHELP), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_CREDS_LOGIN), fEnable);

   if (fEnable)
      NewCreds_OnLogin (hDlg);
   else
      {
      EnableWindow (GetDlgItem (hDlg, IDC_CREDS_ID),       fEnable);
      EnableWindow (GetDlgItem (hDlg, IDC_CREDS_PASSWORD), fEnable);
      }
}


void NewCreds_GetOutParams (HWND hDlg, LPCREDENTIALSDLG_PARAMS lpp)
{
   if (!IsDlgButtonChecked (hDlg, IDC_CREDS_LOGIN))
      {
      lpp->szIdentity[0] = TEXT('\0');
      lpp->szPassword[0] = TEXT('\0');
      }
   else
      {
      GetDlgItemText (hDlg, IDC_CREDS_ID, lpp->szIdentity, cchNAME);
      GetDlgItemText (hDlg, IDC_CREDS_PASSWORD, lpp->szPassword, cchNAME);
      }
}


/*
 * TEST CREDENTIALS ___________________________________________________________
 *
 */


BOOL AfsAppLib_IsUserAdmin (UINT_PTR hCreds, LPTSTR pszUser)
{
#ifndef USE_KASERVER
    return TRUE;
#else
   BOOL rc = FALSE;
   afs_status_t status;

   UINT_PTR idClient;
   if ((idClient = AfsAppLib_GetAdminServerClientID()) != 0)
      {
      TCHAR szCell[ cchRESOURCE ];
      TCHAR szUser[ cchRESOURCE ];
      SYSTEMTIME stExpire;
      if (asc_CredentialsCrack (idClient, hCreds, szCell, szUser, &stExpire, (ULONG*)&status))
         {
         ASID idCell;
         if (asc_CellOpen (idClient, hCreds, szCell, AFSADMSVR_SCOPE_USERS, &idCell, (ULONG*)&status))
            {
            ASID idUser;
            if (asc_ObjectFind (idClient, idCell, TYPE_USER, pszUser, &idUser, (ULONG*)&status))
               {
               ASOBJPROP Info;
               if (asc_ObjectPropertiesGet (idClient, GET_ALL_DATA, idCell, idUser, &Info, (ULONG*)&status))
                  {
                  if (Info.u.UserProperties.fHaveKasInfo)
                     {
                     rc = Info.u.UserProperties.KASINFO.fIsAdmin;
                     }
                  }
               }
            asc_CellClose (idClient, idCell, (ULONG*)&status);
            }
         }
      }
   else if (OpenClientLibrary())
      {
      if (OpenKasLibrary())
         {
         char szUserA[ cchRESOURCE ], szUser2A[ cchRESOURCE ];
         char szCellA[ cchRESOURCE ];
         unsigned long dateExpire;
         int fHasKasToken;

         if (afsclient_TokenQuery (hCreds, &dateExpire, szUserA, szUser2A, szCellA, &fHasKasToken, (afs_status_p)&status))
            {
            PVOID hCell;
            if (afsclient_CellOpen (szCellA, hCreds, &hCell, &status))
               {
               kas_identity_t Identity;
               memset (&Identity, 0x00, sizeof(Identity));
               CopyStringToAnsi (Identity.principal, pszUser);

               kas_principalEntry_t Entry;
               if (kas_PrincipalGet (hCell, NULL, &Identity, &Entry, &status))
                  {
                  if (Entry.adminSetting == KAS_ADMIN)
                     rc = TRUE;
                  }

               afsclient_CellClose (hCell, (afs_status_p)&status);
               }
            }

         CloseKasLibrary();
         }

      CloseClientLibrary();
      }

   return rc;
#endif /* USE_KASERVER */
}


typedef struct
   {
   BADCREDSDLG_PARAMS bcdp;
   LPTSTR pszFullText;
   PVOID hCreds;
   } REALBADCREDSDLG_PARAMS, *LPREALBADCREDSDLG_PARAMS;

BOOL AfsAppLib_CheckCredentials (LPCHECKCREDS_PARAMS lpp)
{
   BOOL fCredsOK = TRUE;
   int idsWarning = IDS_BADCREDS_DESC_GENERAL;

   TCHAR szCell[ cchNAME ];
   TCHAR szUser[ cchNAME ];
   SYSTEMTIME stExpire;

   if (!AfsAppLib_CrackCredentials (lpp->hCreds, szCell, szUser, &stExpire))
      {
      fCredsOK = FALSE;
      }
   else if (!AfsAppLib_IsUserAdmin (lpp->hCreds, szUser))
      {
      fCredsOK = FALSE;
      idsWarning = IDS_BADCREDS_DESC_BADCHOICE;
      }
   else if (!AfsAppLib_IsTimeInFuture (&stExpire))
      {
      fCredsOK = FALSE;
      idsWarning = IDS_BADCREDS_DESC_EXPIRED;
      }

   if (!fCredsOK && lpp->fShowWarning)
      {
      if (lpp->bcdp.pfShowWarningEver && !(*lpp->bcdp.pfShowWarningEver))
         {
         fCredsOK = TRUE; // user says always ignore bad credentials.
         }
      else
         {
         if (!szCell[0])
            AfsAppLib_GetLocalCell (szCell);

         int idsDesc = (lpp->bcdp.idsDesc) ? lpp->bcdp.idsDesc : IDS_BADCREDS_DESC2;
         LPTSTR pszDesc = FormatString (idsDesc, TEXT("%s"), szCell);
         LPTSTR pszFullText = FormatString (idsWarning, TEXT("%s%s%m"), szCell, pszDesc, IDS_BADCREDS_DESC3);
         FreeString (pszDesc);

         REALBADCREDSDLG_PARAMS pp;
         memset (&pp, 0x00, sizeof(pp));
         pp.pszFullText = pszFullText;
         memcpy (&pp.bcdp, &lpp->bcdp, sizeof(BADCREDSDLG_PARAMS));

         HINSTANCE hInst = APP_HINST;
         if (pp.bcdp.idd == 0)
            {
            hInst = APPLIB_HINST;
            pp.bcdp.idd = IDD_APPLIB_BADCREDS;
            }

         INT_PTR rc = ModalDialogParam (pp.bcdp.idd, pp.bcdp.hParent, (DLGPROC)BadCreds_DlgProc, (LPARAM)&pp);
         if (rc == IDCANCEL)
            {
            fCredsOK = TRUE; // user says ignore bad credentials this time.
            }

         FreeString (pszFullText);
         }
      }

   return fCredsOK;
}


HRESULT CALLBACK BadCreds_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   LPREALBADCREDSDLG_PARAMS lpp;
   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, lp);
   if ((lpp = (LPREALBADCREDSDLG_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL) 
      {
      if (lpp->bcdp.hookproc)
         {
         if (CallWindowProc ((WNDPROC)lpp->bcdp.hookproc, hDlg, msg, wp, lp))
            return TRUE;
         }
      if (AfsAppLib_HandleHelp (lpp->bcdp.idd, hDlg, msg, wp, lp))
         return TRUE;
      }

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
         if (!lpp || !lpp->bcdp.pfShowWarningEver)
            DestroyWindow (GetWindow (hDlg, IDC_BADCREDS_SHUTUP));

         SetDlgItemText (hDlg, IDC_BADCREDS_DESC, lpp->pszFullText);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               if (lpp && lpp->bcdp.pfShowWarningEver)
                  *lpp->bcdp.pfShowWarningEver = !IsDlgButtonChecked (hDlg, IDC_BADCREDS_SHUTUP);
               EndDialog (hDlg, LOWORD(wp));
               break;
            }
         break;
      }

   return FALSE;
}


/*
 * EXPIRED CREDENTIALS ________________________________________________________
 *
 */

void AfsAppLib_CheckForExpiredCredentials (LPCREDENTIALSDLG_PARAMS lpp)
{
   static UINT_PTR hCredsPrevious = 0;
   static BOOL fHadGoodCredentials;

   TCHAR szCell[ cchNAME ];
   TCHAR szUser[ cchNAME ];
   SYSTEMTIME stExpire;

   BOOL fHaveCredentials;
   fHaveCredentials = AfsAppLib_CrackCredentials (lpp->hCreds, szCell, szUser, &stExpire)?TRUE:FALSE;

   if (hCredsPrevious && (hCredsPrevious != lpp->hCreds))
      {
      if (!fHaveCredentials)
         fHadGoodCredentials = FALSE;
      else if (!AfsAppLib_IsTimeInFuture (&stExpire))
         fHadGoodCredentials = FALSE;
      else
         fHadGoodCredentials = TRUE;
      hCredsPrevious = lpp->hCreds;
      }
   else if (fHaveCredentials && AfsAppLib_IsTimeInFuture (&stExpire))
      {
      fHadGoodCredentials = TRUE;
      }
   else if (fHadGoodCredentials) // used to have good credentials, but now don't
      {
      fHadGoodCredentials = FALSE;

      LPCREDENTIALSDLG_PARAMS lppCopy = New (CREDENTIALSDLG_PARAMS);
      memcpy (lppCopy, lpp, sizeof(CREDENTIALSDLG_PARAMS));

      if (!AfsAppLib_GetMainWindow())
         OnExpiredCredentials ((WPARAM)(!AfsAppLib_IsTimeInFuture(&stExpire)), (LPARAM)lppCopy);
      else
         PostMessage (AfsAppLib_GetMainWindow(), WM_EXPIRED_CREDENTIALS, (WPARAM)(!AfsAppLib_IsTimeInFuture(&stExpire)), (LPARAM)lppCopy);
      }
}


void OnExpiredCredentials (WPARAM wp, LPARAM lp)
{
   BOOL fExpired = (BOOL)wp;
   LPCREDENTIALSDLG_PARAMS lpp = (LPCREDENTIALSDLG_PARAMS)lp;

   if (lpp && lpp->bcdp.pfShowWarningEver && *(lpp->bcdp.pfShowWarningEver))
      {
      int idsWarning = (wp) ? IDS_BADCREDS_DESC_EXPIRED : IDS_BADCREDS_DESC_DESTROYED;

      int idsDesc = (lpp->bcdp.idsDesc) ? lpp->bcdp.idsDesc : IDS_BADCREDS_DESC2;
      LPTSTR pszDesc = FormatString (idsDesc, TEXT("%s"), lpp->szCell);
      LPTSTR pszFullText = FormatString (idsWarning, TEXT("%s%s%m"), lpp->szCell, pszDesc, IDS_BADCREDS_DESC3);
      FreeString (pszDesc);

      REALBADCREDSDLG_PARAMS pp;
      memset (&pp, 0x00, sizeof(pp));
      pp.pszFullText = pszFullText;
      memcpy (&pp.bcdp, &lpp->bcdp, sizeof(BADCREDSDLG_PARAMS));

      if (ModalDialogParam (IDD_APPLIB_BADCREDS, NULL, (DLGPROC)BadCreds_DlgProc, (LPARAM)&pp) != IDCANCEL)
         {
         AfsAppLib_ShowCredentialsDialog (lpp);
         }

      FreeString (pszFullText);
      }

   if (lpp)
      {
      Delete (lpp);
      }
}


