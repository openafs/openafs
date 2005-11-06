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
#include "options.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cminREFRESH_MIN            1 // 15 minutes
#define cminREFRESH_DEFAULT       60 // 1 hour
#define cminREFRESH_MAX        10080 // 1 week


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Options_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Options_OnInitDialog (HWND hDlg);
void Options_OnApply (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void ShowOptionsDialog (HWND hParent)
{
   LPPROPSHEET psh = PropSheet_Create (IDS_OPTIONS_TITLE, TRUE, hParent, (LPARAM)0);
   PropSheet_AddTab (psh, 0, IDD_OPTIONS, (DLGPROC)Options_DlgProc, (LPARAM)0, TRUE, TRUE);
   PropSheet_ShowModal (psh);
}


BOOL CALLBACK Options_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_OPTIONS, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         Options_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               Options_OnApply (hDlg);
               break;

            case IDC_REFRESH:
               EnableWindow (GetDlgItem (hDlg, IDC_REFRESH_RATE), IsDlgButtonChecked (hDlg, IDC_REFRESH));
               break;
            }
         break;
      }

   return FALSE;
}


void Options_OnInitDialog (HWND hDlg)
{
   CheckDlgButton (hDlg, IDC_REGEXP_UNIX, !gr.fWindowsRegexp);
   CheckDlgButton (hDlg, IDC_REGEXP_WINDOWS, gr.fWindowsRegexp);

   CheckDlgButton (hDlg, IDC_WARN_BADCREDS, gr.fWarnBadCreds);

   CheckDlgButton (hDlg, IDC_REFRESH, (gr.cminRefreshRate != 0));

   DWORD cminShow = (gr.cminRefreshRate != 0) ? gr.cminRefreshRate : cminREFRESH_DEFAULT;
   CreateSpinner (GetDlgItem (hDlg, IDC_REFRESH_RATE), 10, FALSE, cminREFRESH_MIN, cminShow, cminREFRESH_MAX);

   EnableWindow (GetDlgItem (hDlg, IDC_REFRESH_RATE), IsDlgButtonChecked (hDlg, IDC_REFRESH));
}


void Options_OnApply (HWND hDlg)
{
   gr.fWindowsRegexp = IsDlgButtonChecked (hDlg, IDC_REGEXP_WINDOWS);

   gr.fWarnBadCreds = IsDlgButtonChecked (hDlg, IDC_WARN_BADCREDS);

   DWORD cminRateOld = gr.cminRefreshRate;

   if (!IsDlgButtonChecked (hDlg, IDC_REFRESH))
      gr.cminRefreshRate = 0;
   else
      gr.cminRefreshRate = (DWORD) SP_GetPos (GetDlgItem (hDlg, IDC_REFRESH_RATE));

   if ((cminRateOld != gr.cminRefreshRate) && (g.idCell))
      {
      StartTask (taskSET_REFRESH);
      }
}

