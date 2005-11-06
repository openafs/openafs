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
#include "svr_salvage.h"
#include "propcache.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cxMIN_SALVAGE_R  250 // minimum size of Salvage Results dialog
#define cyMIN_SALVAGE_R  200 // minimum size of Salvage Results dialog


/*
 * RESIZING WINDOWS ___________________________________________________________
 *
 */

rwWindowData awdSalvageResults[] = {
    { IDC_SALVAGE_TITLE,        raSizeX | raRepaint,		0,	0 },
    { IDC_SALVAGE_DETAILS,      raSizeX | raSizeY | raRepaint, 	MAKELONG(cxMIN_SALVAGE_R,cyMIN_SALVAGE_R), 0 },
    { IDOK,                     raMoveX | raMoveY,		0,	0 },
    { idENDLIST, 0,						0,	0 }
 };


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Server_Salvage_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Server_Salvage_OnInitDialog (HWND hDlg, LPIDENT lpi);
void Server_Salvage_OnServer (HWND hDlg, LPIDENT lpi);
void Server_Salvage_OnAggregate (HWND hDlg, LPIDENT lpi);
void Server_Salvage_OnAllAggregates (HWND hDlg);
void Server_Salvage_OnAllFilesets (HWND hDlg);
void Server_Salvage_OnSimultaneous (HWND hDlg);
void Server_Salvage_OnAdvanced (HWND hDlg);
void Server_Salvage_OnOK (HWND hDlg);

void Server_Salvage_OnEndTask_EnumServers (HWND hDlg, LPIDENT lpi, LPTASKPACKET ptp);
void Server_Salvage_OnEndTask_EnumAggregates (HWND hDlg, LPIDENT lpi, LPTASKPACKET ptp);
void Server_Salvage_OnEndTask_EnumFilesets (HWND hDlg, LPIDENT lpi, LPTASKPACKET ptp);

BOOL CALLBACK Server_Salvage_Results_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Server_Salvage_Results_OnInitDialog (HWND hDlg, LPIDENT lpiSalvage);
void Server_Salvage_OnEndTask_Salvage (HWND hDlg, LPIDENT lpiSalvage, LPTASKPACKET ptp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Server_Salvage (LPIDENT lpi)
{
   HWND hCurrent;
   if ((hCurrent = PropCache_Search (pcSVR_SALVAGE, lpi)) != NULL)
      {
      SetFocus (hCurrent);
      }
   else
      {
      HWND hDlg = ModelessDialogParam (IDD_SVR_SALVAGE, NULL, (DLGPROC)Server_Salvage_DlgProc, (LPARAM)lpi);
      ShowWindow (hDlg, SW_SHOW);
      }
}


BOOL CALLBACK Server_Salvage_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_SALVAGE, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, lp);

   LPIDENT lpi;
   if ((lpi = (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            PropCache_Add (pcSVR_SALVAGE, lpi, hDlg);
            Server_Salvage_OnInitDialog (hDlg, lpi);
            Server_Salvage_OnAdvanced (hDlg);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskSVR_ENUM_TO_COMBOBOX)
                  Server_Salvage_OnEndTask_EnumServers (hDlg, lpi, ptp);
               else if (ptp->idTask == taskAGG_ENUM_TO_COMBOBOX)
                  Server_Salvage_OnEndTask_EnumAggregates (hDlg, lpi, ptp);
               else if (ptp->idTask == taskSET_ENUM_TO_COMBOBOX)
                  Server_Salvage_OnEndTask_EnumFilesets (hDlg, lpi, ptp);
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  Server_Salvage_OnOK (hDlg);
               case IDCANCEL:
                  DestroyWindow (hDlg);
                  break;

               case IDC_SERVER:
                  Server_Salvage_OnServer (hDlg, lpi);
                  break;
               case IDC_AGGREGATE:
                  Server_Salvage_OnAggregate (hDlg, lpi);
                  break;
               case IDC_AGGREGATE_ALL:
                  Server_Salvage_OnAllAggregates (hDlg);
                  break;
               case IDC_FILESET_ALL:
                  Server_Salvage_OnAllFilesets (hDlg);
                  break;
               case IDC_SALVAGE_SIMUL:
                  Server_Salvage_OnSimultaneous (hDlg);
                  break;

               case IDC_ADVANCED:
                  Server_Salvage_OnAdvanced (hDlg);
                  break;
               }
            break;

         case WM_DESTROY:
            PropCache_Delete (pcSVR_SALVAGE, lpi);
            break;
         }
      }

   return FALSE;
}


void Server_Salvage_OnAdvanced (HWND hDlg)
{
   HWND hGroup = GetDlgItem (hDlg, IDC_ADVANCED_GROUP);

   RECT rWindow;
   RECT rClient;
   RECT rGroup;

   GetWindowRect (hDlg, &rWindow);
   GetClientRect (hDlg, &rClient);
   GetRectInParent (hGroup, &rGroup);

   if (!IsWindowEnabled (GetDlgItem (hDlg, IDC_SALVAGE_TEMPDIR)))
      {
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_TEMPDIR), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_SIMUL), TRUE);
      Server_Salvage_OnSimultaneous (hDlg);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_READONLY), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_BLOCK), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_FORCE), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_FIXDIRS), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_LOG_FILE), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_LOG_INODES), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_LOG_ROOT), TRUE);

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

      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_TEMPDIR), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_SIMUL), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_NUM), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_READONLY), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_BLOCK), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_FORCE), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_FIXDIRS), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_LOG_FILE), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_LOG_INODES), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_LOG_ROOT), FALSE);
      }
}


void Server_Salvage_OnInitDialog (HWND hDlg, LPIDENT lpi)
{
   CheckDlgButton (hDlg, IDC_AGGREGATE_ALL, (lpi->fIsServer()) ? TRUE : FALSE);
   CheckDlgButton (hDlg, IDC_FILESET_ALL,  (lpi->fIsFileset()) ? FALSE : TRUE);

   EnableWindow (GetDlgItem (hDlg, IDC_SERVER), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_AGGREGATE), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_AGGREGATE_ALL), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_FILESET), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_FILESET_ALL), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDOK), FALSE);

   LPSVR_ENUM_TO_COMBOBOX_PACKET lpp = New (SVR_ENUM_TO_COMBOBOX_PACKET);
   memset (lpp, 0x00, sizeof(SVR_ENUM_TO_COMBOBOX_PACKET));
   lpp->hCombo = GetDlgItem (hDlg, IDC_SERVER);
   lpp->lpiSelect = lpi->GetServer();
   StartTask (taskSVR_ENUM_TO_COMBOBOX, hDlg, lpp);

   CheckDlgButton (hDlg, IDC_SALVAGE_SIMUL, TRUE);
   CreateSpinner (GetDlgItem(hDlg,IDC_SALVAGE_NUM), 10, FALSE, 2, 4, 12);
}


void Server_Salvage_OnServer (HWND hDlg, LPIDENT lpi)
{
   LPIDENT lpiServer;
   if ((lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SERVER))) != NULL)
      {
      LPAGG_ENUM_TO_COMBOBOX_PACKET lpp = New (AGG_ENUM_TO_COMBOBOX_PACKET);
      memset (lpp, 0x00, sizeof(AGG_ENUM_TO_COMBOBOX_PACKET));
      lpp->hCombo = GetDlgItem (hDlg, IDC_AGGREGATE);
      lpp->lpiServer = lpiServer;
      lpp->lpiSelect = (lpi && (!lpi->fIsCell()) && (!lpi->fIsServer()) && (lpiServer == lpi->GetServer())) ? (lpi->GetAggregate()) : NULL;
      StartTask (taskAGG_ENUM_TO_COMBOBOX, hDlg, lpp);
      }
}


void Server_Salvage_OnAggregate (HWND hDlg, LPIDENT lpi)
{
   LPIDENT lpiServer;
   if ((lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SERVER))) != NULL)
      {
      LPIDENT lpiAggregate;
      if ((lpiAggregate = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_AGGREGATE))) != NULL)
         {
         LPSET_ENUM_TO_COMBOBOX_PACKET lpp = New (SET_ENUM_TO_COMBOBOX_PACKET);
         memset (lpp, 0x00, sizeof(SET_ENUM_TO_COMBOBOX_PACKET));
         lpp->hCombo = GetDlgItem (hDlg, IDC_FILESET);
         lpp->lpiServer = lpiServer;
         lpp->lpiAggregate = lpiAggregate;
         lpp->lpiSelect = ((lpiServer == lpi) && (lpi->fIsFileset())) ? (lpi->GetFileset()) : NULL;
         StartTask (taskSET_ENUM_TO_COMBOBOX, hDlg, lpp);
         }
      }
}


void Server_Salvage_OnAllAggregates (HWND hDlg)
{
   BOOL fAllAggs = IsDlgButtonChecked (hDlg, IDC_AGGREGATE_ALL);
   BOOL fAllSets = IsDlgButtonChecked (hDlg, IDC_FILESET_ALL);

   if (fAllAggs)
      {
      CheckDlgButton (hDlg, IDC_FILESET_ALL, TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_AGGREGATE), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_FILESET), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_FILESET_ALL), FALSE);
      }
   else
      {
      EnableWindow (GetDlgItem (hDlg, IDC_AGGREGATE), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_FILESET_ALL), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_FILESET), !fAllSets);
      }
}


void Server_Salvage_OnAllFilesets (HWND hDlg)
{
   BOOL fAllSets = IsDlgButtonChecked (hDlg, IDC_FILESET_ALL);
   EnableWindow (GetDlgItem (hDlg, IDC_FILESET), !fAllSets);
}


void Server_Salvage_OnSimultaneous (HWND hDlg)
{
   BOOL fSimul = IsDlgButtonChecked (hDlg, IDC_SALVAGE_SIMUL);
   EnableWindow (GetDlgItem (hDlg, IDC_SALVAGE_NUM), fSimul);
}


void Server_Salvage_OnOK (HWND hDlg)
{
   // Prepare a taskSVR_SALVAGE packet
   //
   LPSVR_SALVAGE_PARAMS lpp = New (SVR_SALVAGE_PARAMS);
   memset (lpp, 0x00, sizeof(SVR_SALVAGE_PARAMS));
   GetDlgItemText (hDlg, IDC_SALVAGE_TEMPDIR, lpp->szTempDir, MAX_PATH);
   GetDlgItemText (hDlg, IDC_SALVAGE_LOG_FILE, lpp->szLogFile, MAX_PATH);
   lpp->fForce = IsDlgButtonChecked (hDlg, IDC_SALVAGE_FORCE);
   lpp->fReadonly = IsDlgButtonChecked (hDlg, IDC_SALVAGE_READONLY);
   lpp->fLogInodes = IsDlgButtonChecked (hDlg, IDC_SALVAGE_LOG_INODES);
   lpp->fLogRootInodes = IsDlgButtonChecked (hDlg, IDC_SALVAGE_LOG_ROOT);
   lpp->fRebuildDirs = IsDlgButtonChecked (hDlg, IDC_SALVAGE_FIXDIRS);
   lpp->fReadBlocks = IsDlgButtonChecked (hDlg, IDC_SALVAGE_BLOCK);
   if (IsDlgButtonChecked (hDlg, IDC_SALVAGE_SIMUL))
      lpp->nProcesses = (int)SP_GetPos (GetDlgItem (hDlg, IDC_SALVAGE_NUM));

   lpp->lpiSalvage = NULL;
   if (!IsDlgButtonChecked (hDlg, IDC_FILESET_ALL))
      lpp->lpiSalvage = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_FILESET));
   if (!lpp->lpiSalvage && !IsDlgButtonChecked (hDlg, IDC_AGGREGATE_ALL))
      lpp->lpiSalvage = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_AGGREGATE));
   if (!lpp->lpiSalvage) // salvage the whole server
      lpp->lpiSalvage = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SERVER));
   if (!lpp->lpiSalvage)
      Delete (lpp);
   else
      {
      // Create (but don't show) a results dialog for this operation.
      //
      HWND hResults = ModelessDialogParam (IDD_SVR_SALVAGE_RESULTS, NULL, (DLGPROC)Server_Salvage_Results_DlgProc, (LPARAM)(lpp->lpiSalvage));
      AfsAppLib_RegisterModelessDialog (hDlg);

      // Fire up the background task; when it finishes, have the task scheduler
      // send the result packet to that results dialog.
      //
      StartTask (taskSVR_SALVAGE, hResults, lpp);
      }
}


void Server_Salvage_OnEndTask_EnumServers (HWND hDlg, LPIDENT lpi, LPTASKPACKET ptp)
{
   // We'll only fill the Servers list once, and that during initialization.
   // When the filling completes, find out what server is currently selected
   // (it should already be the one the user chose earlier), and fill the
   // aggregates list for it. We won't enable anything yet.
   //
   LPIDENT lpiServer;
   if ((lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SERVER))) != NULL)
      {
      LPAGG_ENUM_TO_COMBOBOX_PACKET lpp = New (AGG_ENUM_TO_COMBOBOX_PACKET);
      memset (lpp, 0x00, sizeof(AGG_ENUM_TO_COMBOBOX_PACKET));
      lpp->hCombo = GetDlgItem (hDlg, IDC_AGGREGATE);
      lpp->lpiServer = lpiServer;
      lpp->lpiSelect = (lpi && (!lpi->fIsCell()) && (!lpi->fIsServer()) && (lpiServer == lpi->GetServer())) ? (lpi->GetAggregate()) : NULL;
      StartTask (taskAGG_ENUM_TO_COMBOBOX, hDlg, lpp);
      }
}


void Server_Salvage_OnEndTask_EnumAggregates (HWND hDlg, LPIDENT lpi, LPTASKPACKET ptp)
{
   // We'll refill the Aggregates list any time the user selected a new server;
   // and whenever it gets filled, we must next fill the filesets list.
   //
   LPIDENT lpiServer;
   if ((lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SERVER))) != NULL)
      {
      LPIDENT lpiAggregate;
      if ((lpiAggregate = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_AGGREGATE))) != NULL)
         {
         LPSET_ENUM_TO_COMBOBOX_PACKET lpp = New (SET_ENUM_TO_COMBOBOX_PACKET);
         memset (lpp, 0x00, sizeof(SET_ENUM_TO_COMBOBOX_PACKET));
         lpp->hCombo = GetDlgItem (hDlg, IDC_FILESET);
         lpp->lpiServer = lpiServer;
         lpp->lpiAggregate = lpiAggregate;
         lpp->lpiSelect = ((lpiServer == lpi->GetServer()) && (lpi->fIsFileset())) ? (lpi) : NULL;
         StartTask (taskSET_ENUM_TO_COMBOBOX, hDlg, lpp);
         }
      }
}


void Server_Salvage_OnEndTask_EnumFilesets (HWND hDlg, LPIDENT lpi, LPTASKPACKET ptp)
{
   // The filesets list is finished; at this point, the user is free to go
   // selecting any other server/aggregate/fileset; the OK button should also
   // get enabled.
   //
   EnableWindow (GetDlgItem (hDlg, IDOK), TRUE);
   EnableWindow (GetDlgItem (hDlg, IDC_SERVER), TRUE);
   EnableWindow (GetDlgItem (hDlg, IDC_AGGREGATE_ALL), TRUE);

   Server_Salvage_OnAllAggregates (hDlg);
}


BOOL CALLBACK Server_Salvage_Results_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_SALVAGE_RESULTS, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, lp);

   LPIDENT lpi;
   if ((lpi = (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            RECT rWindow;
            GetWindowRect (hDlg, &rWindow);
            ResizeWindow (hDlg, awdSalvageResults, rwaMoveToHere, &rWindow);

            Server_Salvage_Results_OnInitDialog (hDlg, lpi);
            break;

         case WM_SIZE:
            // if (lp==0), we're minimizing--don't call ResizeWindow().
            if (lp != 0)
               ResizeWindow (hDlg, awdSalvageResults, rwaFixupGuts);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskSVR_SALVAGE)
                  Server_Salvage_OnEndTask_Salvage (hDlg, lpi, ptp);
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
               case IDCANCEL:
                  DestroyWindow (hDlg);
                  break;
               }
            break;

         case WM_CTLCOLOREDIT:
            if ((HWND)lp == GetDlgItem (hDlg, IDC_SALVAGE_DETAILS))
               {
               SetBkColor ((HDC)wp, GetSysColor (COLOR_WINDOW));
               return CreateSolidBrush (GetSysColor (COLOR_WINDOW))?TRUE:FALSE;
               }
            break;
         }
      }
   return FALSE;
}


void Server_Salvage_Results_OnInitDialog (HWND hDlg, LPIDENT lpiSalvage)
{
   TCHAR szServer[ cchNAME ];
   lpiSalvage->GetServerName (szServer);

   TCHAR szAggregate[ cchNAME ];
   if (!lpiSalvage->fIsServer())
      lpiSalvage->GetAggregateName (szAggregate);

   TCHAR szFileset[ cchNAME ];
   if (lpiSalvage->fIsFileset())
      lpiSalvage->GetFilesetName (szFileset);

   LPTSTR pszTitle;
   if (lpiSalvage->fIsServer())
      pszTitle = FormatString (IDS_SALVAGE_SVR, TEXT("%s"), szServer);
   else if (lpiSalvage->fIsAggregate())
      pszTitle = FormatString (IDS_SALVAGE_AGG, TEXT("%s%s"), szServer, szAggregate);
   else // (lpiSalvage->fIsFileset())
      pszTitle = FormatString (IDS_SALVAGE_SET, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
   SetDlgItemText (hDlg, IDC_SALVAGE_TITLE, pszTitle);
   FreeString (pszTitle);
}


void Server_Salvage_OnEndTask_Salvage (HWND hDlg, LPIDENT lpiSalvage, LPTASKPACKET ptp)
{
   if (!ptp->rc)
      {
      ErrorDialog (ptp->status, IDS_ERROR_CANT_SALVAGE);
      DestroyWindow (hDlg);
      }
   else
      {
      if (TASKDATA(ptp)->pszText1)
         {
         SetDlgItemText (hDlg, IDC_SALVAGE_DETAILS, TASKDATA(ptp)->pszText1);
         }
      else
         {
         TCHAR szServer[ cchNAME ];
         lpiSalvage->GetServerName (szServer);
         LPTSTR pszNoLog = FormatString (IDS_ERROR_CANT_READ_SALVAGE_LOG, TEXT("%s"), szServer);
         SetDlgItemText (hDlg, IDC_SALVAGE_DETAILS, pszNoLog);
         FreeString (pszNoLog);
         }

      ShowWindow (hDlg, SW_SHOW);
      }
}

