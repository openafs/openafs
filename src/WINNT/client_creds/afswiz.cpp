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
#include <afs/fs_utils.h>
}

#include "afscreds.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK WizCommon_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK WizStart_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK WizStarting_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK WizCreds_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK WizMount_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK WizMounting_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK WizFinish_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void WizStarting_OnInitDialog (HWND hDlg);
void WizStarting_OnTimer (HWND hDlg);

void WizCreds_OnInitDialog (HWND hDlg);
void WizCreds_OnEnable (HWND hDlg, BOOL fAllowEnable = TRUE);

void WizMount_OnInitDialog (HWND hDlg);
void WizMount_OnEnable (HWND hDlg, BOOL fAllowEnable = TRUE);
BOOL WizMount_IsSubmountValid (HWND hDlg);

void WizMounting_OnInitDialog (HWND hDlg);
void WizMounting_OnFinish (HWND hDlg);
DWORD CALLBACK WizMounting_Thread (LPVOID lp);


/*
 * VARIABLES __________________________________________________________________
 *
 */

static struct l
   {
   DRIVEMAPLIST List;
   BOOL fQueried;
   HWND hDlg;
   int idh;
   TCHAR szSubmountReq[ MAX_PATH ];
   } l;


/*
 * WIZARD STATES ______________________________________________________________
 *
 */

typedef enum
   {
   STEP_START,
   STEP_STARTING,
   STEP_CREDS,
   STEP_MOUNT,
   STEP_MOUNTING,
   STEP_FINISH,
   } WIZSTEP;

static WIZARD_STATE aStates[] = {
   { STEP_START,     IDD_WIZ_START,     (DLGPROC)WizStart_DlgProc,	0 },
   { STEP_STARTING,  IDD_WIZ_STARTING,  (DLGPROC)WizStarting_DlgProc,	0 },
   { STEP_CREDS,     IDD_WIZ_CREDS,     (DLGPROC)WizCreds_DlgProc,	0 },
   { STEP_MOUNT,     IDD_WIZ_MOUNT,     (DLGPROC)WizMount_DlgProc,	0 },
   { STEP_MOUNTING,  IDD_WIZ_MOUNTING,  (DLGPROC)WizMounting_DlgProc,	0 },
   { STEP_FINISH,    IDD_WIZ_FINISH,    (DLGPROC)WizFinish_DlgProc,	0 }
};

static const int cStates = sizeof(aStates) / sizeof(aStates[0]);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void ShowStartupWizard (void)
{
   if (g.pWizard != NULL) // Already showing the wizard? Stop here.
      return;

   memset (&l, 0x00, sizeof(l));

   g.pWizard = New(WIZARD);

   g.pWizard->SetDialogTemplate (IDD_WIZARD, IDC_LHS, IDC_RHS, IDBACK, IDNEXT);
   g.pWizard->SetGraphic (IDB_WIZ16, IDB_WIZ256);
   g.pWizard->SetStates (aStates, cStates);
   g.pWizard->SetState (STEP_START);

   g.pWizard->Show();

   Delete(g.pWizard);
   g.pWizard = NULL;

   Main_RepopulateTabs (TRUE);

   FreeDriveMapList (&l.List);
}


BOOL CALLBACK WizCommon_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
     {
     case WM_COMMAND:
        switch (LOWORD(wp))
           {
           case IDCANCEL:
              g.pWizard->Show(FALSE);
              return TRUE;

           case IDHELP:
               WizCommon_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
           }
        break;

      case WM_HELP:
         if (l.idh)
            WinHelp (GetParent(hDlg), g.szHelpFile, HELP_CONTEXT, l.idh);
         break;
     }
   return FALSE;
}


BOOL CALLBACK WizStart_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (!WizCommon_DlgProc (hDlg, msg, wp, lp))
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            g.pWizard->EnableButtons (NEXT_BUTTON);
            g.pWizard->SetButtonText (IDNEXT, IDS_WIZ_NEXT);
            g.pWizard->SetDefaultControl (IDNEXT);
            l.idh = IDH_AFSCREDS_WIZ_START;
            EnableWindow (GetDlgItem (GetParent (hDlg), IDHELP), TRUE);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDNEXT:
                  g.pWizard->SetState (STEP_STARTING);
                  break;

               case IDC_WIZARD:
                  switch (HIWORD(wp))
                     {
                     case wcIS_STATE_DISABLED:
                        return IsServiceRunning();
                     }
                  break;
               }
            break;
         }
      }

   return FALSE;
}


BOOL CALLBACK WizStarting_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (!WizCommon_DlgProc (hDlg, msg, wp, lp))
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            g.pWizard->EnableButtons (0);
            g.pWizard->SetButtonText (IDNEXT, IDS_WIZ_NEXT);
            WizStarting_OnInitDialog (hDlg);
            l.idh = IDH_AFSCREDS_WIZ_START_FAIL;
            EnableWindow (GetDlgItem (GetParent (hDlg), IDHELP), FALSE);
            break;

         case WM_TIMER:
            WizStarting_OnTimer (hDlg);
            break;

         case WM_DESTROY:
            KillTimer (hDlg, ID_WIZARD_TIMER);
            break;

         case IDC_WIZARD:
            switch (HIWORD(wp))
               {
               case wcIS_STATE_DISABLED:
                  return IsServiceRunning();
               }
            break;
         }
      }

   return FALSE;
}


void WizStarting_OnInitDialog (HWND hDlg)
{
   ShowWindow (GetDlgItem (hDlg, IDC_START_FAIL), SW_HIDE);
   ShowWindow (GetDlgItem (hDlg, IDC_START_TRY), SW_SHOW);

   SC_HANDLE hManager;
    if ((hManager = OpenSCManager (NULL, NULL, 
                                   SC_MANAGER_CONNECT |
                                   SC_MANAGER_ENUMERATE_SERVICE |
                                   SC_MANAGER_QUERY_LOCK_STATUS)) != NULL)
      {
      SC_HANDLE hService;
          if ((hService = OpenService (hManager, TEXT("TransarcAFSDaemon"), 
                                       SERVICE_QUERY_STATUS | SERVICE_START)) != NULL)
         {
         if (StartService (hService, 0, 0))
             TestAndDoMapShare(SERVICE_START_PENDING);

         CloseServiceHandle (hService);
         }

      CloseServiceHandle (hManager);
      }

   SetTimer (hDlg, ID_WIZARD_TIMER, (ULONG)cmsecSERVICE, NULL);

   WizStarting_OnTimer (hDlg);
}


void WizStarting_OnTimer (HWND hDlg)
{
   SERVICE_STATUS Status;
   memset (&Status, 0x00, sizeof(Status));
   Status.dwCurrentState = SERVICE_STOPPED;

   SC_HANDLE hManager;
   if ((hManager = OpenSCManager (NULL, NULL, GENERIC_READ)) != NULL)
      {
      SC_HANDLE hService;
      if ((hService = OpenService (hManager, TEXT("TransarcAFSDaemon"), GENERIC_READ)) != NULL)
         {
         QueryServiceStatus (hService, &Status);
         CloseServiceHandle (hService);
		 TestAndDoMapShare(Status.dwCurrentState);
         }

      CloseServiceHandle (hManager);
      }

   if (Status.dwCurrentState == SERVICE_RUNNING)
      {
      g.pWizard->SetState (STEP_CREDS);
      }
   else if (Status.dwCurrentState != SERVICE_START_PENDING)
      {
      KillTimer (hDlg, ID_WIZARD_TIMER);

      ShowWindow (GetDlgItem (hDlg, IDC_START_FAIL), SW_SHOW);
      ShowWindow (GetDlgItem (hDlg, IDC_START_TRY), SW_HIDE);
      EnableWindow (GetDlgItem (GetParent (hDlg), IDHELP), TRUE);
      }
}


BOOL CALLBACK WizCreds_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (!WizCommon_DlgProc (hDlg, msg, wp, lp))
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            g.pWizard->EnableButtons (NEXT_BUTTON);
            g.pWizard->SetButtonText (IDNEXT, IDS_WIZ_NEXT);
            l.idh = IDH_AFSCREDS_WIZ_CREDS;
            EnableWindow (GetDlgItem (GetParent (hDlg), IDHELP), TRUE);
            WizCreds_OnInitDialog (hDlg);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDNEXT:
                  if (!IsDlgButtonChecked (hDlg, IDC_YESCREDS))
                     {
                     g.pWizard->SetState (STEP_MOUNT);
                     }
                  else
                     {
                     TCHAR szCell[ cchRESOURCE ];
                     GetDlgItemText (hDlg, IDC_NEWCREDS_CELL, szCell, cchRESOURCE);
                     TCHAR szUser[ cchRESOURCE ];
                     GetDlgItemText (hDlg, IDC_NEWCREDS_USER, szUser, cchRESOURCE);
                     TCHAR szPassword[ cchRESOURCE ];
                     GetDlgItemText (hDlg, IDC_NEWCREDS_PASSWORD, szPassword, cchRESOURCE);

                     WizCreds_OnEnable (hDlg, FALSE);

                     if (ObtainNewCredentials (szCell, szUser, szPassword, FALSE) == 0)
                        {
                        g.pWizard->SetState (STEP_MOUNT);
                        }
                     else
                        {
                        WizCreds_OnEnable (hDlg);
                        }
                     }
                  break;

               case IDC_NOCREDS:
               case IDC_YESCREDS:
               case IDC_NEWCREDS_CELL:
               case IDC_NEWCREDS_USER:
               case IDC_NEWCREDS_PASSWORD:
                  WizCreds_OnEnable (hDlg);
                  break;
               }
            break;
         }
      }

   return FALSE;
}


void WizCreds_OnInitDialog (HWND hDlg)
{
   HKEY hk;

   TCHAR szCell[ cchRESOURCE ] = TEXT("");
   SetDlgItemText (hDlg, IDC_NEWCREDS_CELL, szCell);

   TCHAR szUser[ cchRESOURCE ] = TEXT("");
   if (RegOpenKey (HKEY_LOCAL_MACHINE, TEXT("Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon"), &hk) == 0)
      {
      DWORD dwSize = sizeof(szUser);
      DWORD dwType = REG_SZ;
      RegQueryValueEx (hk, TEXT("DefaultUserName"), NULL, &dwType, (PBYTE)szUser, &dwSize);
      RegCloseKey (hk);
      }
   SetDlgItemText (hDlg, IDC_NEWCREDS_USER, szUser);
   g.pWizard->SetDefaultControl (IDC_NEWCREDS_PASSWORD);

   CheckDlgButton (hDlg, IDC_NOCREDS, FALSE);
   CheckDlgButton (hDlg, IDC_YESCREDS, TRUE);
   WizCreds_OnEnable (hDlg);
}


void WizCreds_OnEnable (HWND hDlg, BOOL fAllowEnable)
{
   BOOL fEnable = fAllowEnable;

   if (IsDlgButtonChecked (hDlg, IDC_YESCREDS))
      {
      TCHAR szText[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_NEWCREDS_CELL, szText, cchRESOURCE);
      if (!szText[0])
         fEnable = FALSE;

      GetDlgItemText (hDlg, IDC_NEWCREDS_USER, szText, cchRESOURCE);
      if (!szText[0])
         fEnable = FALSE;

      GetDlgItemText (hDlg, IDC_NEWCREDS_PASSWORD, szText, cchRESOURCE);
      if (!szText[0])
         fEnable = FALSE;
      }

   g.pWizard->EnableButtons ((fEnable) ? NEXT_BUTTON : 0);

   EnableWindow (GetDlgItem (hDlg, IDC_NOCREDS), fAllowEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_YESCREDS), fAllowEnable);

   fEnable = fAllowEnable && IsDlgButtonChecked (hDlg, IDC_YESCREDS);
   EnableWindow (GetDlgItem (hDlg, IDC_NEWCREDS_CELL), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_NEWCREDS_USER), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_NEWCREDS_PASSWORD), fEnable);
}


BOOL CALLBACK WizMount_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (!WizCommon_DlgProc (hDlg, msg, wp, lp))
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            g.pWizard->EnableButtons (BACK_BUTTON | NEXT_BUTTON);
            g.pWizard->SetButtonText (IDNEXT, IDS_WIZ_NEXT);
            g.pWizard->SetDefaultControl (IDNEXT);
            l.idh = IDH_AFSCREDS_WIZ_MOUNT;
            EnableWindow (GetDlgItem (GetParent (hDlg), IDHELP), TRUE);

            WizMount_OnInitDialog (hDlg);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDBACK:
                  g.pWizard->SetState (STEP_CREDS);
                  break;

               case IDNEXT:
                  if (IsDlgButtonChecked (hDlg, IDC_YESMAP))
                     {
                     if (!WizMount_IsSubmountValid (hDlg))
                        {
                        Message (MB_ICONHAND, IDS_BADSUB_TITLE, IDS_BADSUB_DESC);
                        break;
                        }
                     int iItem = SendDlgItemMessage (hDlg, IDC_MAP_LETTER, CB_GETCURSEL, 0, 0);
                     int iDrive = SendDlgItemMessage (hDlg, IDC_MAP_LETTER, CB_GETITEMDATA, iItem, 0);
                     l.List.aDriveMap[ iDrive ].chDrive = chDRIVE_A + iDrive;
                     l.List.aDriveMap[ iDrive ].fActive = FALSE;
                     l.List.aDriveMap[ iDrive ].fPersistent = TRUE;
                     GetDlgItemText (hDlg, IDC_MAP_PATH, l.List.aDriveMap[ iDrive ].szMapping, MAX_PATH);
                     GetDlgItemText (hDlg, IDC_MAP_DESC, l.szSubmountReq, MAX_PATH);
                     WriteDriveMappings (&l.List);
                     }
                  g.pWizard->SetState (STEP_MOUNTING);
                  break;

               case IDC_NOMAP:
               case IDC_YESMAP:
               case IDC_MAP_PATH:
                  WizMount_OnEnable (hDlg);
                  break;
               }
            break;
         }
      }

   return FALSE;
}


void WizMount_OnInitDialog (HWND hDlg)
{
   QueryDriveMapList (&l.List);

   size_t cMap = 0;
   for (size_t iDrive = 0; iDrive < 26; ++iDrive)
      {
      if (l.List.aDriveMap[iDrive].szMapping[0])
         ++cMap;
      }

   if (cMap)
      {
      g.pWizard->SetState (STEP_MOUNTING);
      return;
      }

   // Fill in the combo box
   //
   DWORD dwDrives = GetLogicalDrives() | 0x07; // Always pretend A,B,C: are used

   int iItemSel = -1;
   HWND hCombo = GetDlgItem (hDlg, IDC_MAP_LETTER);
   SendMessage (hCombo, WM_SETREDRAW, FALSE, 0);

   for (int ii = 0; ii < 26; ++ii)
      {
      if (!(dwDrives & (1<<ii)))
         {
         TCHAR szText[ cchRESOURCE ];
         GetString (szText, IDS_MAP_LETTER);

         LPTSTR pch;
         if ((pch = (LPTSTR)lstrchr (szText, TEXT('*'))) != NULL)
            *pch = TEXT('A') + ii;

         int iItem = SendMessage (hCombo, CB_ADDSTRING, 0, (LPARAM)szText);
         SendMessage (hCombo, CB_SETITEMDATA, iItem, ii);
         if (iItemSel == -1)
            iItemSel = iItem;
         }
      }

   SendMessage (hCombo, WM_SETREDRAW, TRUE, 0);
   SendMessage (hCombo, CB_SETCURSEL, iItemSel, 0);

   SetDlgItemText (hDlg, IDC_MAP_PATH, cm_slash_mount_root);

   CheckDlgButton (hDlg, IDC_NOMAP, FALSE);
   CheckDlgButton (hDlg, IDC_YESMAP, TRUE);
}


void WizMount_OnEnable (HWND hDlg, BOOL fAllowEnable)
{
   BOOL fEnable = fAllowEnable;

   if (IsDlgButtonChecked (hDlg, IDC_YESMAP))
      {
      TCHAR szText[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_MAP_PATH, szText, cchRESOURCE);
      if (!szText[0])
         fEnable = FALSE;
      }

   EnableWindow (GetDlgItem (hDlg, IDC_NOMAP), fAllowEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_YESMAP), fAllowEnable);

   EnableWindow (GetDlgItem (hDlg, IDNEXT), fEnable);
   g.pWizard->EnableButtons ((fEnable) ? (BACK_BUTTON | NEXT_BUTTON) : (BACK_BUTTON));

   fEnable = fAllowEnable && (!IsDlgButtonChecked (hDlg, IDC_NOMAP));

   EnableWindow (GetDlgItem (hDlg, IDC_MAP_LETTER), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_MAP_PATH), fEnable);
}


BOOL WizMount_IsSubmountValid (HWND hDlg)
{
   TCHAR szSubmount[ MAX_PATH ];
   GetDlgItemText (hDlg, IDC_MAP_DESC, szSubmount, MAX_PATH);

   if (!szSubmount[0])
      return TRUE;

   return IsValidSubmountName (szSubmount);
}


BOOL CALLBACK WizMounting_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (!WizCommon_DlgProc (hDlg, msg, wp, lp))
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            g.pWizard->EnableButtons (0);
            g.pWizard->SetButtonText (IDNEXT, IDS_WIZ_NEXT);
            g.pWizard->SetDefaultControl (IDNEXT);
            l.idh = IDH_AFSCREDS_WIZ_MOUNT_FAIL;
            EnableWindow (GetDlgItem (GetParent (hDlg), IDHELP), FALSE);
            WizMounting_OnInitDialog (hDlg);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDNEXT:
                  g.pWizard->SetState (STEP_FINISH);
                  break;

               case IDC_MAP_FINISH:
                  WizMounting_OnFinish (hDlg);
                  break;

               case IDC_NOMAP:
               case IDC_YESMAP:
               case IDC_MAP_PATH:
                  WizMount_OnEnable (hDlg);
                  break;

               case IDC_WIZARD:
                  switch (HIWORD(wp))
                     {
                     case wcIS_STATE_DISABLED:
                        size_t ii;
                        for (ii = 0; ii < 26; ++ii)
                           {
                           if (!l.List.aDriveMap[ ii ].szMapping[0])
                              continue;
                           if (l.List.aDriveMap[ ii ].fActive)
                              continue;
                           return FALSE;
                           }
                        return TRUE;
                     }
                  break;
               }
            break;
         }
      }

   return FALSE;
}


void WizMounting_OnInitDialog (HWND hDlg)
{
   l.hDlg = hDlg;
   ShowWindow (GetDlgItem (hDlg, IDC_MAP_TRY), SW_SHOW);
   ShowWindow (GetDlgItem (hDlg, IDC_MAP_FAIL), SW_HIDE);

   DWORD idThread;
   CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)WizMounting_Thread, (LPVOID)0, 0, &idThread);
}


void WizMounting_OnFinish (HWND hDlg)
{
   FreeDriveMapList (&l.List);
   QueryDriveMapList (&l.List);

   size_t cInactive = 0;
   for (size_t iDrive = 0; iDrive < 26; ++iDrive)
      {
      if (!l.List.aDriveMap[iDrive].szMapping[0])
         continue;
      if (l.List.aDriveMap[iDrive].fActive)
         continue;
      ++cInactive;
      }

   if (!cInactive)
      {
      g.pWizard->SetState (STEP_FINISH);
      }
   else
      {
      ShowWindow (GetDlgItem (hDlg, IDC_MAP_TRY), SW_HIDE);
      ShowWindow (GetDlgItem (hDlg, IDC_MAP_FAIL), SW_SHOW);
      EnableWindow (GetDlgItem (GetParent (hDlg), IDHELP), TRUE);
      }
}


DWORD CALLBACK WizMounting_Thread (LPVOID lp)
{
   size_t cInactive = 0;
   for (size_t iDrive = 0; iDrive < 26; ++iDrive)
      {
      if (!l.List.aDriveMap[iDrive].szMapping[0])
         continue;
      if (l.List.aDriveMap[iDrive].fActive)
         continue;

      DWORD status;
      (void)ActivateDriveMap (l.List.aDriveMap[iDrive].chDrive, l.List.aDriveMap[iDrive].szMapping, l.szSubmountReq, l.List.aDriveMap[iDrive].fPersistent, &status);
      l.szSubmountReq[0] = TEXT('\0');
      }

   PostMessage (l.hDlg, WM_COMMAND, IDC_MAP_FINISH, 0);
   return 0;
}


BOOL CALLBACK WizFinish_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (!WizCommon_DlgProc (hDlg, msg, wp, lp))
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            g.pWizard->EnableButtons (NEXT_BUTTON);
            g.pWizard->SetButtonText (IDNEXT, IDS_WIZ_FINISH);
            g.pWizard->SetDefaultControl (IDNEXT);
            EnableWindow (GetDlgItem (GetParent (hDlg), IDCANCEL), FALSE);
            EnableWindow (GetDlgItem (GetParent (hDlg), IDHELP), FALSE);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDNEXT:
                  g.pWizard->Show (FALSE);
                  break;
               }
            break;
         }
      }

   return FALSE;
}

