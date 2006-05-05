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
#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "afscfg.h"
#include <stdlib.h>
#include "resource.h"
#include "volume_utils.h"


/*
 * DEFINITIONS _________________________________________________________________
 *
 */
static HWND hDlg = 0;
static HWND hDriveList = 0;
static BOOL bCantCreate = FALSE;
static HLISTITEM hSelectedItem = 0;

//	The idea for this var is that if the user clicks on a drive and the vice name
//	hasn't been set by the user, then we will set the vice name to the drive letter 
//	selected.  However, if the user sets the vice name, then clicking on a drive 
//	letter doesn't change the vice name.  This var tells us whether it is ok to set
//	the vice name or not.
static BOOL bAutoSetPartitionName = TRUE;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg);
static void EnableDriveListCtrls(BOOL bEnable = TRUE);
static BOOL SavePartitionInfo(BOOL bValidate);
static void ShowPartitionInfo();
static void CheckEnableButtons();
static void OnListSelection(LPFLN_ITEMSELECT_PARAMS pItemParms);
static void CantMakePartition(UINT nMsgID);
static void MustMakePartition();
static void OnPartitionName();


/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */

/*
 * Dialog Proc _________________________________________________________________
 *
 */
/*
 * WndProc for our subclass of the wizard's dialog.  We do this to detect
 * the WM_ACTIVATEAPP message, which we use as a trigger to refresh the
 * partition info that we display.
 */
static BOOL CALLBACK WizardDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if ((g_pWiz->GetState() == sidSTEP_EIGHT) && (uMsg == WM_ACTIVATEAPP) && wParam) {
		UpdateDriveList();
		ShowPartitionInfo();
	}		

	return CallWindowProc((WNDPROC)Subclass_FindNextHook(hwndDlg, WizardDlgProc), hwndDlg, uMsg, wParam, lParam);
}

BOOL CALLBACK PartitionPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	if (WizStep_Common_DlgProc (hwndDlg, msg, wp, lp))
		return FALSE;

	switch (msg) {
		case WM_INITDIALOG:
	         OnInitDialog(hwndDlg);
			 CheckEnableButtons();
		     break;

		case WM_DESTROY_SHEET:	
			Subclass_RemoveHook(g_pWiz->GetWindow(), WizardDlgProc);
			break;

		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDNEXT:
					if (SavePartitionInfo(TRUE))
					    g_pWiz->SetState(sidSTEP_NINE);
					break;

				case IDBACK:
					if (SavePartitionInfo(FALSE))
					    g_pWiz->SetState(sidSTEP_SEVEN);
					break;

				case IDC_CREATE_PARTITION:
					g_CfgData.configPartition = CS_CONFIGURE;
					CheckEnableButtons();
					EnableDriveListCtrls();
					break;

				case IDC_DONT_CREATE_PARTITION:
					g_CfgData.configPartition = CS_DONT_CONFIGURE;
					CheckEnableButtons();
					EnableDriveListCtrls(FALSE);
					break;

				case IDC_PARTITION_NAME:
					if (HIWORD(wp) == EN_CHANGE) {
						OnPartitionName();
						SetFocus((HWND)lp);
					}
					break;
			}
			break;

		case WM_NOTIFY:	
			switch (((LPNMHDR)lp)->code) {
				case FLN_ITEMSELECT:	OnListSelection((LPFLN_ITEMSELECT_PARAMS)lp);
			}
			break;

	}

	return FALSE;
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
	HOURGLASS hg;

	hDlg = hwndDlg;

	hDriveList = GetDlgItem(hDlg, IDC_DRIVE_LIST);

	g_pWiz->SetButtonText(IDNEXT, IDS_NEXT);
	g_pWiz->SetDefaultControl(IDNEXT);

	if (g_CfgData.configPartition == CS_ALREADY_CONFIGURED) {
		CantMakePartition(IDS_PARTITION_ALREADY_CREATED);
		return;
	}

	// Should this step be disabled?  Yes, if this machine is
	// not configured as a file server.
	if (!ConfiguredOrConfiguring(g_CfgData.configFS)) {
		CantMakePartition(IDS_NOT_A_FS_SERVER);
		EnableStep(g_CfgData.configPartition, FALSE);
		return;
	}

	// Do this in case it was disabled the last time
	EnableStep(g_CfgData.configPartition);

	switch (g_CfgData.configPartition) {
		case CS_DONT_CONFIGURE:
			SetCheck(hDlg, IDC_DONT_CREATE_PARTITION);
			EnableDriveListCtrls(FALSE);
			break;

		case CS_CONFIGURE:
		default:
			SetCheck(hDlg, IDC_CREATE_PARTITION);
			EnableDriveListCtrls();
			break;
	}

	Subclass_AddHook(g_pWiz->GetWindow(), WizardDlgProc);

	SetupDriveList(hDriveList);
	UpdateDriveList();
	ShowPartitionInfo();

	if (g_CfgData.bFirstServer)
		MustMakePartition();
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
				LPCTSTR pDrive = FastList_GetItemText(hDriveList, hSelectedItem, 0);
				g_CfgData.szPartitionName[0] = _totlower(pDrive[0]);
				g_CfgData.szPartitionName[1] = 0;
				SetWndText(hDlg, IDC_PARTITION_NAME, g_CfgData.szPartitionName);

				// Must set this to true because the call to SetWndText will cause
				// a call to OnPartitionName, which would incorrectly think that the
				// Partition Name had been set by the user rather than by us, thus
				// setting bAutoSetPartitionName to false.
				bAutoSetPartitionName = TRUE;
			}
		}
	}

	CheckEnableButtons();
}


/*
 * Utility Functions _________________________________________________________________
 *
 */
static void CantMakePartition(UINT nMsgID)
{
	TCHAR szMsg[cchRESOURCE];

	GetString(szMsg, nMsgID);

	ShowWnd(hDlg, IDC_CREATE_PARTITION, FALSE);
	ShowWnd(hDlg, IDC_DONT_CREATE_PARTITION, FALSE);
	ShowWnd(hDlg, IDC_ASK_CREATE_PARTITION, FALSE);
	ShowWnd(hDlg, IDC_SELECT_DRIVE, FALSE);
	ShowWnd(hDlg, IDC_DRIVE_LIST, FALSE);
	ShowWnd(hDlg, IDC_NAME_LABEL, FALSE);
	ShowWnd(hDlg, IDC_PARTITION_NAME, FALSE);

	ShowWnd(hDlg, IDC_PARTITION_COVER);
	HWND hMsg = GetDlgItem(hDlg, IDC_PARTITION_MSG);
	ShowWindow(hMsg, SW_SHOW);
	SetWindowText(hMsg, szMsg);

	bCantCreate = TRUE;
}

static void MustMakePartition()
{
	TCHAR szMsg[cchRESOURCE];

	GetString(szMsg, IDS_MUST_MAKE_PARTITION);
	
	ShowWnd(hDlg, IDC_CREATE_PARTITION, FALSE);
	ShowWnd(hDlg, IDC_DONT_CREATE_PARTITION, FALSE);

	SetWndText(hDlg, IDC_ASK_CREATE_PARTITION, szMsg);
}

static void EnableDriveListCtrls(BOOL bEnable)
{
	EnableWnd(hDlg, IDC_SELECT_DRIVE, bEnable);
	EnableWnd(hDlg, IDC_DRIVE_LIST, bEnable);
	EnableWnd(hDlg, IDC_NAME_LABEL, bEnable);
	EnableWnd(hDlg, IDC_PARTITION_NAME, bEnable);
}

static BOOL SavePartitionInfo(BOOL bValidate)
{
	if (bCantCreate)
		return TRUE;

    if (GetButtonState(hDlg, IDC_CREATE_PARTITION) != BST_CHECKED) {
	    g_CfgData.szPartitionName[0] = 0;
	    bAutoSetPartitionName = TRUE;
	} else {
	    GetWindowText(GetDlgItem(hDlg, IDC_PARTITION_NAME), g_CfgData.szPartitionName, MAX_PARTITION_NAME_LEN);
        if (bValidate && !Validation_IsValid(g_CfgData.szPartitionName, VALID_AFS_PARTITION_NAME))
            return FALSE;
    }

	if (hSelectedItem == 0)
		g_CfgData.chDeviceName = 0;
	else {
		LPCTSTR pDrive = FastList_GetItemText(hDriveList, hSelectedItem, 0);
		g_CfgData.chDeviceName = pDrive[0];
	}

    return TRUE;
}

static void ShowPartitionInfo()
{
    // The SetWndText call below will mess up our bAutoSetPartitionName variable.
    // It will trigger the change event on the partition name field causing our 
    // OnPartitionName function to get called, and making it look to us like the 
    // user set the partition name.  Therefore, we will save the current value,
    // make the call, then restore the value.
    BOOL bAutoSet = bAutoSetPartitionName;	
    SetWndText(hDlg, IDC_PARTITION_NAME, g_CfgData.szPartitionName);
    bAutoSetPartitionName = bAutoSet;

	if (g_CfgData.chDeviceName != 0) {
		HLISTITEM hItem = NULL;
		while ((hItem = FastList_FindNext(hDriveList, hItem)) != NULL) {
			LPCTSTR pDrive = FastList_GetItemText(hDriveList, hItem, 0);
			if (pDrive[0] == g_CfgData.chDeviceName) {
				FastList_SelectItem(hDriveList, hItem, TRUE);
				hSelectedItem = hItem;
				break;
			}
		}
	}
}

static void CheckEnableButtons()
{
	if (IsButtonChecked(hDlg, IDC_CREATE_PARTITION)) {
		TCHAR szBuf[MAX_PARTITION_NAME_LEN];

		GetWindowText(GetDlgItem(hDlg, IDC_PARTITION_NAME), szBuf, MAX_PARTITION_NAME_LEN);
		if ((hSelectedItem == 0) || (szBuf[0] == 0)) {
			g_pWiz->EnableButtons(BACK_BUTTON);
			g_pWiz->SetDefaultControl(IDBACK);
			return;
		}
	}
		
	g_pWiz->EnableButtons(BACK_BUTTON | NEXT_BUTTON);
}

