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
#include "svr_syncvldb.h"
#include "propcache.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Server_SyncVLDB_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Server_SyncVLDB_OnInitDialog (HWND hDlg, LPIDENT lpi);
void Server_SyncVLDB_OnOK (HWND hDlg, LPIDENT lpi);
void Server_SyncVLDB_OnEndTask_Init (HWND hDlg, LPTASKPACKET ptp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Server_SyncVLDB (LPIDENT lpi)
{
   HWND hCurrent;
   if ((hCurrent = PropCache_Search (pcSVR_SYNCVLDB, lpi)) != NULL)
      {
      SetFocus (hCurrent);
      }
   else
      {
      ModelessDialogParam (IDD_SVR_SYNCVLDB, NULL, (DLGPROC)Server_SyncVLDB_DlgProc, (LPARAM)lpi);
      }
}


BOOL CALLBACK Server_SyncVLDB_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_SYNCVLDB, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, lp);

   LPIDENT lpi;
   if ((lpi = (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            PropCache_Add (pcSVR_SYNCVLDB, lpi, hDlg);
            Server_SyncVLDB_OnInitDialog (hDlg, lpi);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskAGG_FIND_GHOST)
                  Server_SyncVLDB_OnEndTask_Init (hDlg, ptp);
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  Server_SyncVLDB_OnOK (hDlg, lpi);
                  DestroyWindow (hDlg);
                  break;

               case IDCANCEL:
                  DestroyWindow (hDlg);
                  break;
               }
            break;

         case WM_DESTROY:
            PropCache_Delete (pcSVR_SYNCVLDB, lpi);
            break;
         }
      }

   return FALSE;
}


void Server_SyncVLDB_OnInitDialog (HWND hDlg, LPIDENT lpi)
{
   if (lpi->fIsServer())
      {
      TCHAR szServer[ cchNAME ];
      lpi->GetServerName (szServer);

      LPTSTR pszText = FormatString (IDS_SYNCVLDB_SVR_DESC, TEXT("%s"), szServer);
      SetDlgItemText (hDlg, IDC_SYNC_DESC, pszText);
      FreeString (pszText);
      pszText = FormatString (IDS_SYNCVLDB_SVR_DESC2, TEXT("%s"), szServer);
      SetDlgItemText (hDlg, IDC_SYNC_DESC2, pszText);
      FreeString (pszText);

      ShowWindow (hDlg, SW_SHOW);
      }
   else // (lpi->fIsAggregate())
      {
      TCHAR szServer[ cchNAME ];
      lpi->GetServerName (szServer);
      TCHAR szAggregate[ cchNAME ];
      lpi->GetAggregateName (szAggregate);

      LPTSTR pszText = FormatString (IDS_SYNCVLDB_AGG_DESC, TEXT("%s%s"), szServer, szAggregate);
      SetDlgItemText (hDlg, IDC_SYNC_DESC, pszText);
      FreeString (pszText);
      pszText = FormatString (IDS_SYNCVLDB_AGG_DESC2, TEXT("%s%s"), szServer, szAggregate);
      SetDlgItemText (hDlg, IDC_SYNC_DESC2, pszText);
      FreeString (pszText);

      StartTask (taskAGG_FIND_GHOST, hDlg, lpi);
      }
}


void Server_SyncVLDB_OnEndTask_Init (HWND hDlg, LPTASKPACKET ptp)
{
   if (ptp->rc && (!(TASKDATA(ptp)->wGhost & GHOST_HAS_SERVER_ENTRY)))
      {
      LPIDENT lpi = (LPIDENT)( ptp->lpUser );

      TCHAR szServer[ cchNAME ];
      lpi->GetServerName (szServer);
      TCHAR szAggregate[ cchNAME ];
      lpi->GetAggregateName (szAggregate);

      ErrorDialog (0, IDS_ERROR_CANT_SYNC_GHOST_AGGREGATE, TEXT("%s%s"), szServer, szAggregate);
      DestroyWindow (hDlg);
      return;
      }

   ShowWindow (hDlg, SW_SHOW);
}


void Server_SyncVLDB_OnOK (HWND hDlg, LPIDENT lpi)
{
   LPSVR_SYNCVLDB_PARAMS lpp = New (SVR_SYNCVLDB_PARAMS);
   lpp->lpi = (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER);
   lpp->fForce = TRUE;
   StartTask (taskSVR_SYNCVLDB, NULL, lpp);
}

