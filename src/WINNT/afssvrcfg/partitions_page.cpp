/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * INCLUDES _________________________________________________________________
 *
 */
extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "afscfg.h"
#include "resource.h"
#include "create_partition_dlg.h"
#include "partition_utils.h"
#include "salvage_results_dlg.h"
extern "C" {
#include <afs\afs_vosAdmin.h>
#include <afs\afs_clientAdmin.h>
}


// TODO:  Add context menu to fastlist


/*
 * DEFINITIONS _________________________________________________________________
 *
 */
#define MAX_PARTITIONS	26

static HWND hDlg = 0;
static HWND hPartitionList = 0;
static HLISTITEM hSelectedItem;

static TCHAR szYes[cchRESOURCE];
static TCHAR szNo[cchRESOURCE];

static const UINT DISK_DRIVE_IMAGE = 0;
//static const UINT DISABLED_DISK_DRIVE_IMAGE = 1;
//static const UINT DISK_DRIVE_WITH_WARNING_IMAGE = 2;

// Remember the config state of the FS so we can detect when it changes.
// When it changes we must redisplay the partition info.
static CONFIG_STATE configFS;



/*
 * PROTOTYPES _________________________________________________________________
 *
 */
BOOL ShowSalvageDlg(HWND hParent, LPCTSTR pszPartitionName);

static void OnCreatePartitions();
static void OnInitDialog(HWND hwndDlg);
static void SetupListCols();
static void AddColumn(int nWidth, LPCTSTR pszTitle, DWORD dwFlags = FLCF_JUSTIFY_LEFT);
static void ShowPartitions();
cfg_partitionEntry_t *GetPartitionTableFromRegistry(int& cEntries);
static vos_partitionEntry_t *GetPartitionTableFromVos(int &nNumPartitions);
static void OnListSelection(LPFLN_ITEMSELECT_PARAMS pItemParms);
static void SetupImageLists();
static void OnRemove();
static BOOL CheckShowPartitions();
static void OnSalvage();
static void CheckEnableSalvage();


/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */
/*
 * Dialog Proc _________________________________________________________________
 *
 */
BOOL CALLBACK PartitionsPageDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (AfsAppLib_HandleHelp(IDD_PARTITIONS_PAGE, hwndDlg, uMsg, wParam, lParam))
		return TRUE;

	switch (uMsg) {
		case WM_INITDIALOG:	OnInitDialog(hwndDlg);
							break;
	
		case WM_COMMAND:	switch (LOWORD(wParam)) {
								case IDINIT:		CheckShowPartitions();
													break;

								case IDC_CREATE_PARTITIONS:	
													OnCreatePartitions();
													break;
							
								case IDC_REMOVE:	OnRemove();
													break;

								case IDC_SALVAGE:	OnSalvage();
													break;
							}
							break;

		case WM_NOTIFY:		if ((((LPNMHDR)lParam)->code) == FLN_ITEMSELECT)
								OnListSelection((LPFLN_ITEMSELECT_PARAMS)lParam);
							break;
	}

	return FALSE;
}

void UpdatePartitionList()
{
	ShowPartitions();
	CheckEnableSalvage();
}


/*
 * STATIC FUNCTIONS _________________________________________________________________
 *
 */

/*
 * Event Handler Functions _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg)
{
	hDlg = hwndDlg;

//	PropSheet_CancelToClose(GetParent(hDlg));

	hSelectedItem = 0;

	hPartitionList = GetDlgItem(hDlg, IDC_PARTITION_LIST);
	_ASSERTE(hPartitionList);

	SetupImageLists();

	GetString(szYes, IDS_YES);
	GetString(szNo, IDS_NO);

	SetupListCols();

	configFS = g_CfgData.configFS;

	ShowPartitions();

	CheckEnableSalvage();
}

static void OnCreatePartitions()
{
	if (CreatePartition(hDlg))
		ShowPartitions();
}

static void OnListSelection(LPFLN_ITEMSELECT_PARAMS pItemParms)
{
	ASSERT(pItemParms);

	hSelectedItem = pItemParms->hItem;

	ENABLE_STATE es = pItemParms->hItem ? ES_ENABLE : ES_DISABLE;

	SetEnable(hDlg, IDC_REMOVE, es);
	SetEnable(hDlg, IDC_REMOVE_MSG, es);
}

static void OnRemove()
{
	ASSERT(hSelectedItem);
	ASSERT(g_hServer);
	
	afs_status_t nStatus;

	BOOL bExported = (BOOL)FastList_GetItemParam(hPartitionList, hSelectedItem);
	if (bExported) {
		MsgBox(hDlg, IDS_CANT_DELETE_EXPORTED_PARTITION, GetAppTitleID(), MB_OK | MB_ICONSTOP);
		return;
	}

	LPCTSTR pszPartitionName = FastList_GetItemText(hPartitionList, hSelectedItem, 0);
	ASSERT(pszPartitionName);

	int nResult = Message(MB_ICONQUESTION | MB_YESNO, GetAppTitleID(), IDS_DELETE_PARTITION_PROMPT, TEXT("%s"), pszPartitionName);
	if (nResult == IDNO)
		return;

	g_LogFile.Write("Removing partition '%s'.\r\n", (char *)S2A(pszPartitionName));
	nResult = cfg_HostPartitionTableRemoveEntry(g_hServer, S2A(pszPartitionName), &nStatus);
	if (!nResult) {
		ShowError(hDlg, nStatus, IDS_REMOVE_PARTITION_ERROR);
		return;
	}

	FastList_RemoveItem(hPartitionList, hSelectedItem);

	CheckEnableSalvage();
}

static void OnSalvage()
{
    // Can't salvage if the file server is not configured
    if (!Configured(g_CfgData.configFS)) {
        ShowError(hDlg, 0, IDS_CANT_SALVAGE_WHEN_FS_NOT_CONFIGURED);
        return;
    }

	LPCTSTR pszPartitionName = TEXT("");

	if (hSelectedItem) {
		pszPartitionName = FastList_GetItemText(hPartitionList, hSelectedItem, 0);
		ASSERT(pszPartitionName);
	}	

	if (!ShowSalvageDlg(hDlg, pszPartitionName))
        return;

    ShowSalvageResults(hDlg);

    // Since a salvage was performed, there may be partitions exported that were
    // not exported before the salvage.  Salvage may stop and restart the file
    // server, which will pick up the previously unexported partitions.  We will
    // update our partition list in case this has happened.
    UpdatePartitionList();
}


/*
 * Utility Functions _________________________________________________________________
 *
 */
static BOOL CheckShowPartitions()
{
	if (configFS != g_CfgData.configFS) {
		configFS = g_CfgData.configFS;
        UpdatePartitionList();
		return TRUE;
	}

	return FALSE;
}

static void SetupImageLists()
{
	HIMAGELIST hiList = ImageList_Create(16, 16, TRUE, 1, 1);

	AfsAppLib_AddToImageList(hiList, IDI_AGGREGATE, FALSE);
	AfsAppLib_AddToImageList(hiList, IDI_DISABLED_DISK_DRIVE, FALSE);

	FastList_SetImageLists(hPartitionList, hiList, 0);
}

static void AddColumn(int nWidth, LPCTSTR pszTitle, DWORD dwFlags)
{
	static int nCol = 1;
	FASTLISTCOLUMN col;
	
	col.dwFlags = dwFlags;
	col.cxWidth = nWidth;
	lstrcpy(col.szText, pszTitle);
	
	FastList_SetColumn(hPartitionList, nCol++, &col);
}

static void SetupListCols()
{
	TCHAR szMsg[cchRESOURCE];

	AddColumn(75, GetResString(IDS_NAME, szMsg));
	AddColumn(55, GetResString(IDS_DRIVE, szMsg));
	AddColumn(60, GetResString(IDS_EXPORTED, szMsg));
	AddColumn(75, GetResString(IDS_TOTAL, szMsg), FLCF_JUSTIFY_RIGHT);
	AddColumn(75, GetResString(IDS_FREE, szMsg), FLCF_JUSTIFY_RIGHT);
}

cfg_partitionEntry_t *GetPartitionTableFromRegistry(int& cEntries)
{
	afs_status_t nStatus;
 
	// Read the parition table out of the registry
	int nResult = ReadPartitionTable(&nStatus);
	if (!nResult) {
		ShowError(hDlg, nStatus, IDS_READ_PARTITIONS_ERROR);
		return 0;
	}

	return GetPartitionTable(cEntries);
}

static vos_partitionEntry_t *GetPartitionTableFromVos(int &nNumPartitions)
{
	ASSERT(g_hCell);
	ASSERT(g_CfgData.szHostname[0]);

	nNumPartitions = 0;

	if (g_CfgData.configFS != CS_ALREADY_CONFIGURED)
		return 0;

	static vos_partitionEntry_t aPartitions[MAX_PARTITIONS];
	afs_status_t nStatus, nIgnore;
	void *hServer = 0;
	int nNumParts = 0;

	// Open this server
	g_LogFile.Write("Opening server %s.\r\n", GetHostnameA());
	int nResult = vos_ServerOpen(g_hCell, GetHostnameA(), &hServer, &nStatus);
	if (nResult) {
		
		// Read the partition info
		g_LogFile.Write("Reading paritition information for this server.\r\n");
		void *iterID;

		nResult = vos_PartitionGetBegin(g_hCell, hServer, 0, &iterID, &nStatus);
		if (nResult) {
			while (nNumParts < MAX_PARTITIONS) {
				nResult = vos_PartitionGetNext(iterID, &aPartitions[nNumParts], &nStatus);
				if (!nResult) {
					if (nStatus == ADMITERATORDONE) {
						nResult = 1;
						nStatus = 0;
					}
					break;
				}

				nNumParts++;
			}
			vos_PartitionGetDone(iterID, &nIgnore);
		}
		vos_ServerClose(hServer, &nIgnore);
	}
	
	if (!nResult) {
		ShowError(hDlg, nStatus, IDS_GET_PARTITION_LIST_ERROR);
		return 0;
	}

	nNumPartitions = nNumParts;

	return aPartitions;
}

// Convert a disk space value in Kbytes into a string
static LPTSTR DiskSpaceToString(int nSpace)
{
	const float oneMB = 1024;	// in K bytes
	const double oneGB = oneMB * 1024;
	const double oneTB = oneGB * 1024;

	static TCHAR szSpace[64];
	double space;
	LPTSTR pszUnits;

	space = nSpace;	

	if (space >= oneTB) {
		space /= oneTB;
		pszUnits = TEXT(" TB");
	} else if (space >= oneGB) {
		space /= oneGB;
		pszUnits = TEXT(" GB");
    } else if (space >= oneMB) {
		space /= oneMB;
		pszUnits = TEXT(" MB");
	} else
		pszUnits = TEXT(" KB");

	int nNumDecimals = 0;
	if (space - double(int(space)) > 0)
		nNumDecimals = 2;

	_stprintf(szSpace, TEXT("%3.*f%s"), nNumDecimals, space, pszUnits);

	return szSpace;
}

static void ShowPartitions()
{
	FastList_RemoveAll(hPartitionList);

	int cRegParts = 0, cVosParts = 0;

    // If we got nothing from the registry, then leave the list empty
	cfg_partitionEntry_t *pRegParts = GetPartitionTableFromRegistry(cRegParts);
    if (!pRegParts)
        return;

    // If we failed to get vos info, then only show the registry info
	vos_partitionEntry_t *pVosParts = GetPartitionTableFromVos(cVosParts);
    if (!pVosParts)
        cVosParts = 0;

	// We have two partition tables, one from the registry and one from the vos
	// library.  The one from the vos library tells us the partitions that are
	// currently exported.  The one from the registry tells us the partitions
	// that will be exported after the file server restarts.  The registry list
	// should always be at least as big as the vos list.  The vos list can be
	// smaller if one of the disks could not be exported.  To add a new partition,
	// an entry must be added to the registry table and then the file server must
	// be restarted.  To remove an entry, the partition must not be exported (due
	// to an error) or the file server must not be running, in which case nothing
	// will be exported.

	// To display the partitions to the user, we use the list from the registry,
	// looking up each of its entries in the vos list to see if it is exported,
	// and if it is, to get its size info.


	for (int nCurRegPart = 0; nCurRegPart < cRegParts; nCurRegPart++) {
        LPTSTR pPartNameAsString = AnsiToString(pRegParts[nCurRegPart].partitionName);

        // Show the partition in the list
		FASTLISTADDITEM ai = { 0, DISK_DRIVE_IMAGE, IMAGE_NOIMAGE, pPartNameAsString, 0, 0 };
		HLISTITEM hItem = FastList_AddItem(hPartitionList, &ai);

        FreeString(pPartNameAsString);

		FastList_SetItemText(hPartitionList, hItem, 1, pRegParts[nCurRegPart].deviceName);
		
		// For the rest of the info we need to know if this guy is exported.
		// Look him up in the vos table.
		BOOL bExported = FALSE;
		int nTotalSpace = 0;
		int nFreeSpace = 0;
		
		for (int nCurVosPart = 0; nCurVosPart < cVosParts; nCurVosPart++) {
			if (stricmp(pVosParts[nCurVosPart].name, pRegParts[nCurRegPart].partitionName) == 0) {
				bExported = TRUE;
				nTotalSpace = pVosParts[nCurVosPart].totalSpace;
				nFreeSpace = pVosParts[nCurVosPart].totalFreeSpace;
				break;
			}
		}

		FastList_SetItemText(hPartitionList, hItem, 2, bExported ? szYes : szNo);
		FastList_SetItemText(hPartitionList, hItem, 3, bExported ? DiskSpaceToString(nTotalSpace) : TEXT(""));
		FastList_SetItemText(hPartitionList, hItem, 4, bExported ? DiskSpaceToString(nFreeSpace) : TEXT(""));
		
		// Set the item param to indicate that this partition is exported or not
		FastList_SetItemParam(hPartitionList, hItem, bExported);
	
        
    }
}

static void CheckEnableSalvage()
{
	ENABLE_STATE es = (FastList_GetItemCount(hPartitionList) > 0) ? ES_ENABLE : ES_DISABLE;

	SetEnable(hDlg, IDC_SALVAGE, es);
	SetEnable(hDlg, IDC_SALVAGE_MSG, es);
}

