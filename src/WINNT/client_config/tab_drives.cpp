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

#include "afs_config.h"
#include "tab_drives.h"
#include <lanahelper.h>

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void DrivesTab_OnInitDialog (HWND hDlg);
void DrivesTab_OnSelect (HWND hDlg);
void DrivesTab_OnCheck (HWND hDlg);
void DrivesTab_OnAdd (HWND hDlg);
void DrivesTab_OnEdit (HWND hDlg);
void DrivesTab_OnRemove (HWND hDlg);
void DrivesTab_OnAdvanced (HWND hDlg);

void DrivesTab_Enable (HWND hDlg);
int DrivesTab_DriveFromItem (HWND hDlg, int iItem);
void DrivesTab_FillList (HWND hDlg);
void DrivesTab_EditMapping (HWND hDlg, int iDrive);

BOOL CALLBACK DriveEdit_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void DriveEdit_OnInitDialog (HWND hDlg);
void DriveEdit_OnOK (HWND hDlg);
void DriveEdit_Enable (HWND hDlg);

BOOL CALLBACK Submounts_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Submounts_OnInitDialog (HWND hDlg);
void Submounts_OnApply (HWND hDlg);
void Submounts_OnSelect (HWND hDlg);
void Submounts_OnAdd (HWND hDlg);
void Submounts_OnEdit (HWND hDlg);
void Submounts_OnRemove (HWND hDlg);
void Submounts_EditSubmount (HWND hDlg, PSUBMOUNT pSubmount);

BOOL CALLBACK SubEdit_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void SubEdit_OnInitDialog (HWND hDlg);
void SubEdit_OnOK (HWND hDlg);
void SubEdit_Enable (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK DrivesTab_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         DrivesTab_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_REFRESH:
               DrivesTab_Enable (hDlg);
               break;

            case IDC_LIST:
               if (HIWORD(wp) == LBN_CLICKED)
                  DrivesTab_OnCheck (hDlg);
               else if ((HIWORD(wp) == LBN_SELCHANGE) || (HIWORD(wp) == LBN_SELCANCEL))
                  DrivesTab_OnSelect (hDlg);
               break;

            case IDC_ADD:
               DrivesTab_OnAdd (hDlg);
               break;

            case IDC_EDIT:
               DrivesTab_OnEdit (hDlg);
               break;

            case IDC_REMOVE:
               DrivesTab_OnRemove (hDlg);
               break;

            case IDC_ADVANCED:
               DrivesTab_OnAdvanced (hDlg);
               break;

            case IDHELP:
               DrivesTab_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_DRIVES);
         break;
      }

   return FALSE;
}


void DrivesTab_OnInitDialog (HWND hDlg)
{
   ShowWindow (GetDlgItem (hDlg, IDC_ADVANCED), g.fIsWinNT);

   DrivesTab_FillList (hDlg);
}


void DrivesTab_OnSelect (HWND hDlg)
{
   if (IsWindowEnabled (GetDlgItem (hDlg, IDC_LIST)))
      {
      UINT iSel = SendDlgItemMessage (hDlg, IDC_LIST, LB_GETCURSEL, 0, 0);

      EnableWindow (GetDlgItem (hDlg, IDC_EDIT), (iSel != -1));
      EnableWindow (GetDlgItem (hDlg, IDC_REMOVE), (iSel != -1));
      }
}


void DrivesTab_OnCheck (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   int iItemSel = SendMessage (hList, LB_GETCURSEL, 0, 0);
   int iDriveSel = DrivesTab_DriveFromItem (hDlg, iItemSel);
   BOOL fChecked = SendMessage (hList, LB_GETITEMDATA, iItemSel, 0);

   if (iDriveSel != -1)
      {
      DWORD dwStatus;
      if (fChecked && g.Configuration.NetDrives.aDriveMap[ iDriveSel ].szMapping[0] && !g.Configuration.NetDrives.aDriveMap[ iDriveSel ].fActive)
         {
         if (!ActivateDriveMap (g.Configuration.NetDrives.aDriveMap[ iDriveSel ].chDrive, g.Configuration.NetDrives.aDriveMap[ iDriveSel ].szMapping, g.Configuration.NetDrives.aDriveMap[ iDriveSel ].szSubmount, g.Configuration.NetDrives.aDriveMap[ iDriveSel ].fPersistent, &dwStatus))
            Message (MB_OK | MB_ICONHAND, IDS_ERROR_MAP, IDS_ERROR_MAP_DESC, TEXT("%08lX"), dwStatus);
         DrivesTab_FillList (hDlg);
         }
      else if (!fChecked && g.Configuration.NetDrives.aDriveMap[ iDriveSel ].fActive)
         {
         if (!InactivateDriveMap (g.Configuration.NetDrives.aDriveMap[ iDriveSel ].chDrive, &dwStatus))
            Message (MB_OK | MB_ICONHAND, IDS_ERROR_UNMAP, IDS_ERROR_UNMAP_DESC, TEXT("%08lX"), dwStatus);
         DrivesTab_FillList (hDlg);
         }
      WriteActiveMap(g.Configuration.NetDrives.aDriveMap[ iDriveSel ].chDrive, fChecked && 
                     g.Configuration.NetDrives.aDriveMap[ iDriveSel ].fPersistent );

      }
}


void DrivesTab_OnAdd (HWND hDlg)
{
   DrivesTab_EditMapping (hDlg, -1);
}


void DrivesTab_OnEdit (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   int iItemSel = SendMessage (hList, LB_GETCURSEL, 0, 0);
   int iDriveSel = DrivesTab_DriveFromItem (hDlg, iItemSel);

   DrivesTab_EditMapping (hDlg, iDriveSel);
}


void DrivesTab_OnRemove (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   int iItemSel = SendMessage (hList, LB_GETCURSEL, 0, 0);
   int iDriveSel = DrivesTab_DriveFromItem (hDlg, iItemSel);

   if (iDriveSel != -1)
      {
      if (g.Configuration.NetDrives.aDriveMap[ iDriveSel ].szMapping[0])
         {
         if (g.Configuration.NetDrives.aDriveMap[ iDriveSel ].fActive)
            {
            DWORD dwStatus;
            if (!InactivateDriveMap (g.Configuration.NetDrives.aDriveMap[ iDriveSel ].chDrive, &dwStatus))
               {
               Message (MB_OK | MB_ICONHAND, IDS_ERROR_UNMAP, IDS_ERROR_UNMAP_DESC, TEXT("%08lX"), dwStatus);
               return;
               }
            }
         g.Configuration.NetDrives.aDriveMap[ iDriveSel ].szMapping[0] = TEXT('\0');
         WriteDriveMappings (&g.Configuration.NetDrives);

         DrivesTab_FillList (hDlg);
         }
      WriteActiveMap(g.Configuration.NetDrives.aDriveMap[ iDriveSel ].chDrive, FALSE );
      }
}


void DrivesTab_OnAdvanced (HWND hDlg)
{
   TCHAR szTitle[ cchRESOURCE ];
   GetString (szTitle, IDS_SUBMOUNTS_TITLE);

   LPPROPSHEET psh = PropSheet_Create (szTitle, FALSE, GetParent(hDlg), 0);
   psh->sh.dwFlags |= PSH_NOAPPLYNOW;  // Remove the Apply button
   psh->sh.dwFlags |= PSH_HASHELP;     // Add a Help button instead
   PropSheet_AddTab (psh, szTitle, IDD_SUBMOUNTS, (DLGPROC)Submounts_DlgProc, 0, TRUE);
   PropSheet_ShowModal (psh);
}


void DrivesTab_Enable (HWND hDlg)
{
   BOOL fRunning = (Config_GetServiceState() == SERVICE_RUNNING);

   EnableWindow (GetDlgItem (hDlg, IDC_LIST), fRunning);
   EnableWindow (GetDlgItem (hDlg, IDC_ADD), fRunning);
   EnableWindow (GetDlgItem (hDlg, IDC_EDIT), fRunning);
   EnableWindow (GetDlgItem (hDlg, IDC_REMOVE), fRunning);

   TCHAR szText[ cchRESOURCE ];
   GetString (szText, (fRunning) ? IDS_TIP_DRIVES : IDS_WARN_STOPPED);
   SetDlgItemText (hDlg, IDC_WARN, szText);
}


int DrivesTab_DriveFromItem (HWND hDlg, int iItem)
{
   TCHAR szItem[ 1024 ] = TEXT("");
   SendDlgItemMessage (hDlg, IDC_LIST, LB_GETTEXT, iItem, (LPARAM)szItem);

   LPTSTR pch;
   if ((pch = (LPTSTR)lstrchr (szItem, TEXT(':'))) != NULL)
      {
      if (pch > szItem)
         {
         pch--;
         if ((*pch >= TEXT('A')) && (*pch <= TEXT('Z')))
            return (*pch) - TEXT('A');
         }
      }

   return -1;
}


void DrivesTab_FillList (HWND hDlg)
{
   FreeDriveMapList (&g.Configuration.NetDrives);
   QueryDriveMapList (&g.Configuration.NetDrives);

   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   int iItemSel = SendMessage (hList, LB_GETCURSEL, 0, 0);
   int iDriveSel = DrivesTab_DriveFromItem (hDlg, iItemSel);
   SendMessage (hList, WM_SETREDRAW, FALSE, 0);
   SendMessage (hList, LB_RESETCONTENT, 0, 0);

   iItemSel = -1;

   for (int iDrive = 0; iDrive < 26; ++iDrive)
      {
      if (!g.Configuration.NetDrives.aDriveMap[ iDrive ].szMapping[0])
         continue;

      TCHAR szAfsPath[ MAX_PATH ];
      AdjustAfsPath (szAfsPath, g.Configuration.NetDrives.aDriveMap[ iDrive ].szMapping, TRUE, FALSE);

      LPTSTR psz = FormatString (IDS_DRIVE_MAP, TEXT("%c%s"), g.Configuration.NetDrives.aDriveMap[ iDrive ].chDrive, szAfsPath);
      int iItem = SendMessage (hList, LB_ADDSTRING, 0, (LPARAM)psz);
      SendMessage (hList, LB_SETITEMDATA, iItem, g.Configuration.NetDrives.aDriveMap[ iDrive ].fActive);

      if (iDrive == iDriveSel)
         iItemSel = iItem;
      }

   SendMessage (hList, WM_SETREDRAW, TRUE, 0);
   if (iItemSel != -1)
      SendMessage (hList, LB_SETCURSEL, iItemSel, 0);

   DrivesTab_Enable (hDlg);
   DrivesTab_OnSelect (hDlg);
}


void DrivesTab_EditMapping (HWND hDlg, int iDrive)
{
   DRIVEMAP DriveMapOrig;
   memset (&DriveMapOrig, 0x00, sizeof(DRIVEMAP));

   if (iDrive != -1)
      {
      memcpy (&DriveMapOrig, &g.Configuration.NetDrives.aDriveMap[ iDrive ], sizeof(DRIVEMAP));
      }

   DRIVEMAP DriveMap;
   memcpy (&DriveMap, &DriveMapOrig, sizeof(DRIVEMAP));

   if (ModalDialogParam (IDD_DRIVE_EDIT, GetParent(hDlg), (DLGPROC)DriveEdit_DlgProc, (LPARAM)&DriveMap) == IDOK)
      {
      TCHAR szAfsPathOrig[ MAX_PATH ] = TEXT("");
      if (iDrive != -1)
         AdjustAfsPath (szAfsPathOrig, DriveMapOrig.szMapping, TRUE, TRUE);

      TCHAR szAfsPathNew[ MAX_PATH ];
      AdjustAfsPath (szAfsPathNew, DriveMap.szMapping, TRUE, TRUE);

      if ( (lstrcmpi (szAfsPathOrig, szAfsPathNew)) ||
           (lstrcmpi (DriveMapOrig.szSubmount, DriveMap.szSubmount)) ||
           (DriveMapOrig.chDrive != DriveMap.chDrive) ||
           (DriveMapOrig.fPersistent != DriveMap.fPersistent) )
         {
         DWORD dwStatus;

         if ((iDrive != -1) && (DriveMapOrig.fActive))
            {
            if (!InactivateDriveMap (DriveMapOrig.chDrive, &dwStatus))
               {
               Message (MB_OK | MB_ICONHAND, IDS_ERROR_UNMAP, IDS_ERROR_UNMAP_DESC, TEXT("%08lX"), dwStatus);
               DrivesTab_FillList (hDlg);
               return;
               }
            }

         if (!ActivateDriveMap (DriveMap.chDrive, szAfsPathNew, DriveMap.szSubmount, DriveMap.fPersistent, &dwStatus))
            {
            Message (MB_OK | MB_ICONHAND, IDS_ERROR_MAP, IDS_ERROR_MAP_DESC, TEXT("%08lX"), dwStatus);
            DrivesTab_FillList (hDlg);
            return;
            }

         if (DriveMap.szSubmount[0])
            {
            TCHAR szSubmountNow[ MAX_PATH ];
            if (GetDriveSubmount (DriveMap.chDrive, szSubmountNow))
               {
               if (lstrcmpi (DriveMap.szSubmount, szSubmountNow))
                  Message (MB_OK | MB_ICONASTERISK, GetCautionTitle(), IDS_NEWSUB_DESC);
               }
            }

         if (iDrive != -1)
            memset (&g.Configuration.NetDrives.aDriveMap[ iDrive ], 0x00, sizeof(DRIVEMAP));
         memcpy (&g.Configuration.NetDrives.aDriveMap[ DriveMap.chDrive-chDRIVE_A ], &DriveMap, sizeof(DRIVEMAP));
         lstrcpy (g.Configuration.NetDrives.aDriveMap[ DriveMap.chDrive-chDRIVE_A ].szMapping, szAfsPathNew);
         WriteDriveMappings (&g.Configuration.NetDrives);

         if (iDrive == -1) {
             WriteActiveMap(DriveMap.chDrive, DriveMap.fPersistent);
         } else if ( (chDRIVE_A + iDrive) != DriveMap.chDrive ) {
             WriteActiveMap(chDRIVE_A+iDrive, FALSE);
             WriteActiveMap(DriveMap.chDrive, DriveMap.fPersistent);
         }

         DrivesTab_FillList (hDlg);
         }
      }
}


BOOL CALLBACK DriveEdit_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLong (hDlg, DWL_USER, lp);
         DriveEdit_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               DriveEdit_OnOK (hDlg);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDC_PATH:
               DriveEdit_Enable (hDlg);
               break;

            case IDHELP:
               DriveEdit_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_DRIVES_ADDEDIT);
         break;
      }
   return FALSE;
}

void DriveEdit_OnInitDialog (HWND hDlg)
{
   PDRIVEMAP pMap = (PDRIVEMAP)GetWindowLong (hDlg, DWL_USER);

   // Fill in the combo box
   //
   DWORD dwDrives = GetLogicalDrives() | 0x07; // Always pretend A,B,C: are used

   if (pMap->chDrive != 0)
      dwDrives &= ~( 1 << (pMap->chDrive - chDRIVE_A) );

   int iItemSel = -1;
   HWND hCombo = GetDlgItem (hDlg, IDC_DRIVE);
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
         if (pMap->chDrive && (ii == pMap->chDrive - chDRIVE_A))
            iItemSel = iItem;
         else if ((!pMap->chDrive) && (iItemSel == -1))
            iItemSel = iItem;
         }
      }

   SendMessage (hCombo, WM_SETREDRAW, TRUE, 0);
   SendMessage (hCombo, CB_SETCURSEL, iItemSel, 0);

   TCHAR szMapping[ MAX_PATH ];
   AdjustAfsPath (szMapping, ((pMap->szMapping[0]) ? pMap->szMapping : cm_slash_mount_root), TRUE, FALSE);
   
   CHAR msg[256], msgf[256];
   if (GetDlgItemText(hDlg,IDC_STATICSUBMOUNT,(LPSTR)msg,sizeof(msg)-1)>0)
   {
       wsprintf(msgf,msg,cm_back_slash_mount_root,cm_back_slash_mount_root);
       SetDlgItemText (hDlg, IDC_STATICSUBMOUNT, msgf);
   }
   SetDlgItemText (hDlg, IDC_PATH, szMapping);
   SetDlgItemText (hDlg, IDC_DESC, pMap->szSubmount);

   CheckDlgButton (hDlg, IDC_PERSISTENT, (pMap->chDrive == 0) ? TRUE : (pMap->fPersistent));

   DriveEdit_Enable (hDlg);
}


void DriveEdit_OnOK (HWND hDlg)
{
   PDRIVEMAP pMap = (PDRIVEMAP)GetWindowLong (hDlg, DWL_USER);

   int iItem = SendDlgItemMessage (hDlg, IDC_DRIVE, CB_GETCURSEL, 0, 0);
   int iDrive = SendDlgItemMessage (hDlg, IDC_DRIVE, CB_GETITEMDATA, iItem, 0);

   pMap->chDrive = chDRIVE_A + iDrive;
   GetDlgItemText (hDlg, IDC_PATH, pMap->szMapping, MAX_PATH);
   GetDlgItemText (hDlg, IDC_DESC, pMap->szSubmount, MAX_PATH);
   pMap->fPersistent = IsDlgButtonChecked (hDlg, IDC_PERSISTENT);

   if (pMap->szSubmount[0] && !IsValidSubmountName (pMap->szSubmount))
      {
      Message (MB_ICONHAND, GetErrorTitle(), IDS_BADSUB_DESC);
      return;
      }

    if ( (lstrncmpi (pMap->szMapping, cm_slash_mount_root, lstrlen(cm_slash_mount_root))) &&
         (lstrncmpi (pMap->szMapping, cm_back_slash_mount_root, lstrlen(cm_back_slash_mount_root))) )
    {
      Message (MB_ICONHAND, GetErrorTitle(), IDS_BADMAP_DESC);
      return;
      }

   EndDialog (hDlg, IDOK);
}


void DriveEdit_Enable (HWND hDlg)
{
   TCHAR szPath[ MAX_PATH ];
   GetDlgItemText (hDlg, IDC_PATH, szPath, MAX_PATH);
   EnableWindow (GetDlgItem (hDlg, IDOK), (szPath[0] != TEXT('\0')));
}


BOOL CALLBACK Submounts_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         Submounts_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               Submounts_OnApply (hDlg);
               break;

            case IDC_ADD:
               Submounts_OnAdd (hDlg);
               break;

            case IDC_EDIT:
               Submounts_OnEdit (hDlg);
               break;

            case IDC_REMOVE:
               Submounts_OnRemove (hDlg);
               break;

            case IDHELP:
               Submounts_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_SUBMOUNTS_NT);
         break;

      case WM_NOTIFY:
         switch (((LPNMHDR)lp)->code)
            {
            case FLN_ITEMSELECT:
               Submounts_OnSelect (hDlg);
               break;

            case FLN_LDBLCLICK:
               if (IsWindowEnabled (GetDlgItem (hDlg, IDC_EDIT)))
                  Submounts_OnEdit (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Submounts_OnInitDialog (HWND hDlg)
{
   // Prepare the columns on the server list
   //
   HWND hList = GetDlgItem (hDlg, IDC_LIST);

   FASTLISTCOLUMN Column;
   Column.dwFlags = FLCF_JUSTIFY_LEFT;
   Column.cxWidth = 100;
   GetString (Column.szText, IDS_SUBCOL_SHARE);
   FastList_SetColumn (hList, 0, &Column);

   Column.dwFlags = FLCF_JUSTIFY_LEFT;
   Column.cxWidth = 200;
   GetString (Column.szText, IDS_SUBCOL_PATH);
   FastList_SetColumn (hList, 1, &Column);

   // Remove the Context Help [?] thing from the title bar
   //
   DWORD dwStyle = GetWindowLong (GetParent (hDlg), GWL_STYLE);
   dwStyle &= ~DS_CONTEXTHELP;
   SetWindowLong (GetParent (hDlg), GWL_STYLE, dwStyle);

   dwStyle = GetWindowLong (GetParent (hDlg), GWL_EXSTYLE);
   dwStyle &= ~WS_EX_CONTEXTHELP;
   SetWindowLong (GetParent (hDlg), GWL_EXSTYLE, dwStyle);

   // Fill in the list of submounts
   //
   FastList_Begin (hList);

   for (size_t ii = 0; ii < g.Configuration.NetDrives.cSubmounts; ++ii)
      {
      if (!g.Configuration.NetDrives.aSubmounts[ ii ].szSubmount[0])
         continue;

      FASTLISTADDITEM ai;
      memset (&ai, 0x00, sizeof(FASTLISTADDITEM));
      ai.iFirstImage = IMAGE_NOIMAGE;
      ai.iSecondImage = IMAGE_NOIMAGE;
      ai.pszText = g.Configuration.NetDrives.aSubmounts[ ii ].szSubmount;
      ai.lParam = 0;
      HLISTITEM hItem = FastList_AddItem (hList, &ai);

      TCHAR szMapping[ MAX_PATH ];
      AdjustAfsPath (szMapping, g.Configuration.NetDrives.aSubmounts[ ii ].szMapping, TRUE, FALSE);
      FastList_SetItemText (hList, hItem, 1, szMapping);
      }

   FastList_End (hList);
   Submounts_OnSelect (hDlg);
}


void Submounts_OnApply (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);

   // Remove our current list of submounts
   //
   for (size_t ii = 0; ii < g.Configuration.NetDrives.cSubmounts; ++ii)
      {
      RemoveSubMount (g.Configuration.NetDrives.aSubmounts[ ii ].szSubmount);
      }

   // Add back all our new submounts
   //
   HLISTITEM hItem;
   for (hItem = FastList_FindFirst (hList); hItem; hItem = FastList_FindNext (hList, hItem))
      {
      LPCTSTR pszSubmount;
      if ((pszSubmount = FastList_GetItemText (hList, hItem, 0)) == NULL)
         continue;
      LPCTSTR pszMapping;
      if ((pszMapping = FastList_GetItemText (hList, hItem, 1)) == NULL)
         continue;

      AddSubMount ((LPTSTR)pszSubmount, (LPTSTR)pszMapping);
      }

   FreeDriveMapList (&g.Configuration.NetDrives);
   QueryDriveMapList (&g.Configuration.NetDrives);
   if (g.Configuration.fLogonAuthent)
	   DoMapShareChange();
}


void Submounts_OnSelect (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);

   size_t cSelected = 0;
   size_t cSelectedInUse = 0;

   HLISTITEM hItem;
   for (hItem = FastList_FindFirstSelected (hList); hItem; hItem = FastList_FindNextSelected (hList, hItem))
      {
      cSelected++;

      LPCTSTR pszSubmount;
      if ((pszSubmount = FastList_GetItemText (hList, hItem, 0)) != NULL)
         {
         for (size_t ii = 0; ii < g.Configuration.NetDrives.cSubmounts; ++ii)
            {
            if (!lstrcmpi (pszSubmount, g.Configuration.NetDrives.aSubmounts[ii].szSubmount))
               {
               if (g.Configuration.NetDrives.aSubmounts[ii].fInUse)
                  cSelectedInUse++;
               }
            }
         }
      }

   EnableWindow (GetDlgItem (hDlg, IDC_REMOVE), (cSelected != 0) && (!cSelectedInUse));
   EnableWindow (GetDlgItem (hDlg, IDC_EDIT), (cSelected == 1) && (!cSelectedInUse));
}


void Submounts_OnAdd (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);

   SUBMOUNT Submount;
   memset (&Submount, 0x00, sizeof(Submount));

   Submounts_EditSubmount (hDlg, &Submount);
}


void Submounts_OnEdit (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);

   HLISTITEM hItem;
   if ((hItem = FastList_FindFirstSelected (hList)) != NULL)
      {
      LPCTSTR pszSubmount = FastList_GetItemText (hList, hItem, 0);
      LPCTSTR pszMapping = FastList_GetItemText (hList, hItem, 1);

      SUBMOUNT Submount;
      memset (&Submount, 0x00, sizeof(Submount));
      lstrcpy (Submount.szSubmount, pszSubmount);
      lstrcpy (Submount.szMapping, pszMapping);

      Submounts_EditSubmount (hDlg, &Submount);
      }
}

// Action - On Remove submount item

void Submounts_OnRemove (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   FastList_Begin (hList);

   HLISTITEM hItem;
   while ((hItem = FastList_FindFirstSelected (hList)) != NULL)
      {
      FastList_RemoveItem (hList, hItem);
      }

   FastList_End (hList);
}


// Action - On Add or On Edit a submount item
void Submounts_EditSubmount (HWND hDlg, PSUBMOUNT pSubmount)
{
    TCHAR szOrigSubmount[MAX_PATH];
    _tcscpy(szOrigSubmount, pSubmount->szSubmount);

    HWND hList = GetDlgItem (hDlg, IDC_LIST);

    if (ModalDialogParam (IDD_SUBMOUNT_EDIT, GetParent(hDlg), (DLGPROC)SubEdit_DlgProc, (LPARAM)pSubmount) == IDOK)
    {
        TCHAR szMapping[ MAX_PATH ];
        BOOL bNameChange = (szOrigSubmount[0] && _tcsicmp(szOrigSubmount, pSubmount->szSubmount));

        AdjustAfsPath (szMapping, pSubmount->szMapping, TRUE, FALSE);

        HLISTITEM hItem;

        if ( bNameChange ) {
            for (hItem = FastList_FindFirst (hList); hItem; hItem = FastList_FindNext (hList, hItem))
            {
                LPCTSTR pszSubmount;
                if ((pszSubmount = FastList_GetItemText (hList, hItem, 0)) == NULL)
                    continue;

                if (!_tcsicmp(szOrigSubmount, pszSubmount) ) {
                    FastList_RemoveItem (hList, hItem);
                    break;
                }
            }
        }

        for (hItem = FastList_FindFirst (hList); hItem; hItem = FastList_FindNext (hList, hItem))
        {
            LPCTSTR pszSubmount;
            if ((pszSubmount = FastList_GetItemText (hList, hItem, 0)) == NULL)
                continue;

            if (!_tcsicmp(pszSubmount, pSubmount->szSubmount))
                break;
        }

        if (!hItem)
        {
            FASTLISTADDITEM ai;
            memset (&ai, 0x00, sizeof(FASTLISTADDITEM));
            ai.iFirstImage = IMAGE_NOIMAGE;
            ai.iSecondImage = IMAGE_NOIMAGE;
            ai.pszText = pSubmount->szSubmount;
            ai.lParam = 0;
            hItem = FastList_AddItem (hList, &ai);
        }

        FastList_SetItemText (hList, hItem, 1, szMapping);
    }
}


BOOL CALLBACK SubEdit_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLong (hDlg, DWL_USER, lp);
         SubEdit_OnInitDialog (hDlg);
         SubEdit_Enable (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               SubEdit_OnOK (hDlg);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDC_SUBMOUNT:
            case IDC_MAPPING:
               SubEdit_Enable (hDlg);
               break;

            case IDHELP:
               SubEdit_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_SUBMOUNTS_NT_ADDEDIT);
         break;
      }

   return 0;
}


void SubEdit_OnInitDialog (HWND hDlg)
{
    CHAR msg[256], msgf[256];
    PSUBMOUNT pSubmount = (PSUBMOUNT)GetWindowLong (hDlg, DWL_USER);
    if (GetDlgItemText(hDlg,IDC_STATICSUBMOUNT,(LPSTR)msg,sizeof(msg)-1)>0)
    {
		wsprintf(msgf,msg,cm_back_slash_mount_root,cm_back_slash_mount_root);
		SetDlgItemText (hDlg, IDC_STATICSUBMOUNT, msgf);
   }

   SetDlgItemText (hDlg, IDC_SUBMOUNT, pSubmount->szSubmount);
   
   SetDlgItemText (hDlg, IDC_MAPPING, pSubmount->szMapping);
}


void SubEdit_OnOK (HWND hDlg)
{
   PSUBMOUNT pSubmount = (PSUBMOUNT)GetWindowLong (hDlg, DWL_USER);
   GetDlgItemText (hDlg, IDC_SUBMOUNT, pSubmount->szSubmount, MAX_PATH);
   GetDlgItemText (hDlg, IDC_MAPPING, pSubmount->szMapping, MAX_PATH);
   EndDialog (hDlg, IDOK);
}


void SubEdit_Enable (HWND hDlg)
{
   BOOL fEnable = TRUE;

   TCHAR szText[ MAX_PATH ];
   GetDlgItemText (hDlg, IDC_SUBMOUNT, szText, MAX_PATH);
   if (!szText[0])
      fEnable = FALSE;

   GetDlgItemText (hDlg, IDC_MAPPING, szText, MAX_PATH);
   if (!szText[0])
      fEnable = FALSE;

   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
}

