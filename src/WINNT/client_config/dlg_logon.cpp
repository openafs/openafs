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

#include "afs_config.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define nRETRY_MIN            5
#define nRETRY_MAX            180

static TCHAR szYes[10] = TEXT("Yes");
static TCHAR szNo[10] = TEXT("No");

// Our dialog data
static BOOL fFirstTime = TRUE;
static DWORD nLoginRetryInterval;
static BOOL fFailLoginsSilently;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Logon_OnInitDialog (HWND hDlg);
void Logon_OnOK (HWND hDlg);
void Logon_OnCancel(HWND hDlg);
BOOL Logon_OnApply();


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Logon_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         Logon_OnInitDialog (hDlg);
         break;

      case WM_CTLCOLORSTATIC:
         if ((HWND)lp == GetDlgItem (hDlg, IDC_CHUNK_SIZE))
            {
            if (IsWindowEnabled ((HWND)lp))
               {
               static HBRUSH hbrStatic = CreateSolidBrush (GetSysColor (COLOR_WINDOW));
               SetTextColor ((HDC)wp, GetSysColor (COLOR_WINDOWTEXT));
               SetBkColor ((HDC)wp, GetSysColor (COLOR_WINDOW));
               return (BOOL)hbrStatic;
               }
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDHELP:
               Logon_DlgProc (hDlg, WM_HELP, 0, 0);
               break;

            case IDOK:
                Logon_OnOK(hDlg);
                break;
                
            case IDCANCEL:
                Logon_OnCancel(hDlg);
                break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_ADVANCED_LOGON);
         break;
      }

   return FALSE;
}


void Logon_OnInitDialog (HWND hDlg)
{
   if (fFirstTime) {
      Config_GetLoginRetryInterval(&g.Configuration.nLoginRetryInterval);
      Config_GetFailLoginsSilently(&g.Configuration.fFailLoginsSilently);

      nLoginRetryInterval = g.Configuration.nLoginRetryInterval;
      fFailLoginsSilently = g.Configuration.fFailLoginsSilently;

      fFirstTime = FALSE;
   }

   CreateSpinner (GetDlgItem (hDlg, IDC_LOGIN_RETRY_INTERVAL), 10, FALSE, nRETRY_MIN, nLoginRetryInterval, nRETRY_MAX);

   GetString (szYes, IDS_YES);
   GetString (szNo, IDS_NO);

   HWND hCombo = GetDlgItem(hDlg, IDC_FAIL_SILENTLY);

   // Always add szNo first so it has index 0 and szYes has index 1
   CB_AddItem (hCombo, szNo, 0);
   CB_AddItem (hCombo, szYes, 0);
  
   CB_SetSelected (hCombo, fFailLoginsSilently);
}


void Logon_OnOK (HWND hDlg)
{
   nLoginRetryInterval = SP_GetPos (GetDlgItem (hDlg, IDC_LOGIN_RETRY_INTERVAL));
   fFailLoginsSilently = CB_GetSelected (GetDlgItem (hDlg, IDC_FAIL_SILENTLY));

   EndDialog(hDlg, IDOK);
}


BOOL Logon_OnApply()
{
   if (fFirstTime)
      return TRUE;
   
   if (nLoginRetryInterval != g.Configuration.nLoginRetryInterval) {
      if (!Config_SetLoginRetryInterval (nLoginRetryInterval))
         return FALSE;
      g.Configuration.nLoginRetryInterval = nLoginRetryInterval;
   }
   
   if (fFailLoginsSilently != g.Configuration.fFailLoginsSilently) {
      if (!Config_SetFailLoginsSilently (fFailLoginsSilently))
         return FALSE;
      g.Configuration.fFailLoginsSilently = fFailLoginsSilently;
   }

   return TRUE;
}


void Logon_OnCancel(HWND hDlg)
{
   fFirstTime = TRUE;

   EndDialog(hDlg, IDCANCEL);
}
