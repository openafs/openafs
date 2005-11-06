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
#include <WINNT\afsreg.h>
#include <stdio.h>
#include <lanahelper.h>

/*
 * DEFINITIONS ________________________________________________________________
 *
 */
static DRIVEMAPLIST GlobalDrives;
static TCHAR szHostName[128];

// DefineDosDrive actions
enum DDDACTION  { DDD_ADD, DDD_REMOVE };

#define DRIVE_LETTER_INDEX 2


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
void AutoMap_OnInitDialog (HWND hDlg);
void AutoMap_OnAdd (HWND hDlg);
void AutoMap_OnSelect (HWND hDlg);
void AutoMap_OnEdit (HWND hDlg);
void AutoMap_OnRemove (HWND hDlg);

void ShowDriveList(HWND hDlg, DRIVEMAPLIST& drives);
void AddToDriveList(DRIVEMAPLIST& DriveMapList, DRIVEMAP& DriveMap);
void RemoveFromDriveList(DRIVEMAPLIST& DriveMapList, DRIVEMAP& DriveMap);
DRIVEMAP *GetSelectedDrive(HWND hDlg, HLISTITEM  *pItem = 0);
BOOL DefineDosDrive(DRIVEMAP *pDrive, DDDACTION dddAction = DDD_ADD);

BOOL CALLBACK AutoMapEdit_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void AutoMapEdit_OnInitDialog (HWND hDlg);
void AutoMapEdit_OnOK (HWND hDlg);
void AutoMapEdit_Enable (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK AutoMap_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         AutoMap_OnInitDialog (hDlg);
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
               AutoMap_DlgProc (hDlg, WM_HELP, 0, 0);
               break;

            case IDOK:
               EndDialog(hDlg, IDOK);
               break;
                
            case IDCANCEL:
               EndDialog(hDlg, IDCANCEL);
               break;

				case IDC_ADD:
					AutoMap_OnAdd(hDlg);
					break;

				case IDC_CHANGE:
					AutoMap_OnEdit(hDlg);
					break;

				case IDC_REMOVE:
					AutoMap_OnRemove(hDlg);
					break;
            }
         break;

      case WM_NOTIFY:
         switch (((LPNMHDR)lp)->code)
         	{
            case FLN_ITEMSELECT:
               AutoMap_OnSelect (hDlg);
               break;

            case FLN_LDBLCLICK:
               if (IsWindowEnabled (GetDlgItem (hDlg, IDC_EDIT)))
                  AutoMap_OnEdit (hDlg);
               break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_ADVANCED_AUTOMAP);
         break;
      }

   return FALSE;
}


void AddToDriveList(DRIVEMAPLIST& DriveMapList, DRIVEMAP& DriveMap)
{
   int nCurDrive = DriveMap.chDrive - TEXT('A');
   
   memcpy(&DriveMapList.aDriveMap[nCurDrive], &DriveMap, sizeof(DRIVEMAP));
}


void RemoveFromDriveList(DRIVEMAPLIST& DriveMapList, DRIVEMAP& DriveMap)
{
   int nCurDrive = DriveMap.chDrive - TEXT('A');
   
   memset(&DriveMapList.aDriveMap[nCurDrive], 0, sizeof(DRIVEMAP));
}


void AutoMap_OnInitDialog (HWND hDlg)
{
   // Prepare the columns
   HWND hList = GetDlgItem (hDlg, IDC_GLOBAL_DRIVE_LIST);

   FASTLISTCOLUMN Column;
   Column.dwFlags = FLCF_JUSTIFY_LEFT;
   Column.cxWidth = 65;
   GetString (Column.szText, IDS_DRIVE);
   FastList_SetColumn (hList, 0, &Column);

   Column.dwFlags = FLCF_JUSTIFY_LEFT;
   Column.cxWidth = 266;
   GetString (Column.szText, IDS_SUBCOL_PATH);
   FastList_SetColumn (hList, 1, &Column);

	gethostname(szHostName, sizeof(szHostName));

   Config_GetGlobalDriveList(&GlobalDrives);

   ShowDriveList(hDlg, GlobalDrives);
}


void ShowDriveList(HWND hDlg, DRIVEMAPLIST& drives)
{
   // Prepare the columns
   HWND hList = GetDlgItem (hDlg, IDC_GLOBAL_DRIVE_LIST);

   // Fill in the list of drives
   FastList_Begin (hList);

   FastList_RemoveAll(hList);

   for (size_t ii = 0; ii < 26; ++ii) {
      if (!GlobalDrives.aDriveMap[ ii ].chDrive)
         continue;

      HWND hList = GetDlgItem (hDlg, IDC_GLOBAL_DRIVE_LIST);
   
   	FASTLISTADDITEM ai;
   	memset (&ai, 0x00, sizeof(FASTLISTADDITEM));
   	ai.iFirstImage = IMAGE_NOIMAGE;
   	ai.iSecondImage = IMAGE_NOIMAGE;
   	// There must be DRIVE_LETTER_INDEX number of spaces before the ? character
   	ai.pszText = _tcsdup(TEXT("  ?:"));
   	ai.pszText[DRIVE_LETTER_INDEX] = GlobalDrives.aDriveMap[ ii ].chDrive;
   	ai.lParam = 0;
   	
   	HLISTITEM hItem = FastList_AddItem (hList, &ai);
   
      TCHAR szAfsPath[ MAX_PATH ];
      AdjustAfsPath (szAfsPath, GlobalDrives.aDriveMap[ ii ].szMapping, TRUE, FALSE);
   
   	FastList_SetItemText (hList, hItem, 1, szAfsPath);
   }

   FastList_End (hList);
}


BOOL UpdateRegistry(DRIVEMAP *pDrive, BOOL bRemove)
{
   TCHAR szKeyName[128];
   TCHAR szValueName[128];
   HKEY hKey;
   LONG result;
   DWORD dwDispo;

   if (!pDrive)
      return FALSE;

   _stprintf(szKeyName, TEXT("%s\\GlobalAutoMapper"), TEXT(AFSREG_CLT_SVC_PARAM_SUBKEY));

   if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szKeyName, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, &dwDispo) != ERROR_SUCCESS)
      return FALSE;

   _stprintf(szValueName, TEXT("%c:"), pDrive->chDrive);
 
   if (bRemove) 
      result = RegDeleteValue(hKey, szValueName);
   else
      result = RegSetValueEx(hKey, szValueName, 0, REG_SZ, (BYTE *)pDrive->szSubmount, lstrlen(pDrive->szSubmount) + 1);

   RegCloseKey(hKey);
   
   return (result == ERROR_SUCCESS);
}


BOOL DefineDosDrive(DRIVEMAP *pDrive, DDDACTION dddAction)
{
    // TCHAR szAfsPath[MAX_PATH];
    // TCHAR szDrive[3] = TEXT("?:");
   BOOL fResult = FALSE;

   if (!pDrive)
      return FALSE;

   if (dddAction == DDD_REMOVE) {
       if (!(fResult=(DisMountDOSDrive(pDrive->chDrive)==NO_ERROR)))
           Message (MB_OK | MB_ICONHAND, IDS_ERROR_UNMAP, IDS_ERROR_UNMAP_DESC, TEXT("%08lX"), GetLastError());
   } else if (dddAction == DDD_ADD) {
       if (!(fResult=(MountDOSDrive(pDrive->chDrive, pDrive->szSubmount,FALSE)==NO_ERROR)))
	   Message (MB_OK | MB_ICONHAND, IDS_ERROR_MAP, IDS_ERROR_MAP_DESC, TEXT("%08lX"), GetLastError());
   }
   /*
     Replace this code with Drive mapping routine that doesn't require different formats for each OS
     szDrive[0] = pDrive->chDrive;
     _stprintf(szAfsPath, TEXT("\\Device\\LanmanRedirector\\%s\\%s-AFS\\%s"), szDrive, szHostName, pDrive->szSubmount);

     if (dddAction == DDD_REMOVE) {
         fResult = DefineDosDevice(DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE, szDrive, szAfsPath);
	 if (!fResult)
             Message (MB_OK | MB_ICONHAND, IDS_ERROR_UNMAP, IDS_ERROR_UNMAP_DESC, TEXT("%08lX"), GetLastError());
     } else if (dddAction == DDD_ADD) {
          fResult = DefineDosDevice(DDD_RAW_TARGET_PATH, szDrive, szAfsPath);
	  if (!fResult)
	      Message (MB_OK | MB_ICONHAND, IDS_ERROR_MAP, IDS_ERROR_MAP_DESC, TEXT("%08lX"), GetLastError());
     }

    */
   if (fResult)
       UpdateRegistry(pDrive, dddAction == DDD_REMOVE);
   
   return fResult;
}   


void AutoMap_OnAdd(HWND hDlg)
{
	DRIVEMAP DriveMap;
   memset(&DriveMap, 0, sizeof(DRIVEMAP));

	if (ModalDialogParam (IDD_GLOBAL_DRIVES_ADDEDIT, hDlg, (DLGPROC)AutoMapEdit_DlgProc, (LPARAM)&DriveMap) != IDOK)
		return;

	if (DriveMap.chDrive) {
      if (DefineDosDrive(&DriveMap)) {
		   AddToDriveList(GlobalDrives, DriveMap);
		   ShowDriveList(hDlg, GlobalDrives);
		}
	}

	AutoMap_OnSelect(hDlg);
}


void AutoMap_OnSelect (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_GLOBAL_DRIVE_LIST);

   BOOL bEnable = FastList_FindFirstSelected (hList) != NULL;
	
	EnableWindow (GetDlgItem (hDlg, IDC_REMOVE), bEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_CHANGE), bEnable);
}


DRIVEMAP *GetSelectedDrive(HWND hDlg, HLISTITEM  *pItem)
{
	static DRIVEMAP DriveMap;
   HLISTITEM hItem;
   
   HWND hList = GetDlgItem (hDlg, IDC_GLOBAL_DRIVE_LIST);

   if (!pItem)
      pItem = &hItem;

   if ((*pItem = FastList_FindFirstSelected (hList)) == NULL)
   	return 0;
   	
	LPCTSTR pszDrive = FastList_GetItemText (hList, *pItem, 0);
   int nCurDrive = pszDrive[DRIVE_LETTER_INDEX] - TEXT('A');

   memcpy(&DriveMap, &GlobalDrives.aDriveMap[nCurDrive], sizeof(DRIVEMAP));

   return &DriveMap;
}

   
void AutoMap_OnEdit (HWND hDlg)
{
   DRIVEMAP *pOldDrive = GetSelectedDrive(hDlg);
   if (!pOldDrive)
      return;

   DRIVEMAP NewDrive;
   memcpy(&NewDrive, pOldDrive, sizeof(DRIVEMAP));
	
	if (ModalDialogParam (IDD_GLOBAL_DRIVES_ADDEDIT, hDlg, (DLGPROC)AutoMapEdit_DlgProc, (LPARAM)&NewDrive) != IDOK)
		return;

   if (DefineDosDrive(pOldDrive, DDD_REMOVE)) {
      RemoveFromDriveList(GlobalDrives, *pOldDrive);

      if (DefineDosDrive(&NewDrive))
   	   AddToDriveList(GlobalDrives, NewDrive);

   	ShowDriveList(hDlg, GlobalDrives);
	}

	AutoMap_OnSelect(hDlg);
}


void AutoMap_OnRemove (HWND hDlg)
{
   HLISTITEM hItem;

   HWND hList = GetDlgItem (hDlg, IDC_GLOBAL_DRIVE_LIST);

   DRIVEMAP *pDrive = GetSelectedDrive(hDlg, &hItem);
   if (pDrive == 0)
      return;
      
   if (DefineDosDrive(pDrive, DDD_REMOVE)) {
      RemoveFromDriveList(GlobalDrives, *pDrive);
   	FastList_RemoveItem (hList, hItem);
   }

	AutoMap_OnSelect(hDlg);
}


BOOL CALLBACK AutoMapEdit_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLongPtr (hDlg, DWLP_USER, lp);
         AutoMapEdit_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               AutoMapEdit_OnOK (hDlg);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDC_PATH:
               AutoMapEdit_Enable (hDlg);
               break;

            case IDHELP:
               AutoMapEdit_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_ADVANCED_AUTOMAP_ADDEDIT);
         break;
      }
   return FALSE;
}


void AutoMapEdit_OnInitDialog (HWND hDlg)
{
   PDRIVEMAP pMap = (PDRIVEMAP)GetWindowLongPtr (hDlg, DWLP_USER);

   DWORD dwDrives = GetLogicalDrives() | 0x07; // Always pretend A,B,C: are used

   // Fill in the combo box
   //

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
   SetDlgItemText (hDlg, IDC_PATH, szMapping);
   SetDlgItemText (hDlg, IDC_DESC, pMap->szSubmount);

   CheckDlgButton (hDlg, IDC_PERSISTENT, (pMap->chDrive == 0) ? TRUE : (pMap->fPersistent));

   AutoMapEdit_Enable (hDlg);
}


void AutoMapEdit_OnOK (HWND hDlg)
{
   PDRIVEMAP pMap = (PDRIVEMAP)GetWindowLongPtr (hDlg, DWLP_USER);

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

   // First get a proper submount
   if (pMap->szSubmount[0]) {
      TCHAR szNewSubmount[MAX_PATH];
      PathToSubmount (szNewSubmount, pMap->szMapping, pMap->szSubmount, 0);
      if (lstrcmp(szNewSubmount, pMap->szSubmount) != 0) {
         Message (MB_OK | MB_ICONASTERISK, GetCautionTitle(), IDS_NEWSUB_DESC);
         return;
      }
   } else { // If no submount was specified, then get a new one
      if (!PathToSubmount (pMap->szSubmount, pMap->szMapping, 0, 0)) {
         return;
      }
   }

   EndDialog (hDlg, IDOK);
}


void AutoMapEdit_Enable (HWND hDlg)
{
   TCHAR szPath[ MAX_PATH ];
   GetDlgItemText (hDlg, IDC_PATH, szPath, MAX_PATH);
   EnableWindow (GetDlgItem (hDlg, IDOK), (szPath[0] != TEXT('\0')));
}

