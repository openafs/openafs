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

#define nBUFSIZE_MIN            3000
#define nBUFSIZE_MAX            32000

// Our dialog data
static BOOL fFirstTime = TRUE;
static DWORD nTraceBufSize;
static BOOL fTrapOnPanic;
static BOOL fReportSessionStartups;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Diag_OnInitDialog (HWND hDlg);
void Diag_OnOK(HWND hDlg);
void Diag_OnCancel(HWND hDlg);
BOOL Diag_OnApply();


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Diag_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         Diag_OnInitDialog (hDlg);
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
               Diag_DlgProc (hDlg, WM_HELP, 0, 0);
               break;

            case IDOK:
                Diag_OnOK(hDlg);
                break;
                
            case IDCANCEL:
                Diag_OnCancel(hDlg);
                break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_ADVANCED_DIAG);
         break;
      }

   return FALSE;
}

static void SetUpYesNoCombo(HWND hDlg, UINT nCtrlID, BOOL fInitialSetting)
{
   static TCHAR szYes[10] = TEXT("");
   static TCHAR szNo[10] = TEXT("");

   if (!*szYes) {
        GetString (szYes, IDS_YES);
        GetString (szNo, IDS_NO);
   }

   HWND hCombo = GetDlgItem(hDlg, nCtrlID);

   // Always add szNo first so it has index 0 and szYes has index 1
   CB_AddItem (hCombo, szNo, 0);
   CB_AddItem (hCombo, szYes, 0);

   CB_SetSelected (hCombo, fInitialSetting);
}

void Diag_OnInitDialog (HWND hDlg)
{
   if (fFirstTime) {
      Config_GetTraceBufferSize(&g.Configuration.nTraceBufSize);
      Config_GetTrapOnPanic(&g.Configuration.fTrapOnPanic);
      Config_GetReportSessionStartups(&g.Configuration.fReportSessionStartups);
      
      nTraceBufSize = g.Configuration.nTraceBufSize;
      fTrapOnPanic = g.Configuration.fTrapOnPanic;
      fReportSessionStartups = g.Configuration.fReportSessionStartups;
      
      fFirstTime = FALSE;
   }

   CreateSpinner (GetDlgItem (hDlg, IDC_TRACE_LOG_BUF_SIZE), 10, FALSE, nBUFSIZE_MIN, nTraceBufSize, nBUFSIZE_MAX);

   SetUpYesNoCombo(hDlg, IDC_TRAP_ON_PANIC, fTrapOnPanic);
   SetUpYesNoCombo(hDlg, IDC_REPORT_SESSION_STARTUPS, fReportSessionStartups);
}


void Diag_OnOK (HWND hDlg)
{
   nTraceBufSize = SP_GetPos (GetDlgItem (hDlg, IDC_TRACE_LOG_BUF_SIZE));
   fTrapOnPanic = CB_GetSelected (GetDlgItem (hDlg, IDC_TRAP_ON_PANIC));
   fReportSessionStartups = CB_GetSelected (GetDlgItem (hDlg, IDC_REPORT_SESSION_STARTUPS));

   EndDialog(hDlg, IDOK);
}


BOOL Diag_OnApply()
{
   if (fFirstTime)
      return TRUE;

   if (nTraceBufSize != g.Configuration.nTraceBufSize) {
      if (!Config_SetTraceBufferSize (nTraceBufSize))
         return FALSE;
      g.Configuration.nTraceBufSize = nTraceBufSize;
   }
   
   if (fTrapOnPanic != g.Configuration.fTrapOnPanic) {
      if (!Config_SetTrapOnPanic (fTrapOnPanic))
         return FALSE;
      g.Configuration.fTrapOnPanic = fTrapOnPanic;
   }
   
   if (fReportSessionStartups != g.Configuration.fReportSessionStartups) {
      if (!Config_SetReportSessionStartups (fReportSessionStartups))
         return FALSE;
      g.Configuration.fReportSessionStartups = fReportSessionStartups;
   }

   return TRUE;
}


void Diag_OnCancel(HWND hDlg)
{
   fFirstTime = TRUE;

   EndDialog(hDlg, IDCANCEL);
}
