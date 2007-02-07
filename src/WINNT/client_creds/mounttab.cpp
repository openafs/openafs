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
#include <afs/fs_utils.h>
}

#include "afscreds.h"


/*
 * VARIABLES __________________________________________________________________
 *
 */

static struct l
{
    int iDriveSelectLast;
} l;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Mount_OnInitDialog (HWND hDlg);
void Mount_OnUpdate (HWND hDlg, BOOL fOnInitDialog = FALSE);
void Mount_OnSelect (HWND hDlg);
void Mount_OnCheck (HWND hDlg);
void Mount_OnRemove (HWND hDlg);
void Mount_OnAdd (HWND hDlg);
void Mount_OnEdit (HWND hDlg);

void Mount_AdjustMapping (HWND hDlg, int iDrive);
int Mount_DriveFromItem (HWND hDlg, int iItem);

BOOL CALLBACK Mapping_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Mapping_OnInitDialog (HWND hDlg);
void Mapping_OnOK (HWND hDlg);
void Mapping_OnEnable (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Mount_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         RECT rTab;
         GetClientRect (GetParent(hDlg), &rTab);
         TabCtrl_AdjustRect (GetParent (hDlg), FALSE, &rTab); 
         SetWindowPos (hDlg, NULL, rTab.left, rTab.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);

         Mount_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_ADD:
               Mount_OnAdd (hDlg);
               break;

            case IDC_EDIT:
               Mount_OnEdit (hDlg);
               break;

            case IDC_REMOVE:
               Mount_OnRemove (hDlg);
               break;

            case IDC_LIST:
               if (HIWORD(wp) == LBN_CLICKED)
                  Mount_OnCheck (hDlg);
               else if ((HIWORD(wp) == LBN_SELCHANGE) || (HIWORD(wp) == LBN_SELCANCEL))
                  Mount_OnSelect (hDlg);
               break;

            case IDHELP:
               Mount_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCREDS_TAB_DRIVES);
         break;
      }
   return FALSE;
}


void Mount_OnInitDialog (HWND hDlg)
{
   int xTabStop = 250;
   SendDlgItemMessage (hDlg, IDC_LIST, LB_SETTABSTOPS, 1, (LPARAM)&xTabStop);

   Mount_OnUpdate (hDlg, TRUE);
}


void Mount_OnUpdate (HWND hDlg, BOOL fOnInitDialog)
{
    DRIVEMAPLIST List;
    memset(&List, 0, sizeof(DRIVEMAPLIST));
    QueryDriveMapList (&List);

    HWND hList = GetDlgItem (hDlg, IDC_LIST);
    int iItemSel = LB_GetSelected(hList);
    int iDataSel = Mount_DriveFromItem (hDlg, iItemSel);
    iItemSel = -1;

    if (fOnInitDialog && (iDataSel == -1))
	iDataSel = l.iDriveSelectLast;

    LB_StartChange(hList, TRUE);

    for (int iDrive = 25; iDrive >= 0; --iDrive)
    {
	if (!List.aDriveMap[ iDrive ].szMapping[0])
	    continue;

	TCHAR szAfsPath[ MAX_PATH ];
	AdjustAfsPath (szAfsPath, List.aDriveMap[ iDrive ].szMapping, TRUE, FALSE);

	LPTSTR psz = FormatString (IDS_DRIVE_MAP, TEXT("%c%s"), List.aDriveMap[ iDrive ].chDrive, szAfsPath);
	int iItem = LB_AddItem(hList, psz, List.aDriveMap[ iDrive ].fActive);
	FreeString (psz);

	int iCount = SendMessage(hList, LB_GETCOUNT, 0, 0);

	/* This really shouldn't work except that we are adding 
	 * the strings in alphabetical order.  Otherwise, we could
	 * identify an index value for a string that could then change.
	 */
	if (iDrive == iDataSel)
	    iItemSel = iItem;
    }

    LB_EndChange(hList, NULL);

    LB_SetSelected(hList, iItemSel);

    Mount_OnSelect (hDlg);
    FreeDriveMapList (&List);
}


void Mount_OnSelect (HWND hDlg)
{
    BOOL fServiceRunning = IsServiceRunning();

   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   int iItemSel = LB_GetSelected(hList);
   int iDataSel = Mount_DriveFromItem (hDlg, iItemSel);

   l.iDriveSelectLast = iDataSel;

   EnableWindow (GetDlgItem (hDlg, IDC_ADD), fServiceRunning);
   EnableWindow (GetDlgItem (hDlg, IDC_EDIT), fServiceRunning && (iDataSel != -1));
   EnableWindow (GetDlgItem (hDlg, IDC_REMOVE), fServiceRunning && (iDataSel != -1));
}


void Mount_OnCheck (HWND hDlg)
{
   DRIVEMAPLIST List;
   QueryDriveMapList (&List);

   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   int iItemSel = LB_GetSelected(hList);
   int iDriveSel = Mount_DriveFromItem (hDlg, iItemSel);
   BOOL fChecked = SendMessage (hList, LB_GETITEMDATA, iItemSel, 0);

   if (iDriveSel != -1)
      {
      DWORD dwStatus;
      if (fChecked && List.aDriveMap[ iDriveSel ].szMapping[0] && !List.aDriveMap[ iDriveSel ].fActive)
         {
         if (!ActivateDriveMap (List.aDriveMap[ iDriveSel ].chDrive, List.aDriveMap[ iDriveSel ].szMapping, List.aDriveMap[ iDriveSel ].szSubmount, List.aDriveMap[ iDriveSel ].fPersistent, &dwStatus))
            Message (MB_OK | MB_ICONHAND, IDS_ERROR_MAP, IDS_ERROR_MAP_DESC, TEXT("%08lX"), dwStatus);
         Mount_OnUpdate (hDlg);
         }
      else if (!fChecked && List.aDriveMap[ iDriveSel ].fActive)
         {
         if (!InactivateDriveMap (List.aDriveMap[ iDriveSel ].chDrive, &dwStatus))
            Message (MB_OK | MB_ICONHAND, IDS_ERROR_UNMAP, IDS_ERROR_UNMAP_DESC, TEXT("%08lX"), dwStatus);
         Mount_OnUpdate (hDlg);
         }
      WriteActiveMap(List.aDriveMap[ iDriveSel ].chDrive, fChecked && List.aDriveMap[ iDriveSel ].fPersistent );
      }

   FreeDriveMapList (&List);
}


void Mount_OnRemove (HWND hDlg)
{
    HWND hList = GetDlgItem (hDlg, IDC_LIST);
    int iItemSel = LB_GetSelected(hList);
   int iDriveSel = Mount_DriveFromItem (hDlg, iItemSel);

   if (iDriveSel != -1)
      {
      DRIVEMAPLIST List;
      QueryDriveMapList (&List);

      if (List.aDriveMap[ iDriveSel ].szMapping[0])
         {
         if (List.aDriveMap[ iDriveSel ].fActive)
            {
            DWORD dwStatus;
            if (!InactivateDriveMap (List.aDriveMap[ iDriveSel ].chDrive, &dwStatus))
               {
               Message (MB_OK | MB_ICONHAND, IDS_ERROR_UNMAP, IDS_ERROR_UNMAP_DESC, TEXT("%08lX"), dwStatus);
               return;
               }
            }
         List.aDriveMap[ iDriveSel ].szMapping[0] = TEXT('\0');
         WriteDriveMappings (&List);

         Mount_OnUpdate (hDlg);
         }
	  WriteActiveMap(List.aDriveMap[ iDriveSel ].chDrive, FALSE );
      FreeDriveMapList (&List);
      }
}


void Mount_OnAdd (HWND hDlg)
{
   Mount_AdjustMapping (hDlg, -1);
}


void Mount_OnEdit (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   int iItemSel = LB_GetSelected(hList);
   int iDriveSel = Mount_DriveFromItem (hDlg, iItemSel);

   Mount_AdjustMapping (hDlg, iDriveSel);
}


void Mount_AdjustMapping (HWND hDlg, int iDrive)
{
   DRIVEMAPLIST List;
   QueryDriveMapList (&List);

   DRIVEMAP DriveMapOrig;
   memset (&DriveMapOrig, 0x00, sizeof(DRIVEMAP));

   if (iDrive != -1)
      {
      memcpy (&DriveMapOrig, &List.aDriveMap[ iDrive ], sizeof(DRIVEMAP));
      }

   DRIVEMAP DriveMap;
   memcpy (&DriveMap, &DriveMapOrig, sizeof(DRIVEMAP));

   if (ModalDialogParam (IDD_MAPPING, hDlg, (DLGPROC)Mapping_DlgProc, (LPARAM)&DriveMap) == IDOK)
      {
      TCHAR szAfsPathOrig[ MAX_PATH ];
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
               Mount_OnUpdate (hDlg);
               return;
               }
            }

         if (!ActivateDriveMap (DriveMap.chDrive, szAfsPathNew, DriveMap.szSubmount, DriveMap.fPersistent, &dwStatus))
            {
            Message (MB_OK | MB_ICONHAND, IDS_ERROR_MAP, IDS_ERROR_MAP_DESC, TEXT("%08lX"), dwStatus);
            Mount_OnUpdate (hDlg);
            return;
            }

         if (DriveMap.szSubmount[0])
            {
            TCHAR szSubmountNow[ MAX_PATH ];
            if (GetDriveSubmount (DriveMap.chDrive, szSubmountNow))
               {
               if (lstrcmpi (DriveMap.szSubmount, szSubmountNow))
                  {
                  int idsTitle = (g.fIsWinNT) ? IDS_NEWSUB_TITLE : IDS_NEWSUB_TITLE_95;
                  Message (MB_OK | MB_ICONASTERISK, idsTitle, IDS_NEWSUB_DESC);
                  }
               }
            }

         if (iDrive != -1)
            memset (&List.aDriveMap[ iDrive ], 0x00, sizeof(DRIVEMAP));
         memcpy (&List.aDriveMap[ DriveMap.chDrive-chDRIVE_A ], &DriveMap, sizeof(DRIVEMAP));
         lstrcpy (List.aDriveMap[ DriveMap.chDrive-chDRIVE_A ].szMapping, szAfsPathNew);
         WriteDriveMappings (&List);

         Mount_OnUpdate (hDlg);
         }
      }
}


int Mount_DriveFromItem (HWND hDlg, int iItem)
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


BOOL CALLBACK Mapping_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLongPtr (hDlg, DWLP_USER, lp);
         Mapping_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               Mapping_OnOK (hDlg);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDC_MAP_PATH:
               Mapping_OnEnable (hDlg);
               break;

            case IDHELP:
               Mapping_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCREDS_MAPDRIVE);
         break;
      }
   return FALSE;
}


void Mapping_OnInitDialog (HWND hDlg)
{
   PDRIVEMAP pMap = (PDRIVEMAP)GetWindowLongPtr (hDlg, DWLP_USER);

   // Fill in the combo box
   //
   DWORD dwDrives = GetLogicalDrives() | 0x07; // Always pretend A,B,C: are used

   if (pMap->chDrive != 0)
      dwDrives &= ~( 1 << (pMap->chDrive - chDRIVE_A) );

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
   SetDlgItemText (hDlg, IDC_MAP_PATH, szMapping);
   SetDlgItemText (hDlg, IDC_MAP_DESC, pMap->szSubmount);

   CheckDlgButton (hDlg, IDC_MAP_PERSISTENT, (pMap->chDrive == 0) ? TRUE : (pMap->fPersistent));

   Mapping_OnEnable (hDlg);
}


void Mapping_OnOK (HWND hDlg)
{
   PDRIVEMAP pMap = (PDRIVEMAP)GetWindowLongPtr (hDlg, DWLP_USER);

   int iItem = SendDlgItemMessage (hDlg, IDC_MAP_LETTER, CB_GETCURSEL, 0, 0);
   int iDrive = SendDlgItemMessage (hDlg, IDC_MAP_LETTER, CB_GETITEMDATA, iItem, 0);

   pMap->chDrive = chDRIVE_A + iDrive;
   GetDlgItemText (hDlg, IDC_MAP_PATH, pMap->szMapping, MAX_PATH);
   GetDlgItemText (hDlg, IDC_MAP_DESC, pMap->szSubmount, MAX_PATH);
   pMap->fPersistent = IsDlgButtonChecked (hDlg, IDC_MAP_PERSISTENT);

   if (pMap->szSubmount[0] && !IsValidSubmountName (pMap->szSubmount))
      {
      int idsTitle = (g.fIsWinNT) ? IDS_BADSUB_TITLE : IDS_BADSUB_TITLE_95;
      Message (MB_ICONHAND, idsTitle, IDS_BADSUB_DESC);
      return;
      }

   if ( (lstrncmpi (pMap->szMapping, cm_slash_mount_root, lstrlen(cm_slash_mount_root))) &&	/*TEXT("/afs")*/
        (lstrncmpi (pMap->szMapping, cm_back_slash_mount_root, lstrlen(cm_back_slash_mount_root))) ) /*TEXT("\\afs")*/
   {
     Message (MB_ICONHAND, IDS_BADMAP_TITLE, IDS_BADMAP_DESC);
     return;
   }

   WriteActiveMap(pMap->chDrive, pMap->fPersistent);
   EndDialog (hDlg, IDOK);
}


void Mapping_OnEnable (HWND hDlg)
{
   TCHAR szPath[ MAX_PATH ];
   GetDlgItemText (hDlg, IDC_MAP_PATH, szPath, MAX_PATH);
   EnableWindow (GetDlgItem (hDlg, IDOK), (szPath[0] != TEXT('\0')));
}

