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

#include "svrmgr.h"
#include "set_clone.h"
#include "propcache.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Filesets_Clone_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Filesets_Clone_OnInitDialog (HWND hDlg, LPSET_CLONESYS_PARAMS pcsp);

BOOL CALLBACK Filesets_Clonesys_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Filesets_Clonesys_OnInitDialog (HWND hDlg, LPSET_CLONESYS_PARAMS pcsp);
void Filesets_Clonesys_OnOK (HWND hDlg, LPSET_CLONESYS_PARAMS pcsp);
void Filesets_Clonesys_OnSelect (HWND hDlg, LPSET_CLONESYS_PARAMS pcsp);
void Filesets_Clonesys_OnSelectServer (HWND hDlg, LPSET_CLONESYS_PARAMS pcsp);
void Filesets_Clonesys_OnEndTask_EnumServers (HWND hDlg, LPSET_CLONESYS_PARAMS pcsp);
void Filesets_Clonesys_OnEndTask_EnumAggregs (HWND hDlg, LPSET_CLONESYS_PARAMS pcsp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Filesets_Clone (LPIDENT lpi)
{
   LPSET_CLONESYS_PARAMS pcsp = New (SET_CLONESYS_PARAMS);
   memset (pcsp, 0x00, sizeof(SET_CLONESYS_PARAMS));
   pcsp->lpi = (lpi == NULL) ? g.lpiCell : lpi;

   int rc;
   if (pcsp->lpi && pcsp->lpi->fIsFileset())
      rc = ModalDialogParam (IDD_SET_CLONE, NULL, (DLGPROC)Filesets_Clone_DlgProc, (LPARAM)pcsp);
   else
      rc = ModalDialogParam (IDD_SET_CLONESYS, NULL, (DLGPROC)Filesets_Clonesys_DlgProc, (LPARAM)pcsp);

   if ((rc != IDOK) || (!pcsp->lpi))
      {
      Delete (pcsp);
      }
   else if (pcsp->lpi->fIsFileset()) // clone one fileset?
      {
      StartTask (taskSET_CLONE, NULL, pcsp->lpi);
      Delete (pcsp);
      }
   else // or clone lots of filesets?
      {
      StartTask (taskSET_CLONESYS, NULL, pcsp);
      // don't delete pcsp--it'll be freed by the StartTask handler
      }
}



BOOL CALLBACK Filesets_Clone_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SET_CLONE, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLong (hDlg, DWL_USER, lp);

   LPSET_CLONESYS_PARAMS pcsp = (LPSET_CLONESYS_PARAMS)GetWindowLong (hDlg, DWL_USER);

   if (pcsp != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            PropCache_Add (pcSET_CLONE, NULL, hDlg);
            Filesets_Clone_OnInitDialog (hDlg, pcsp);
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

         case WM_DESTROY:
            PropCache_Delete (pcSET_CLONE, NULL);
            break;
         }
      }

   return FALSE;
}


void Filesets_Clone_OnInitDialog (HWND hDlg, LPSET_CLONESYS_PARAMS pcsp)
{
   TCHAR szServer[ cchNAME ];
   TCHAR szAggregate[ cchNAME ];
   TCHAR szFileset[ cchNAME ];
   pcsp->lpi->GetServerName (szServer);
   pcsp->lpi->GetAggregateName (szAggregate);
   pcsp->lpi->GetFilesetName (szFileset);

   TCHAR szText[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_CLONE_DESC, szText, cchRESOURCE);

   LPTSTR pszText = FormatString (szText, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
   SetDlgItemText (hDlg, IDC_CLONE_DESC, pszText);
   FreeString (pszText);
}


BOOL CALLBACK Filesets_Clonesys_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SET_CLONESYS, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLong (hDlg, DWL_USER, lp);

   LPSET_CLONESYS_PARAMS pcsp = (LPSET_CLONESYS_PARAMS)GetWindowLong (hDlg, DWL_USER);

   if (pcsp != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            PropCache_Add (pcSET_CLONE, NULL, hDlg);
            Filesets_Clonesys_OnInitDialog (hDlg, pcsp);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskSVR_ENUM_TO_COMBOBOX)
                  {
                  Filesets_Clonesys_OnEndTask_EnumServers (hDlg, pcsp);
                  }
               else if (ptp->idTask == taskAGG_ENUM_TO_COMBOBOX)
                  {
                  Filesets_Clonesys_OnEndTask_EnumAggregs (hDlg, pcsp);
                  }
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  Filesets_Clonesys_OnOK (hDlg, pcsp);
                  EndDialog (hDlg, IDOK);
                  break;

               case IDCANCEL:
                  EndDialog (hDlg, IDCANCEL);
                  break;

               case IDC_CLONE_ALL:
               case IDC_CLONE_SOME:
               case IDC_CLONE_SVR_LIMIT:
               case IDC_CLONE_AGG_LIMIT:
               case IDC_CLONE_PREFIX_LIMIT:
                  Filesets_Clonesys_OnSelect (hDlg, pcsp);
                  break;

               case IDC_CLONE_SVR:
                  if (HIWORD(wp) == CBN_SELCHANGE)
                     {
                     Filesets_Clonesys_OnSelectServer (hDlg, pcsp);
                     }
                  break;
               }
            break;

         case WM_DESTROY:
            PropCache_Delete (pcSET_CLONE, NULL);
            break;
         }
      }

   return FALSE;
}


void Filesets_Clonesys_OnInitDialog (HWND hDlg, LPSET_CLONESYS_PARAMS pcsp)
{
   CheckDlgButton (hDlg, IDC_CLONE_ALL,   pcsp->lpi->fIsCell());
   CheckDlgButton (hDlg, IDC_CLONE_SOME, !pcsp->lpi->fIsCell());
   CheckDlgButton (hDlg, IDC_CLONE_SVR_LIMIT, !pcsp->lpi->fIsCell());
   CheckDlgButton (hDlg, IDC_CLONE_AGG_LIMIT, pcsp->lpi->fIsAggregate());

   pcsp->fEnumedServers = FALSE;
   pcsp->fEnumedAggregs = FALSE;
   Filesets_Clonesys_OnSelect (hDlg, pcsp);

   LPSVR_ENUM_TO_COMBOBOX_PACKET lpp = New (SVR_ENUM_TO_COMBOBOX_PACKET);
   lpp->hCombo = GetDlgItem (hDlg, IDC_CLONE_SVR);
   lpp->lpiSelect = (pcsp->lpi && !pcsp->lpi->fIsCell()) ? pcsp->lpi->GetServer() : NULL;
   StartTask (taskSVR_ENUM_TO_COMBOBOX, hDlg, lpp);
}


void Filesets_Clonesys_OnOK (HWND hDlg, LPSET_CLONESYS_PARAMS pcsp)
{
   if (IsDlgButtonChecked (hDlg, IDC_CLONE_ALL))
      {
      pcsp->lpi = g.lpiCell;
      pcsp->fUsePrefix = FALSE;
      pcsp->szPrefix[0] = TEXT('\0');
      }
   else // (IsDlgButtonChecked (hDlg, IDC_CLONE_SOME))
      {
      if (IsDlgButtonChecked (hDlg, IDC_CLONE_AGG_LIMIT))
         pcsp->lpi = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_CLONE_AGG));
      else if (IsDlgButtonChecked (hDlg, IDC_CLONE_SVR_LIMIT))
         pcsp->lpi = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_CLONE_SVR));
      else
         pcsp->lpi = g.lpiCell;

      pcsp->fUsePrefix = IsDlgButtonChecked (hDlg, IDC_CLONE_PREFIX_LIMIT);
      GetDlgItemText (hDlg, IDC_CLONE_PREFIX, pcsp->szPrefix, MAX_PATH);

      if (pcsp->fUsePrefix && (pcsp->szPrefix[0] == TEXT('!')))
         {
         TCHAR szPrefixNoBang[ cchRESOURCE ];
         lstrcpy (szPrefixNoBang, &pcsp->szPrefix[1]);
         lstrcpy (pcsp->szPrefix, szPrefixNoBang);
         pcsp->fExcludePrefix = TRUE;
         }
      }
}


void Filesets_Clonesys_OnSelect (HWND hDlg, LPSET_CLONESYS_PARAMS pcsp)
{
   BOOL fCloneAll = (IsDlgButtonChecked (hDlg, IDC_CLONE_ALL)) ? TRUE : FALSE;

   EnableWindow (GetDlgItem (hDlg, IDC_CLONE_SVR_LIMIT), !fCloneAll);
   EnableWindow (GetDlgItem (hDlg, IDC_CLONE_AGG_LIMIT), !fCloneAll);
   EnableWindow (GetDlgItem (hDlg, IDC_CLONE_PREFIX_LIMIT), !fCloneAll);

   if (fCloneAll)
      {
      CheckDlgButton (hDlg, IDC_CLONE_SVR_LIMIT, FALSE);
      CheckDlgButton (hDlg, IDC_CLONE_AGG_LIMIT, FALSE);
      CheckDlgButton (hDlg, IDC_CLONE_PREFIX_LIMIT, FALSE);
      }

   BOOL fEnable;

   fEnable = IsDlgButtonChecked (hDlg, IDC_CLONE_SVR_LIMIT) && pcsp->fEnumedServers;
   EnableWindow (GetDlgItem (hDlg, IDC_CLONE_SVR), fEnable);

   fEnable = IsDlgButtonChecked (hDlg, IDC_CLONE_SVR_LIMIT);
   EnableWindow (GetDlgItem (hDlg, IDC_CLONE_AGG_LIMIT), fEnable);
   if (!fEnable)
      CheckDlgButton (hDlg, IDC_CLONE_AGG_LIMIT, FALSE);

   fEnable = fEnable && IsDlgButtonChecked (hDlg, IDC_CLONE_AGG_LIMIT) && pcsp->fEnumedAggregs;
   EnableWindow (GetDlgItem (hDlg, IDC_CLONE_AGG), fEnable);

   fEnable = IsDlgButtonChecked (hDlg, IDC_CLONE_PREFIX_LIMIT);
   EnableWindow (GetDlgItem (hDlg, IDC_CLONE_PREFIX), fEnable);

   fEnable = TRUE;
   if (IsDlgButtonChecked (hDlg, IDC_CLONE_SVR_LIMIT) && !pcsp->fEnumedServers)
      fEnable = FALSE;
   if (IsDlgButtonChecked (hDlg, IDC_CLONE_AGG_LIMIT) && !pcsp->fEnumedAggregs)
      fEnable = FALSE;
   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
}


void Filesets_Clonesys_OnSelectServer (HWND hDlg, LPSET_CLONESYS_PARAMS pcsp)
{
   pcsp->fEnumedAggregs = FALSE;
   Filesets_Clonesys_OnSelect (hDlg, pcsp);

   LPAGG_ENUM_TO_COMBOBOX_PACKET lpp = New (AGG_ENUM_TO_COMBOBOX_PACKET);
   lpp->hCombo = GetDlgItem (hDlg, IDC_CLONE_AGG);
   lpp->lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_CLONE_SVR));
   lpp->lpiSelect = (pcsp->lpi->fIsAggregate() && (pcsp->lpi->GetServer() == lpp->lpiServer)) ? pcsp->lpi : NULL;
   StartTask (taskAGG_ENUM_TO_COMBOBOX, hDlg, lpp);
}


void Filesets_Clonesys_OnEndTask_EnumServers (HWND hDlg, LPSET_CLONESYS_PARAMS pcsp)
{
   pcsp->fEnumedServers = TRUE;
   Filesets_Clonesys_OnSelectServer (hDlg, pcsp);
}


void Filesets_Clonesys_OnEndTask_EnumAggregs (HWND hDlg, LPSET_CLONESYS_PARAMS pcsp)
{
   pcsp->fEnumedAggregs = TRUE;
   Filesets_Clonesys_OnSelect (hDlg, pcsp);
}

