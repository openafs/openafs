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
#include "svc_create.h"
#include "svc_general.h"
#include "propcache.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Services_Create_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Services_Create_OnInitDialog (HWND hDlg, LPIDENT lpi);
void Services_Create_OnType (HWND hDlg);
void Services_Create_OnApply (HWND hDlg);
void Services_Create_EnableOK (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Services_Create (LPIDENT lpiServer)
{
   HWND hCurrent;

   if ((hCurrent = PropCache_Search (pcSVC_CREATE, NULL)) != NULL)
      {
      SetFocus (hCurrent);
      }
   else
      {
      LPPROPSHEET psh = PropSheet_Create (IDS_SVC_ADD_TITLE, FALSE);
      psh->sh.dwFlags |= PSH_NOAPPLYNOW;

      PropSheet_AddTab (psh, IDS_SVC_ADD_TAB, IDD_SVC_CREATE, (DLGPROC)Services_Create_DlgProc, (LPARAM)lpiServer, TRUE);
      PropSheet_ShowModeless (psh);
      }
}


BOOL CALLBACK Services_Create_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVC_CREATE, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG_SHEET:
         PropCache_Add (pcSVC_CREATE, NULL, hDlg);
         break;

      case WM_DESTROY_SHEET:
         PropCache_Delete (hDlg);
         break;

      case WM_INITDIALOG:
         Services_Create_OnInitDialog (hDlg, (LPIDENT)( ((LPPROPSHEETPAGE)lp)->lParam ));
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               Services_Create_OnApply (hDlg);
               break;

            case IDC_SVC_NAME:
               TCHAR szService[ cchNAME ];
               GetDlgItemText (hDlg, IDC_SVC_NAME, szService, cchNAME);

               TCHAR szLogFile[ MAX_PATH ];
               Services_GuessLogName (szLogFile, szService);
               if (szLogFile[0] != TEXT('\0'))
                  SetDlgItemText (hDlg, IDC_SVC_LOGFILE, szLogFile);

               Services_Create_EnableOK (hDlg);
               break;

            case IDC_SVC_COMMAND:
               Services_Create_EnableOK (hDlg);
               break;

            case IDC_SVC_TYPE_SIMPLE:
            case IDC_SVC_TYPE_CRON:
               Services_Create_OnType (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Services_Create_OnInitDialog (HWND hDlg, LPIDENT lpi)
{
   CheckDlgButton (hDlg, IDC_SVC_TYPE_SIMPLE, TRUE);
   CheckDlgButton (hDlg, IDC_SVC_RUNNOW, TRUE);

   LPSVR_ENUM_TO_COMBOBOX_PACKET lpp = New (SVR_ENUM_TO_COMBOBOX_PACKET);
   lpp->hCombo = GetDlgItem (hDlg, IDC_SVC_SERVER);
   lpp->lpiSelect = lpi;
   StartTask (taskSVR_ENUM_TO_COMBOBOX, hDlg, lpp);

   HWND hName = GetDlgItem (hDlg, IDC_SVC_NAME);
   CB_StartChange (hName, TRUE);
   CB_AddItem (hName, TEXT("buserver"), 0);
   CB_AddItem (hName, TEXT("fileserver"), 0);
   CB_AddItem (hName, TEXT("kaserver"), 0);
   CB_AddItem (hName, TEXT("ptserver"), 0);
   CB_AddItem (hName, TEXT("salvager"), 0);
   CB_AddItem (hName, TEXT("upclient"), 0);
   CB_AddItem (hName, TEXT("upserver"), 0);
   CB_AddItem (hName, TEXT("vlserver"), 0);
   CB_AddItem (hName, TEXT("volserver"), 0);
   CB_EndChange (hName, 0);

   Services_Create_EnableOK (hDlg);
   Services_Create_OnType (hDlg);
}


void Services_Create_OnType (HWND hDlg)
{
   AFSSERVICETYPE type;
   if (IsDlgButtonChecked (hDlg, IDC_SVC_TYPE_FS))
      type = SERVICETYPE_FS;
   else if (IsDlgButtonChecked (hDlg, IDC_SVC_TYPE_CRON))
      type = SERVICETYPE_CRON;
   else // (IsDlgButtonChecked (hDlg, IDC_SVC_TYPE_SIMPLE))
      type = SERVICETYPE_SIMPLE;

   EnableWindow (GetDlgItem (hDlg, IDC_SVC_RUNNOW), (type == SERVICETYPE_SIMPLE));
   EnableWindow (GetDlgItem (hDlg, IDC_SVC_RUNDAY),  (type == SERVICETYPE_CRON));
   EnableWindow (GetDlgItem (hDlg, IDC_SVC_RUNTIME), (type == SERVICETYPE_CRON));

   HWND hDay = GetDlgItem (hDlg, IDC_SVC_RUNDAY);
   CB_StartChange (hDay, TRUE);
   CB_AddItem (hDay, IDS_RECUR_DAILY,    -1);
   CB_AddItem (hDay, IDS_RECUR_SUNDAY,    0);
   CB_AddItem (hDay, IDS_RECUR_MONDAY,    1);
   CB_AddItem (hDay, IDS_RECUR_TUESDAY,   2);
   CB_AddItem (hDay, IDS_RECUR_WEDNESDAY, 3);
   CB_AddItem (hDay, IDS_RECUR_THURSDAY,  4);
   CB_AddItem (hDay, IDS_RECUR_FRIDAY,    5);
   CB_AddItem (hDay, IDS_RECUR_SATURDAY,  6);
   CB_EndChange (hDay, -1);
}


void Services_Create_OnApply (HWND hDlg)
{
   LPSVC_CREATE_PARAMS lpp = New (SVC_CREATE_PARAMS);
   lpp->lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SVC_SERVER));
   GetDlgItemText (hDlg, IDC_SVC_NAME, lpp->szService, cchNAME);
   GetDlgItemText (hDlg, IDC_SVC_COMMAND, lpp->szCommand, cchNAME);
   GetDlgItemText (hDlg, IDC_SVC_PARAMS, lpp->szParams, cchNAME);
   GetDlgItemText (hDlg, IDC_SVC_LOGFILE, lpp->szLogFile, cchNAME);
   GetDlgItemText (hDlg, IDC_SVC_NOTIFIER, lpp->szNotifier, cchNAME);

   if (IsDlgButtonChecked (hDlg, IDC_SVC_TYPE_FS))
      lpp->type = SERVICETYPE_FS;
   else if (IsDlgButtonChecked (hDlg, IDC_SVC_TYPE_CRON))
      lpp->type = SERVICETYPE_CRON;
   else // (IsDlgButtonChecked (hDlg, IDC_SVC_TYPE_SIMPLE))
      lpp->type = SERVICETYPE_SIMPLE;

   if (lpp->type == SERVICETYPE_SIMPLE)
      {
      lpp->fRunNow = IsDlgButtonChecked (hDlg, IDC_SVC_RUNNOW);
      }
   else if (lpp->type == SERVICETYPE_CRON)
      {
      TI_GetTime (GetDlgItem (hDlg, IDC_SVC_RUNTIME), &lpp->stIfCron);
      lpp->stIfCron.wDayOfWeek = (WORD)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SVC_RUNDAY));
      // wDayOfWeek: -1==daily, 0==sunday etc
      }

   StartTask (taskSVC_CREATE, NULL, lpp);
}


void Services_Create_EnableOK (HWND hDlg)
{
   BOOL fEnable = TRUE;

   if (!IsWindowEnabled (GetDlgItem (hDlg, IDC_SVC_SERVER)))
      fEnable = FALSE;

   if (fEnable)
      {
      TCHAR szText[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_SVC_NAME, szText, cchRESOURCE);
      if (szText[0] == TEXT('\0'))
         fEnable = FALSE;
      }

   if (fEnable)
      {
      TCHAR szText[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_SVC_COMMAND, szText, cchRESOURCE);
      if (szText[0] == TEXT('\0'))
         fEnable = FALSE;
      }

   EnableWindow (GetDlgItem (GetParent(hDlg), IDOK), fEnable);
}

