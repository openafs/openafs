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
#include "options.h"
#include "display.h"
#include "propcache.h"
#include "creds.h"
#include "set_general.h"
#include "command.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Options_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Options_General_OnInitDialog (HWND hDlg);
void Options_General_OnApply (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void ShowOptionsDialog (void)
{
   TCHAR szCell[ cchNAME ];
   if (g.lpiCell)
      g.lpiCell->GetCellName (szCell);
   else
      AfsAppLib_GetLocalCell (szCell);

   LPPROPSHEET psh = PropSheet_Create (IDS_OPTIONS_TITLE, FALSE);
   psh->sh.hwndParent = g.hMain;
   PropSheet_AddTab (psh, IDS_OPTIONS_GENERAL_TAB, IDD_OPTIONS_GENERAL, (DLGPROC)Options_General_DlgProc, 0, TRUE);
   PropSheet_ShowModal (psh, PumpMessage);
}


BOOL CALLBACK Options_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_OPTIONS_GENERAL, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG_SHEET:
         PropCache_Add (pcGENERAL, 0, hDlg);
         break;

      case WM_DESTROY_SHEET:
         PropCache_Delete (hDlg);
         break;

      case WM_INITDIALOG:
         PropCache_Add (pcGENERAL, NULL, hDlg);
         Options_General_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               Options_General_OnApply (hDlg);
               break;

            case IDC_OPT_SVR_LONGNAMES:
            case IDC_OPT_SVR_DBL_PROP:
            case IDC_OPT_SVR_DBL_DEPENDS:
            case IDC_OPT_SVR_DBL_OPEN:
            case IDC_OPT_SVR_OPENMON:
            case IDC_OPT_SVR_CLOSEUNMON:
            case IDC_OPT_WARN_BADCREDS:
               PropSheetChanged (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Options_General_OnInitDialog (HWND hDlg)
{
   CheckDlgButton (hDlg, IDC_OPT_SVR_LONGNAMES,   (gr.fServerLongNames));
   CheckDlgButton (hDlg, IDC_OPT_SVR_DBL_PROP,    (gr.fDoubleClickOpens == 0));
   CheckDlgButton (hDlg, IDC_OPT_SVR_DBL_DEPENDS, (gr.fDoubleClickOpens == 2));
   CheckDlgButton (hDlg, IDC_OPT_SVR_DBL_OPEN,    (gr.fDoubleClickOpens == 1));
   CheckDlgButton (hDlg, IDC_OPT_SVR_OPENMON,     (gr.fOpenMonitors));
   CheckDlgButton (hDlg, IDC_OPT_SVR_CLOSEUNMON,  (gr.fCloseUnmonitors));
   CheckDlgButton (hDlg, IDC_OPT_WARN_BADCREDS,   (gr.fWarnBadCreds));
}


void Options_General_OnApply (HWND hDlg)
{
   BOOL fServerLongNamesOld = gr.fServerLongNames;

   gr.fServerLongNames = IsDlgButtonChecked (hDlg, IDC_OPT_SVR_LONGNAMES);

   if (IsDlgButtonChecked (hDlg, IDC_OPT_SVR_DBL_PROP))
      gr.fDoubleClickOpens = 0;
   else if (IsDlgButtonChecked (hDlg, IDC_OPT_SVR_DBL_OPEN))
      gr.fDoubleClickOpens = 1;
   else // (IsDlgButtonChecked (hDlg, IDC_OPT_SVR_DBL_DEPENDS))
      gr.fDoubleClickOpens = 2;

   gr.fOpenMonitors = IsDlgButtonChecked (hDlg, IDC_OPT_SVR_OPENMON);
   gr.fCloseUnmonitors = IsDlgButtonChecked (hDlg, IDC_OPT_SVR_CLOSEUNMON);
   gr.fWarnBadCreds = IsDlgButtonChecked (hDlg, IDC_OPT_WARN_BADCREDS);

   StoreSettings (REGSTR_SETTINGS_BASE, REGSTR_SETTINGS_PATH, REGVAL_SETTINGS, &gr, sizeof(gr), wVerGLOBALS_RESTORED);

   if (fServerLongNamesOld != gr.fServerLongNames)
      {
      AfsClass_RequestLongServerNames (gr.fServerLongNames);

      // repopulate the list of server names.
      UpdateDisplay_Servers (FALSE, NULL, 0);
      }

   if (gr.fWarnBadCreds)
      {
      if (!CheckCredentials (TRUE)) // user needs new creds?
         {
         PostMessage (g.hMain, WM_COMMAND, MAKELONG(M_CREDENTIALS,0), 0);
         }
      }
}

