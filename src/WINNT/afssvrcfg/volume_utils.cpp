/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Includes _________________________________________________________________
 *
 */
#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "afscfg.h"
#include <stdio.h>
#include "volume_utils.h"
#include "resource.h"
#include "partition_utils.h"


/*
 * Definitions _________________________________________________________________
 *
 */
static HWND m_hDriveList = 0;

//static const UINT MAX_DRIVES = 26;
static const UINT DISK_DRIVE_IMAGE = 0;
static const UINT DISABLED_DISK_DRIVE_IMAGE = 1;
static const UINT DISK_DRIVE_WITH_WARNING_IMAGE = 2;
static const UINT AFS_DISK_DRIVE_IMAGE = 3;


struct DRIVE_INFO
{
	TCHAR szRootDir[4];		// Drive letter plus colon plus slash, ex: "c:\"
	TCHAR szVolName[256];
	TCHAR szSize[32];		// Drive size in megabytes
	DWORD dwFlags;
	BOOL bDisabled;			// Disabled is TRUE if partition not suitable for AFS
	UINT nImage;			// Image to show in the FastList
};


/*
 * Prototypes ____________________________________________________________________
 *
 */


/*
 * Static Functions _________________________________________________________________
 *
 */
LPTSTR GetString(UINT nStrID)
{
	static TCHAR szText[cchRESOURCE];

	GetString(szText, nStrID);

	return szText;
}

static BOOL SetupImageLists()
{
	HIMAGELIST hiList = ImageList_Create(16, 16, TRUE, 1, 1);

	AfsAppLib_AddToImageList(hiList, IDI_DISK_DRIVE, FALSE);
	AfsAppLib_AddToImageList(hiList, IDI_DISABLED_DISK_DRIVE, FALSE);
	AfsAppLib_AddToImageList(hiList, IDI_DISK_DRIVE_WITH_WARNING, FALSE);
	AfsAppLib_AddToImageList(hiList, IDI_AGGREGATE, FALSE);

	FastList_SetImageLists(m_hDriveList, hiList, 0);

	return TRUE;
}

static void AddColumn(int nWidth, LPCTSTR pszTitle)
{
	static int nCol = 1;
	FASTLISTCOLUMN col;
	
	col.dwFlags = FLCF_JUSTIFY_LEFT;
	col.cxWidth = nWidth;
	lstrcpy(col.szText, pszTitle);
	
	FastList_SetColumn(m_hDriveList, nCol++, &col);
}

static void SetupDriveListCols()
{
	// Set width of cols based on width of the list		
	RECT rect;
	GetClientRect(m_hDriveList, &rect);
	
	int nWidth = rect.right - rect.left;

	// If the width of the list is too small to show all
	// three cols in a reasonable size that will fit without
	// scrolling, then set the width to the ideal size and
	// make the user scroll.
	if (nWidth < 150)	// Minimum we will accept
		nWidth = 370;	// The ideal size

	AddColumn(50, GetString(IDS_DRIVE));
	AddColumn(nWidth - 100, GetString(IDS_NAME_OR_ERROR));
	AddColumn(50, GetString(IDS_SIZE));
}

static LPCTSTR GetDriveSizeAsString(LPCTSTR pszRootDir)
{
	_ASSERTE(pszRootDir != 0);

	ULARGE_INTEGER liDummy;
	ULARGE_INTEGER liSize;
	static TCHAR szSize[64];

	*szSize = 0;
	
	// Get partition size in bytes	
	if (GetDiskFreeSpaceEx(pszRootDir, &liDummy, &liSize, &liDummy)) {
		// Covert to megabytes
		ULONG nSize = (ULONG)(liSize.QuadPart / (1024 * 1024));
		// Convert to a string
		_ultot(nSize, szSize, 10);
		lstrcat(szSize, TEXT(" MB"));
	}

	return szSize;
}

static BOOL DoesDriveContainData(LPCTSTR pszDriveRootDir)
{
	TCHAR szSearchSpec[16];
	WIN32_FIND_DATA findData;

	_stprintf(szSearchSpec, TEXT("%s*.*"), pszDriveRootDir);

	HANDLE hFind = FindFirstFile(szSearchSpec, &findData);
	if (hFind == INVALID_HANDLE_VALUE)
		return FALSE;

	FindClose(hFind);

	return TRUE;
}

static BOOL OnlyHasFolder(LPCTSTR pszRootDir, LPCTSTR pszFolder)
{
	TCHAR szSearchSpec[MAX_PATH];
	WIN32_FIND_DATA findData;
	BOOL bFound = FALSE;

	_stprintf(szSearchSpec, TEXT("%s*.*"), pszRootDir);

	// If there is nothing in the root dir, then return FALSE
	HANDLE hFind = FindFirstFile(szSearchSpec, &findData);
	if (hFind == INVALID_HANDLE_VALUE)
		return bFound;

	// Is the first thing on the disk the recycle bin?  If not
	// the the recycle bin is not the only thing on the disk.
	if (_tcsicmp(findData.cFileName, pszFolder) == 0) {
		// Is anything else on the disk?
		if (!FindNextFile(hFind, &findData))
			bFound = TRUE;
	}

	FindClose(hFind);

	return bFound;
}

static BOOL DriveHasRecycleBin(LPCTSTR pszDriveRootDir)
{
	if (OnlyHasFolder(pszDriveRootDir, TEXT("Recycled")))
		return TRUE;
	
	if (OnlyHasFolder(pszDriveRootDir, TEXT("Recycle Bin")))
		return TRUE;

	if (OnlyHasFolder(pszDriveRootDir, TEXT("Recycler")))
		return TRUE;

	return FALSE;
}

static BOOL DoesDriveContainNT(LPCTSTR pszDriveRootDir)
{
	UINT nBufSize = MAX_PATH;
	LPTSTR pszWinDir = 0;

	while (!pszWinDir) {
		pszWinDir = new TCHAR[nBufSize];

		UINT nWinDirLen = GetWindowsDirectory(pszWinDir, nBufSize);

		if (nWinDirLen > nBufSize) {
			nBufSize = nWinDirLen;
			delete [] pszWinDir;
			pszWinDir = 0;
		}
	}

	BOOL bNT = (_tcsncmp(pszDriveRootDir, pszWinDir, 3) == 0);

	delete [] pszWinDir;

	return bNT;
}

static BOOL ValidateDrive(BOOL bInvalid, UINT uiErrorMsgID, LPTSTR pszErrorStr)
{
	if (!bInvalid)
		return FALSE;

	if (*pszErrorStr)
		lstrcat(pszErrorStr, GetString(IDS_ERROR_SEP));
	lstrcat(pszErrorStr, GetString(uiErrorMsgID));

	return TRUE;
}

static BOOL GetDriveInfo(TCHAR chDrive, DRIVE_INFO& di)
{
	DWORD dwDummy;
	TCHAR szFileSys[64];
	DWORD dwDriveFlags;

	_stprintf(di.szRootDir, TEXT("%c:\\"), chDrive);
	
	if (GetDriveType(di.szRootDir) != DRIVE_FIXED)
		return FALSE;

	if (!GetVolumeInformation(di.szRootDir, di.szVolName, sizeof(di.szVolName), 0, &dwDummy, &dwDriveFlags, szFileSys, sizeof(szFileSys)))
		return FALSE;

	TCHAR szError[256] = TEXT("");

	BOOL bInvalid = FALSE, bHasData = FALSE;
	BOOL bIsAfs = ValidateDrive(IsAnAfsPartition(di.szRootDir), IDS_ERROR_DRIVE_ALREADY_HAS_AFS, szError);
	if (!bIsAfs) {
		bInvalid |= ValidateDrive(dwDriveFlags & FS_VOL_IS_COMPRESSED, IDS_ERROR_DRIVE_COMPRESSED, szError);
		bInvalid |= ValidateDrive(lstrcmp(szFileSys, TEXT("NTFS")) != 0, IDS_ERROR_FS_IS_NOT_NTFS, szError);

            /* 
             *  We are no longer going to require that afs drives be empty; we will give a warning instead.
             *
	     *	bInvalid |= ValidateDrive(DoesDriveContainNT(di.szRootDir), IDS_ERROR_DRIVE_CONTAINS_NT, szError);
	     *	bRecycle = ValidateDrive(DriveHasRecycleBin(di.szRootDir), IDS_WARNING_DRIVE_HAS_RECYCLE_BIN, szError);
	     */
	     	if (!bInvalid)
	     		bHasData = ValidateDrive(DoesDriveContainData(di.szRootDir), IDS_ERROR_DRIVE_HAS_DATA, szError);
	}

	if (*szError) {
		lstrcpy(di.szVolName, szError);
		
		if (bIsAfs)
			di.nImage = AFS_DISK_DRIVE_IMAGE;
		else if (bHasData)
			di.nImage = DISK_DRIVE_WITH_WARNING_IMAGE;      // Treat this as a warning
		else
			di.nImage = DISABLED_DISK_DRIVE_IMAGE;
		
		if (bIsAfs || bInvalid) {
			di.bDisabled = TRUE;
			di.dwFlags |= FLIF_DISALLOW_SELECT;
		}
	} else {
		di.nImage = DISK_DRIVE_IMAGE;
		
		if (lstrlen(di.szVolName) == 0)
			GetString(di.szVolName, IDS_VOLUME_HAS_NO_NAME);
	}

	lstrncpy(di.szSize, GetDriveSizeAsString(di.szRootDir), sizeof(di.szSize));

	return TRUE;
}

static BOOL FillDriveList()
{
	DRIVE_INFO di;

	TCHAR chDrive = TEXT('A');
	DWORD dwDrives = GetLogicalDrives();

	// This failing is not fatal to this function
	// Make sure the partition table is up to date
	afs_status_t nStatus;
	int nResult = ReadPartitionTable(&nStatus);
	ASSERT(nResult);

	while (dwDrives) {
		if (dwDrives & 1) {
			memset(&di, 0, sizeof(di));
			
			if (GetDriveInfo(chDrive, di)) {
				FASTLISTADDITEM ai = { 0, di.nImage, IMAGE_NOIMAGE, di.szRootDir, di.bDisabled, di.dwFlags };
				HLISTITEM hItem = FastList_AddItem(m_hDriveList, &ai);
				FastList_SetItemText(m_hDriveList, hItem, 1, di.szVolName);
				FastList_SetItemText(m_hDriveList, hItem, 2, di.szSize);
			}
		}

		chDrive++;
		dwDrives >>= 1;
	}

	return TRUE;
}

static int CALLBACK DriveListSortFunc(HWND hList, HLISTITEM hItem1, LPARAM lpItem1, HLISTITEM hItem2, LPARAM lpItem2)
{
	_ASSERTE(hList == m_hDriveList);

	// Ignore the first call which is only used to initialize static data
	if (!hItem1 && !hItem2)
		return 0;

	// The lpItem vars contain 1 if the item is disabled, 0 if not
	// Show enabled items before disabled items
	if (lpItem1 != lpItem2)
		return lpItem1 - lpItem2;

	LPCTSTR pszItem1 = FastList_GetItemText(m_hDriveList, hItem1, 0);
	LPCTSTR pszItem2 = FastList_GetItemText(m_hDriveList, hItem2, 0);

	return lstrcmp(pszItem1, pszItem2);
}


/*
 * Exported Functions _________________________________________________________________
 *
 */
void SetupDriveList(HWND driveList)
{
	m_hDriveList = driveList;

	SetupImageLists();

	SetupDriveListCols();

	FastList_SetSortFunction(m_hDriveList, DriveListSortFunc);
}

BOOL UpdateDriveList()
{
	FastList_RemoveAll(m_hDriveList);

	return FillDriveList();
}

