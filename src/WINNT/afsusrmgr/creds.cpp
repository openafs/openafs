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

#include "TaAfsUsrMgr.h"
#include "creds.h"


/*
 * OPENCELL DIALOG ____________________________________________________________
 *
 */

void OpenCell_Hook_Enable (HWND hDlg, BOOL fEnable)
{
   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDCANCEL), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDHELP), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_OPENCELL_CELL), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_OPENCELL_ID), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_OPENCELL_PASSWORD), fEnable);
}


void OpenCell_Hook_OnEndTask_OpenCell (HWND hDlg, LPTASKPACKET ptp)
{
   if (ptp->rc)
      {
      EndDialog (hDlg, IDOK);
      }
   else
      {
      OpenCell_Hook_Enable (hDlg, TRUE);

      TCHAR szCell[ cchNAME ];
      GetDlgItemText (hDlg, IDC_OPENCELL_CELL, szCell, cchNAME);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_OPEN_CELL, TEXT("%s"), szCell);
      }
}


void OpenCell_Hook_OnOK (HWND hDlg, LPOPENCELLDLG_PARAMS lpp)
{
   BOOL rc = FALSE;
   OpenCell_Hook_Enable (hDlg, FALSE);
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

   if (rc)
      {
      g.hCreds = lpp->hCreds;

      // Instead of closing the dialog, start an taskOPENCELL task;
      // we'll wait for that to complete successfully before we'll
      // close the dialog.
      //
      LPOPENCELL_PARAMS lpocp = New (OPENCELL_PARAMS);
      memset (lpocp, 0x00, sizeof(OPENCELL_PARAMS));
      lstrcpy (lpocp->szCell, lpp->szCell);
      lpocp->fCloseAppOnFail = FALSE;
      lpocp->hCreds = lpp->hCreds;
      StartTask (taskOPENCELL, hDlg, lpocp);
      }

   if (!rc)
      OpenCell_Hook_Enable (hDlg, TRUE);
   StopHourGlass ();
}


BOOL CALLBACK OpenCell_Hook (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskOPENCELL)
               OpenCell_Hook_OnEndTask_OpenCell (hDlg, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               OpenCell_Hook_OnOK (hDlg, (LPOPENCELLDLG_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER));
               return TRUE;
            }
         break;
      }

   return FALSE;
}


/*
 * CREDENTIALS ________________________________________________________________
 *
 */

void GetBadCredsDlgParams (LPBADCREDSDLG_PARAMS lpp)
{
   memset (lpp, 0x00, sizeof(BADCREDSDLG_PARAMS));
   lpp->pfShowWarningEver = &gr.fWarnBadCreds;
   lpp->idsDesc = IDS_BADCREDS_DESC;
}


void GetCredentialsDlgParams (LPCREDENTIALSDLG_PARAMS lpp)
{
   memset (lpp, 0x00, sizeof(CREDENTIALSDLG_PARAMS));
   lpp->hParent = g.hMain;
   lpp->hCreds = g.hCreds;
   if (g.idCell)
      asc_CellNameGet_Fast (g.idClient, g.idCell, lpp->szCell);
   else
      AfsAppLib_GetLocalCell (lpp->szCell);
   GetBadCredsDlgParams (&lpp->bcdp);
}


/*
 * OPERATIONS _________________________________________________________________
 *
 */

BOOL OpenCellDialog (void)
{
   OPENCELLDLG_PARAMS pp;
   memset (&pp, 0x00, sizeof(pp));
   pp.hookproc = (DLGPROC)OpenCell_Hook;
   pp.hParent = (IsWindowVisible (g.hMain)) ? g.hMain : NULL;
   pp.idsDesc = 0;
   pp.hCreds = g.hCreds;
   pp.lpcl = AfsAppLib_GetCellList (HKCU, REGSTR_SETTINGS_CELLS);
   GetBadCredsDlgParams (&pp.bcdp);

   BOOL rc = AfsAppLib_ShowOpenCellDialog (&pp);

   AfsAppLib_FreeCellList (pp.lpcl);
   return rc;
}


BOOL NewCredsDialog (void)
{
   CREDENTIALSDLG_PARAMS pp;
   GetCredentialsDlgParams (&pp);
   return AfsAppLib_ShowCredentialsDialog (&pp);
}


void CheckForExpiredCredentials (void)
{
   CREDENTIALSDLG_PARAMS pp;
   GetCredentialsDlgParams (&pp);
   AfsAppLib_CheckForExpiredCredentials (&pp);
}


BOOL CheckCredentials (BOOL fComplain)
{
   CHECKCREDS_PARAMS pp;
   memset (&pp, 0x00, sizeof(pp));
   pp.fShowWarning = fComplain;
   pp.hCreds = g.hCreds;
   GetBadCredsDlgParams (&pp.bcdp);

   return AfsAppLib_CheckCredentials (&pp);
}


void ShowCurrentCredentials (void)
{
   int idsCreds = IDS_CRED_NONE;
   TCHAR szCell[ cchNAME ];
   TCHAR szUser[ cchNAME ];
   SYSTEMTIME stExpire;

   if (g.hCreds)
      {
      if (AfsAppLib_CrackCredentials (g.hCreds, szCell, szUser, &stExpire))
         {
         if (AfsAppLib_IsTimeInFuture (&stExpire))
            idsCreds = IDS_CRED_OK;
         else
            idsCreds = IDS_CRED_EXP;
         }
      }

   LPTSTR pszCreds = FormatString (idsCreds, TEXT("%s%t"), szUser, &stExpire);
   SetDlgItemText (g.hMain, IDC_CREDS, pszCreds);
   FreeString (pszCreds);
}

