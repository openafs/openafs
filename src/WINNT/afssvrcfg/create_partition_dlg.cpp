/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * INCLUDES ___________________________________________________________________
 *
 */
extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "afscfg.h"		// Main header for this application
#include "resource.h"
#include "create_partition_dlg.h"
#include "volume_utils.h"
#include "partition_utils.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */
static HWND hDlg = 0;								// This dialog's HWND
static HWND hDriveList = 0;
static BOOL bAutoSetPartitionName = TRUE;
static HLISTITEM hSelectedItem = 0;
static BOOL bCreated;								// TRUE if a partition was created

static const UINT IDD = IDD_CREATE_PARTITION;		// Dialog's resource ID

static TCHAR szPartitionName[MAX_PARTITION_NAME_LEN];
static TCHAR szDrive[4];
static TCHAR szSize[32];

static rwWindowData arwDialog[] = {
	{ IDC_TITLE,		raSizeX | raRepaint,	0,	0 },
	{ IDC_DRIVE_LIST,	raSizeX | raSizeY, 	MAKELONG(350, 150), 0 },
	{ IDC_ARGS_FRAME,	raSizeX | raMoveY,	0,	0 },
	{ IDC_NAME_STATIC,	raMoveY | raRepaint,	0,	0 },
	{ IDC_VICEP_STATIC,	raMoveY | raRepaint,	0,	0 },
	{ IDC_PARTITION_NAME,	raMoveY | raRepaint,	0,	0 },
	{ IDC_CREATE,		raMoveX | raMoveY,	0,	0 },
	{ IDC_CLOSE,		raMoveX | raMoveY,	0,	0 },
	{ 9,			raMoveX | raMoveY,	0,	0 },
	{ idENDLIST,		0,			0,	0 }
};


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static BOOL CALLBACK DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void OnInitDialog(HWND hwndDlg);
static void OnClose();
static void OnCreate();
static void OnListSelection(LPFLN_ITEMSELECT_PARAMS pItemParms);
static void OnPartitionName();
static void CheckEnableButtons();


/*
 * EXPORTED FUNCTIONS _________________________________________________________
 *
 */
BOOL CreatePartition(HWND hParent)
{	
	ModalDialog(IDD, hParent, (DLGPROC)DlgProc);

	return bCreated;
}


/*
 * STATIC FUNCTIONS ___________________________________________________________
 *
 */

/*
 * Dialog Proc ________________________________________________________________
 *
 */
static BOOL CALLBACK DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (AfsAppLib_HandleHelp(IDD_CREATE_PARTITION, hwndDlg, uMsg, wParam, lParam))
		return TRUE;

	switch (uMsg) {
		case WM_INITDIALOG:	OnInitDialog(hwndDlg);
							break;
				
		case WM_COMMAND:	switch (LOWORD(wParam)) {
								case IDC_CLOSE:		OnClose();
													break;

								case IDC_CREATE:	OnCreate();
													break;

								case IDC_PARTITION_NAME:
									if (HIWORD(wParam) == EN_CHANGE) {
										OnPartitionName();
										SetFocus((HWND)lParam);
									}
									break;
							}
							break;
	
		case WM_NCACTIVATE:	if (wParam)
								UpdateDriveList();
							break;

		case WM_NOTIFY:	if ((((LPNMHDR)lParam)->code) == FLN_ITEMSELECT)
							OnListSelection((LPFLN_ITEMSELECT_PARAMS)lParam);
						break;

		case WM_SIZE:	if (lParam != 0)
							ResizeWindow(hwndDlg, arwDialog, rwaFixupGuts);
						break;

	}

	return FALSE;
}


/*
 * Event Handler Functions ____________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg)
{
	hDlg = hwndDlg;

	ResizeWindow(hDlg, arwDialog, rwaFixupGuts);

	hDriveList = GetDlgItem(hDlg, IDC_DRIVE_LIST);

	bAutoSetPartitionName = TRUE;
	hSelectedItem = 0;

	SetupDriveList(hDriveList);

	bCreated = FALSE;
}

static void OnClose()
{
	EndDialog(hDlg, IDCANCEL);
}

static void OnCreate()
{
	ASSERT(g_hServer);
	
	HLISTITEM hItem = FastList_FindFirstSelected(hDriveList);
	_ASSERTE(hItem);

	GetWindowText(GetDlgItem(hDlg, IDC_PARTITION_NAME), szPartitionName, sizeof(szPartitionName));
    if (!Validation_IsValid(szPartitionName, VALID_AFS_PARTITION_NAME))
        return;

	lstrcpy(szDrive, FastList_GetItemText(hDriveList, hItem, 0));
	lstrcpy(szSize, FastList_GetItemText(hDriveList, hItem, 2));

	// Constuct the device name
	char szDevNameA[] = "?:";
	S2A drive(szDrive);
    szDevNameA[0] = ((char*)drive)[0];

	// Construct the partition name
	char szPartNameA[9] = "/vicep";
	strncat(szPartNameA, (char *)S2A(szPartitionName), 2);

	if (DoesPartitionExist(A2S(szPartNameA))) {
		MsgBox(hDlg, IDS_PARTITION_EXISTS, GetAppTitleID(), MB_OK | MB_ICONSTOP);
		return;
	}

	g_LogFile.Write("Adding an AFS partition on device '%s' with name '%s'.\r\n", szDevNameA, szPartNameA);

	afs_status_t nStatus;
	int nResult = cfg_HostPartitionTableAddEntry(g_hServer, szPartNameA, szDevNameA, &nStatus);
	if (!nResult) {
		ShowError(hDlg, nStatus, IDS_CREATE_PARTITION_ERROR);
		return;
	}

	MsgBox(hDlg, IDS_PARTITION_CREATED, GetAppTitleID(), MB_OK);

	bCreated = TRUE;

	SetWndText(hDlg, IDC_PARTITION_NAME, TEXT(""));
}

static void OnPartitionName()
{
	TCHAR szBuf[MAX_PARTITION_NAME_LEN];
	GetWindowText(GetDlgItem(hDlg, IDC_PARTITION_NAME), szBuf, MAX_PARTITION_NAME_LEN);

	bAutoSetPartitionName = szBuf[0] == 0;
	
	CheckEnableButtons();
}

static void OnListSelection(LPFLN_ITEMSELECT_PARAMS pItemParms)
{
	ASSERT(pItemParms);

	hSelectedItem = 0;

	if (pItemParms->hItem) {
		LPARAM lParam = FastList_GetItemParam(hDriveList, pItemParms->hItem);
		if (lParam == 0) {
			hSelectedItem = pItemParms->hItem;

			if (bAutoSetPartitionName) {
				TCHAR szName[2];
				LPCTSTR pDrive = FastList_GetItemText(hDriveList, hSelectedItem, 0);
				szName[0] = _totlower(pDrive[0]);
				szName[1] = 0;
				
				TCHAR szFullName[6] = TEXT("vice");
				lstrcat(szFullName, szName);

				if (!DoesPartitionExist(szFullName)) {
					SetWndText(hDlg, IDC_PARTITION_NAME, szName);

					// Must set this to true because the call to SetWndText will cause
					// a call to OnPartitionName, which would incorrectly think that the
					// Partition Name had been set by the user rather than by us, thus
					// setting bAutoSetPartitionName to false.
					bAutoSetPartitionName = TRUE;
				}
			}
		}
	}

	CheckEnableButtons();
}

static void CheckEnableButtons()
{
	TCHAR szBuf[MAX_PARTITION_NAME_LEN];

	GetWindowText(GetDlgItem(hDlg, IDC_PARTITION_NAME), szBuf, MAX_PARTITION_NAME_LEN);
	
	ENABLE_STATE es = ES_ENABLE;
	
	if ((hSelectedItem == 0) || (szBuf[0] == 0))
		es = ES_DISABLE;

	SetEnable(hDlg, IDC_CREATE, es);
}

