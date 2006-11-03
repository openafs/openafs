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

      #include <stdio.h>

#include "afs_config.h"
#include "tab_advanced.h"
#include "pagesize.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define ckCACHE_MIN           1024L     // 1MB Cache Minimum
#define ckCACHE_MAX           2097152L  // 2GB Cache Maximum (limited by space)

#define ckCHUNK_MIN           4L
#define ckCHUNK_MAX           1048576L

#define cSTATS_MIN            256L
#define cSTATS_MAX            262144L


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void AdvancedTab_OnInitDialog (HWND hDlg);
BOOL AdvancedTab_OnApply (HWND hDlg);
void AdvancedTab_OnRefresh (HWND hDlg);

// From dlg_automap.cpp
extern BOOL CALLBACK AutoMap_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

// From dlg_misc.cpp
extern BOOL Misc_OnApply();    
extern BOOL CALLBACK Misc_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

// From dlg_logon.cpp
extern BOOL CALLBACK Logon_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
extern BOOL Logon_OnApply();   

// From dlg_diag.cpp
extern BOOL CALLBACK Diag_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
extern BOOL Diag_OnApply();    

// From binding_misc.cpp
extern BOOL Binding_OnApply();
extern BOOL CALLBACK Binding_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

/*
 * ROUTINES ___________________________________________________________________
 *
 */

static DWORD log2 (DWORD dwValue)
{
   DWORD dwLog;
   for (dwLog = 0; (DWORD)(1<<dwLog) < dwValue; ++dwLog)
      ;
   return dwLog;
}


BOOL CALLBACK AdvancedTab_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         AdvancedTab_OnInitDialog (hDlg);
         break;

      case WM_CTLCOLORSTATIC:
         if ((HWND)lp == GetDlgItem (hDlg, IDC_CHUNK_SIZE))
            {
            if (IsWindowEnabled ((HWND)lp))
               {
               static HBRUSH hbrStatic = CreateSolidBrush (GetSysColor (COLOR_WINDOW));
               SetTextColor ((HDC)wp, GetSysColor (COLOR_WINDOWTEXT));
               SetBkColor ((HDC)wp, GetSysColor (COLOR_WINDOW));
               return hbrStatic?TRUE:FALSE;
               }
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               if (!AdvancedTab_OnApply (hDlg))
                  SetWindowLongPtr (hDlg, DWLP_MSGRESULT, TRUE);
               break;

            case IDC_REFRESH:
               AdvancedTab_OnRefresh (hDlg);
               break;

            case IDC_MISC_PARMS:
         	    ModalDialog(IDD_MISC_CONFIG_PARMS, hDlg, (DLGPROC)Misc_DlgProc);
                break;
                
            case IDC_BINDING_PARMS:
                ModalDialog(IDD_BINDING_CONFIG_PARAMS, hDlg, (DLGPROC)Binding_DlgProc);
                break;

            case IDC_LOGON_PARMS:
                ModalDialog(IDD_LOGIN_CONFIG_PARMS, hDlg, (DLGPROC)Logon_DlgProc);
                break;

            case IDC_AUTOMAP_PARMS: 
                ModalDialog(IDD_GLOBAL_DRIVES, hDlg, (DLGPROC)AutoMap_DlgProc);
                break;

            case IDC_DIAG_PARMS:
                ModalDialog(IDD_DIAG_PARMS, hDlg, (DLGPROC)Diag_DlgProc);
                break;

            case IDC_CHUNK_SIZE:
               switch (HIWORD(wp))
                  {
                  case SPN_CHANGE_UP:
                     (*(LPDWORD)lp) <<= 1;
                     break;
                  case SPN_CHANGE_DOWN:
                     (*(LPDWORD)lp) >>= 1;
                     break;
                  case SPN_CHANGE:
                     (*(LPDWORD)lp) = 1 << (log2 (*(LPDWORD)lp));
                     break;
                  }
               break;

            case IDHELP:
               AdvancedTab_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_ADVANCED_NT);
         break;
      }

   return FALSE;
}


void AdvancedTab_OnInitDialog (HWND hDlg)
{
   Config_GetCacheSize (&g.Configuration.ckCache);
   Config_GetCachePath (g.Configuration.szCachePath);
   Config_GetChunkSize (&g.Configuration.ckChunk);
   Config_GetStatEntries (&g.Configuration.cStatEntries);

   ULONG ckCacheMin = ckCACHE_MIN;
   ULONG ckCacheMax = ckCACHE_MAX;

   CreateSpinner (GetDlgItem (hDlg, IDC_CACHE_SIZE), 10, FALSE, ckCacheMin, g.Configuration.ckCache, ckCacheMax);
   CreateSpinner (GetDlgItem (hDlg, IDC_CHUNK_SIZE), 10, FALSE, ckCHUNK_MIN, g.Configuration.ckChunk, ckCHUNK_MAX);
   CreateSpinner (GetDlgItem (hDlg, IDC_STAT_ENTRIES), 10, FALSE, cSTATS_MIN, g.Configuration.cStatEntries, cSTATS_MAX);

   SetDlgItemText (hDlg, IDC_CACHE_PATH, g.Configuration.szCachePath);
   SetDlgItemText (hDlg, IDC_SYSNAME, g.Configuration.szSysName);

   AdvancedTab_OnRefresh (hDlg);
}


BOOL AdvancedTab_CommitChanges (BOOL fForce)
{
   HWND hDlg;
   if ((hDlg = PropSheet_FindTabWindow (g.psh, (DLGPROC)AdvancedTab_DlgProc)) == NULL)
      return TRUE;
   if (fForce)
      SetWindowLongPtr (hDlg, DWLP_MSGRESULT, FALSE); // Make sure we try to apply
   if (AdvancedTab_OnApply (hDlg))
      return TRUE;
   SetWindowLongPtr (hDlg, DWLP_MSGRESULT, TRUE);
   return FALSE;
}


BOOL AdvancedTab_OnApply (HWND hDlg)
{
   // Don't try to do anything if we've already failed the apply
   if (GetWindowLongPtr (hDlg, DWLP_MSGRESULT))
      return FALSE;

   ULONG_PTR Value = SP_GetPos (GetDlgItem (hDlg, IDC_CACHE_SIZE));
   if (Value != g.Configuration.ckCache)
      {
      if (!Config_SetCacheSize (Value))
         return FALSE;
      g.Configuration.ckCache = Value;
      }

   Value = SP_GetPos (GetDlgItem (hDlg, IDC_CHUNK_SIZE));
   if (Value != g.Configuration.ckChunk)
      {
      if (!Config_SetChunkSize (Value))
         return FALSE;
      g.Configuration.ckChunk = Value;
      }

   Value = SP_GetPos (GetDlgItem (hDlg, IDC_STAT_ENTRIES));
   if (Value != g.Configuration.cStatEntries)
      {
      if (!Config_SetStatEntries (Value))
         return FALSE;
      g.Configuration.cStatEntries = Value;
      }

   TCHAR szText[ MAX_PATH ];
   GetDlgItemText (hDlg, IDC_CACHE_PATH, szText, MAX_PATH);
   if (lstrcmp (szText, g.Configuration.szCachePath))
      {
      if (!Config_SetCachePath (szText))
         return FALSE;
      lstrcpy (g.Configuration.szCachePath, szText);
      }

   // Save the data from the advanced tab dialogs
   if (!Misc_OnApply())
      return FALSE;
      
   if (!Binding_OnApply())
        return FALSE;
      
   if (!Logon_OnApply())
      return FALSE;
      
   if (!Diag_OnApply())
      return FALSE;

   return TRUE;
}


void AdvancedTab_OnRefresh (HWND hDlg)
{
   ULONG ckCacheInUse;
   if (!Config_GetCacheInUse (&ckCacheInUse))
      ckCacheInUse = 0;

   LPTSTR pszInUse;
   if (ckCacheInUse)
      pszInUse = FormatString (IDS_KB_IN_USE, TEXT("%ld"), ckCacheInUse);
   else
      pszInUse = FormatString (IDS_KB_ONLY);
   SetDlgItemText (hDlg, IDC_INUSE, pszInUse);
   FreeString (pszInUse);
}

