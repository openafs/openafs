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
#include "svc_startstop.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpi;
   BOOL fStart;
   BOOL fTemporary;
   } SERVICE_STARTSTOP_PARAMS, *LPSERVICE_STARTSTOP_PARAMS;

BOOL CALLBACK Services_StartStop_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Services_StartStop_OnInitDialog (HWND hDlg, LPSERVICE_STARTSTOP_PARAMS lpp);
void Services_StartStop_OnOK (HWND hDlg, LPSERVICE_STARTSTOP_PARAMS lpp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL Services_fRunning (LPSERVICE lpService)
{
   SERVICESTATUS ss;
   ULONG status;
   if (!lpService->GetStatus (&ss, FALSE, &status))
      return FALSE;

   return (ss.state == SERVICESTATE_RUNNING) ? TRUE : FALSE;
}



void Services_Restart (LPIDENT lpiService)
{
   StartTask (taskSVC_RESTART, NULL, lpiService);
}


void Services_Start (LPIDENT lpiService)
{
   SERVICE_STARTSTOP_PARAMS dp;
   memset (&dp, 0x00, sizeof(dp));
   dp.lpi = lpiService;
   dp.fStart = TRUE;

   if (ModalDialogParam (IDD_SVC_STARTSTOP, NULL, (DLGPROC)Services_StartStop_DlgProc, (LPARAM)&dp) == IDOK)
      {
      LPSVC_START_PARAMS lpp = New (SVC_START_PARAMS);
      memset (lpp, 0x00, sizeof(SVC_START_PARAMS));
      lpp->lpiStart = lpiService;
      lpp->fTemporary = dp.fTemporary;
      StartTask (taskSVC_START, NULL, lpp);
      }
}


void Services_Stop (LPIDENT lpiService)
{
   SERVICE_STARTSTOP_PARAMS dp;
   memset (&dp, 0x00, sizeof(dp));
   dp.lpi = lpiService;
   dp.fStart = FALSE;

   if (ModalDialogParam (IDD_SVC_STARTSTOP, NULL, (DLGPROC)Services_StartStop_DlgProc, (LPARAM)&dp) == IDOK)
      {
      LPSVC_STOP_PARAMS lpp = New (SVC_STOP_PARAMS);
      memset (lpp, 0x00, sizeof(SVC_STOP_PARAMS));
      lpp->lpiStop = lpiService;
      lpp->fTemporary = dp.fTemporary;
      StartTask (taskSVC_STOP, NULL, lpp);
      }
}


BOOL CALLBACK Services_StartStop_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   static LPSERVICE_STARTSTOP_PARAMS lpp = NULL;

   if (lpp != NULL)
      {
      if (AfsAppLib_HandleHelp ((lpp->fStart) ? IDD_SVC_START : IDD_SVC_STOP, hDlg, msg, wp, lp))
         return TRUE;
      }

   switch (msg)
      {
      case WM_INITDIALOG:
         lpp = (LPSERVICE_STARTSTOP_PARAMS)lp;
         Services_StartStop_OnInitDialog (hDlg, lpp);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               Services_StartStop_OnOK (hDlg, lpp);
               EndDialog (hDlg, IDOK);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;
            }
         break;
      }

   return FALSE;
}


void Services_StartStop_OnInitDialog (HWND hDlg, LPSERVICE_STARTSTOP_PARAMS lpp)
{
   LPTSTR pszString = FormatString ((lpp->fStart) ? IDS_STARTSERVICE_TITLE : IDS_STOPSERVICE_TITLE);
   SetWindowText (hDlg, pszString);
   FreeString (pszString);

   TCHAR szServer[ cchNAME ];
   lpp->lpi->GetServerName (szServer);

   TCHAR szService[ cchNAME ];
   lpp->lpi->GetServiceName (szService);

   pszString = FormatString ((lpp->fStart) ? IDS_STARTSERVICE_TEXT : IDS_STOPSERVICE_TEXT, TEXT("%s%s"), szServer, szService);
   SetDlgItemText (hDlg, IDC_STARTSTOP_TEXT, pszString);
   FreeString (pszString);

   pszString = FormatString ((lpp->fStart) ? IDS_STARTSERVICE_STARTUP : IDS_STOPSERVICE_STARTUP, TEXT("%s%s"), szServer, szService);
   SetDlgItemText (hDlg, IDC_STARTSTOP_STARTUP, pszString);
   FreeString (pszString);

   pszString = FormatString ((lpp->fStart) ? IDS_STARTSERVICE_PERMANENT : IDS_STOPSERVICE_PERMANENT, TEXT("%s%s"), szServer, szService);
   SetDlgItemText (hDlg, IDC_STARTSTOP_PERMANENT, pszString);
   FreeString (pszString);

   pszString = FormatString ((lpp->fStart) ? IDS_STARTSERVICE_TEMPORARY : IDS_STOPSERVICE_TEMPORARY, TEXT("%s%s"), szServer, szService);
   SetDlgItemText (hDlg, IDC_STARTSTOP_TEMPORARY, pszString);
   FreeString (pszString);

   CheckDlgButton (hDlg, IDC_STARTSTOP_PERMANENT, !lpp->fTemporary);
   CheckDlgButton (hDlg, IDC_STARTSTOP_TEMPORARY,  lpp->fTemporary);
}


void Services_StartStop_OnOK (HWND hDlg, LPSERVICE_STARTSTOP_PARAMS lpp)
{
   lpp->fTemporary = IsDlgButtonChecked (hDlg, IDC_STARTSTOP_TEMPORARY);
}

