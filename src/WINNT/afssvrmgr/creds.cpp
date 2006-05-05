/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "svrmgr.h"
#include "creds.h"
#include "display.h"
#include "time.h"
#include "subset.h"

#include <afs\afskfw.h>

/*
 * OPENCELL DIALOG ____________________________________________________________
 *
 */

void OpenCell_OnSubset (HWND hDlg)
{
   BOOL fEnable;

   fEnable = TRUE;
   if (!IsWindowEnabled (GetDlgItem (hDlg, IDC_MON_ONE)))
      fEnable = FALSE;
   if (!IsDlgButtonChecked (hDlg, IDC_MON_ONE))
      fEnable = FALSE;
   EnableWindow (GetDlgItem (hDlg, IDC_MON_SERVER), fEnable);

   fEnable = TRUE;
   if (!IsWindowEnabled (GetDlgItem (hDlg, IDC_MON_SOME)))
      fEnable = FALSE;
   if (!IsDlgButtonChecked (hDlg, IDC_MON_SOME))
      fEnable = FALSE;
   EnableWindow (GetDlgItem (hDlg, IDC_MON_SUBSET), fEnable);
}


void OpenCell_Hook_Enable (HWND hDlg, BOOL fEnable)
{
   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDHELP), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDCANCEL), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_ADVANCED), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_OPENCELL_CELL), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_MON_ALL), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_MON_ONE), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_OPENCELL_ID), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_OPENCELL_PASSWORD), fEnable);

   if (fEnable)
      {
      BOOL fAnySubsets = !!SendDlgItemMessage (hDlg, IDC_MON_SUBSET, CB_GETCOUNT, 0, 0);
      EnableWindow (GetDlgItem (hDlg, IDC_MON_SOME), fAnySubsets);
      }
   else
      {
      EnableWindow (GetDlgItem (hDlg, IDC_MON_SOME), FALSE);
      }

   OpenCell_OnSubset (hDlg);
}


void OpenCellDlg_Hook_OnOK (HWND hDlg, LPOPENCELLDLG_PARAMS lpp)
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

    if ( KFW_is_available() ) {
        // KFW_AFS_get_cred() parses the szNameA field as complete 
        // princial including potentially
        // a different realm then the specified cell name.
        char *Result = NULL;
        
        char szCellA[ 256 ];
        CopyStringToAnsi (szCellA, lpp->szCell);

        char szUserA[ 256 ];
        CopyStringToAnsi (szUserA, szUser);

        char szPasswordA[ 256 ];
        CopyStringToAnsi (szPasswordA, szPassword);

        rc = !KFW_AFS_get_cred(szUserA, szCellA, szPasswordA, 0, NULL, &Result);
        if (rc) {
            if ((lpp->hCreds = AfsAppLib_GetCredentials (lpp->szCell, &status)) == NULL) {
                ErrorDialog (status, IDS_SVR_ERROR_BAD_CREDENTIALS);
            }
        }
    } else {
        if ((lpp->hCreds = AfsAppLib_SetCredentials (lpp->szCell, szUser, szPassword, &status)) == NULL)
        {
            ErrorDialog (status, IDS_SVR_ERROR_BAD_CREDENTIALS);
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

    }

   if (rc)
      {
      g.hCreds = lpp->hCreds;

      // Instead of closing the dialog, start an taskOPENCELL task;
      // we'll wait for that to complete successfully before we'll
      // close the dialog.
      //
      LPOPENCELL_PACKET lpocp = New (OPENCELL_PACKET);
      memset (lpocp, 0x00, sizeof(OPENCELL_PACKET));
      lstrcpy (lpocp->szCell, lpp->szCell);
      lpocp->fCloseAppOnFail = FALSE;
      lpocp->hCreds = lpp->hCreds;

      if (IsDlgButtonChecked (hDlg, IDC_MON_ALL))
         {
         lpocp->sub = NULL;
         }
      else if (IsDlgButtonChecked (hDlg, IDC_MON_ONE))
         {
         TCHAR szServer[ cchNAME ];
         GetDlgItemText (hDlg, IDC_MON_SERVER, szServer, cchNAME);
         lpocp->sub = New (SUBSET);
         memset (lpocp->sub, 0x0, sizeof(SUBSET));
         if (szServer[0])
            FormatMultiString (&lpocp->sub->pszMonitored, TRUE, TEXT("%1"), TEXT("%s"), szServer);
         else
            {
            lpocp->sub->pszMonitored = AllocateString (2);
            lpocp->sub->pszMonitored[0] = TEXT('\0');
            lpocp->sub->pszMonitored[1] = TEXT('\0');
            }
         }
      else // (IsDlgButtonChecked (hDlg, IDC_MON_SOME))
         {
         TCHAR szSubset[ cchNAME ];
         GetDlgItemText (hDlg, IDC_MON_SUBSET, szSubset, cchNAME);
         lpocp->sub = Subsets_LoadSubset (szCell, szSubset);
         }

      StartTask (taskOPENCELL, hDlg, lpocp);
      }

   if (!rc)
      OpenCell_Hook_Enable (hDlg, TRUE);
   StopHourGlass ();
}


void OpenCell_OnCellChange (HWND hDlg, BOOL fMoveCaret)
{
   HWND hCombo = GetDlgItem (hDlg, IDC_MON_SUBSET);
   CB_StartChange (hCombo, TRUE);
   size_t iSel = 0;
   BOOL fAddedAny = FALSE;

   TCHAR szCell[ cchNAME ];
   GetDlgItemText (hDlg, IDC_OPENCELL_CELL, szCell, cchNAME);

   if (szCell[0] != TEXT('\0'))
      {
      BOOL fSelectedCurrentCell = FALSE;
      if (g.lpiCell)
         {
         TCHAR szCurrentCell[ cchNAME ];
         g.lpiCell->GetCellName (szCurrentCell);

         if (!lstrcmpi (szCell, szCurrentCell))
            fSelectedCurrentCell = TRUE;

         if (g.sub && g.sub->pszMonitored)
            SetDlgItemText (hDlg, IDC_MON_SERVER, g.sub->pszMonitored);
         else
            SetDlgItemText (hDlg, IDC_MON_SERVER, TEXT(""));
         }

      TCHAR szSubset[ cchNAME ];
      for (size_t iIndex = 0; Subsets_EnumSubsets (szCell, iIndex, szSubset); ++iIndex)
         {
         CB_AddItem (hCombo, szSubset, (LPARAM)iIndex+1);
         fAddedAny = TRUE;

         if (fSelectedCurrentCell && !lstrcmpi (szSubset, g.sub->szSubset))
            iSel = iIndex+1;
         }
      }

   CB_EndChange (hCombo, (LPARAM)(iSel == 0) ? 1 : iSel);

   if (fAddedAny && iSel)
      {
      CheckDlgButton (hDlg, IDC_MON_ALL, FALSE);
      CheckDlgButton (hDlg, IDC_MON_ONE, FALSE);
      CheckDlgButton (hDlg, IDC_MON_SOME, TRUE);
      }
   else if (!fAddedAny && IsDlgButtonChecked (hDlg, IDC_MON_SOME))
      {
      CheckDlgButton (hDlg, IDC_MON_ALL, FALSE);
      CheckDlgButton (hDlg, IDC_MON_SOME, FALSE);
      CheckDlgButton (hDlg, IDC_MON_ONE, TRUE);
      }

   BOOL fEnable = fAddedAny;
   if (!IsWindowEnabled (GetDlgItem (hDlg, IDC_MON_ALL)))
      fEnable = FALSE;
   EnableWindow (GetDlgItem (hDlg, IDC_MON_SOME), fEnable);

   OpenCell_OnSubset (hDlg);
}


void OpenCell_OnAdvanced (HWND hDlg)
{
   HWND hGroup = GetDlgItem (hDlg, IDC_ADVANCED_GROUP);

   RECT rWindow;
   RECT rClient;
   RECT rGroup;

   GetWindowRect (hDlg, &rWindow);
   GetClientRect (hDlg, &rClient);
   GetRectInParent (hGroup, &rGroup);

   if (cyRECT(rClient) <= rGroup.top) // closed now?
      {
      SetWindowPos (hDlg, NULL,
                    0, 0,
                    cxRECT(rWindow),
                    cyRECT(rWindow) + cyRECT(rGroup) + 14,
                    SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

      TCHAR szText[ cchRESOURCE ];
      GetString (szText, IDS_ADVANCEDIN_BUTTON);
      SetDlgItemText (hDlg, IDC_ADVANCED, szText);
      }
   else // open now?
      {
      SetWindowPos (hDlg, NULL,
                    0, 0,
                    cxRECT(rWindow),
                    cyRECT(rWindow) - cyRECT(rGroup) - 14,
                    SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

      TCHAR szText[ cchRESOURCE ];
      GetString (szText, IDS_ADVANCEDOUT_BUTTON);
      SetDlgItemText (hDlg, IDC_ADVANCED, szText);
      }
}


void OpenCellDlg_Hook_OnEndTask_OpenCell (HWND hDlg, LPTASKPACKET ptp)
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


BOOL CALLBACK OpenCellDlg_Hook (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_SHOWWINDOW:
         CheckDlgButton (hDlg, IDC_MON_ALL,  TRUE);
         CheckDlgButton (hDlg, IDC_MON_ONE,  FALSE);
         CheckDlgButton (hDlg, IDC_MON_SOME, FALSE);
         OpenCell_OnSubset (hDlg);
         OpenCell_OnAdvanced (hDlg);
         OpenCell_OnCellChange (hDlg, TRUE);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskOPENCELL)
               OpenCellDlg_Hook_OnEndTask_OpenCell (hDlg, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               OpenCellDlg_Hook_OnOK (hDlg, (LPOPENCELLDLG_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER));
               return TRUE;

            case IDC_OPENCELL_CELL:
               switch (HIWORD(wp))
                  {
                  case CBN_SELCHANGE:
                     TCHAR szCell[ cchNAME ];
                     SendDlgItemMessage (hDlg, IDC_OPENCELL_CELL, CB_GETLBTEXT, CB_GetSelected(GetDlgItem (hDlg, IDC_OPENCELL_CELL)), (LPARAM)szCell);
                     SetDlgItemText (hDlg, IDC_OPENCELL_CELL, szCell);
                     OpenCell_OnCellChange (hDlg, FALSE);
                     break;

                  case CBN_EDITCHANGE:
                     OpenCell_OnCellChange (hDlg, FALSE);
                     break;
                  }
               break;

            case IDC_ADVANCED:
               OpenCell_OnAdvanced (hDlg);
               break;

            case IDC_MON_ALL:
            case IDC_MON_ONE:
            case IDC_MON_SOME:
               OpenCell_OnSubset (hDlg);
               break;
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
   if (g.lpiCell)
      g.lpiCell->GetCellName (lpp->szCell);
   else
      AfsAppLib_GetLocalCell (lpp->szCell);
   lpp->hCreds = g.hCreds;
   GetBadCredsDlgParams (&lpp->bcdp);
}


/*
 * OPERATIONS _________________________________________________________________
 *
 */

BOOL OpenCellDialog (void)
{
   if (!Subsets_SaveIfDirty (g.sub))
      return FALSE;

   OPENCELLDLG_PARAMS pp;
   memset (&pp, 0x00, sizeof(pp));
   pp.idd = IDD_OPENCELL;
   pp.hookproc = (DLGPROC)OpenCellDlg_Hook;
   pp.hParent = g.hMain;
   pp.idsDesc = 0;
   pp.lpcl = AfsAppLib_GetCellList (HKCU, REGSTR_SETTINGS_CELLS);
   pp.hCreds = g.hCreds;
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

