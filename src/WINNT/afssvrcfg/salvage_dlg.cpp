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
#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "afscfg.h"		// Main header for this application
#include "resource.h"
extern "C" {
#include "afs_bosAdmin.h"
}
#include "admin_info_dlg.h"


/*
 * DEFINITIONS _________________________________________________________________
 *
 */
static const int DEFAULT_NUM_PROCESSES		= 4;
static const char *DEFAULT_LOG_FILE			= "";
static const int NUM_PROCS_BUF_SIZE			= 5;
static const int MIN_NUM_PROCESSES			= 1;
static const int MAX_NUM_PROCESSES			= 32;

static HWND hDlg = 0;						// HWND for this page's dialog
static BOOL bAdvanced;
static TCHAR szPartitionName[25];
static TCHAR szVolumeName[cchRESOURCE];
static TCHAR szNumProcesses[NUM_PROCS_BUF_SIZE];
static TCHAR szTempDir[_MAX_PATH];
static LPTSTR pszPartitionName;
static LPTSTR pszVolumeName;
static int nNumProcesses;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg);
static void OnAdvanced();
static void UpdateControls();
static BOOL OnSalvage();
static DWORD WINAPI Salvage(LPVOID param);

BOOL CALLBACK SalvageDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);


/*
 * EXPORTED FUNCTIONS _________________________________________________________
 *
 */
BOOL ShowSalvageDlg(HWND hParent, LPCTSTR pszPartitionName)
{	
    ASSERT(pszPartitionName);

    lstrcpy(szPartitionName, pszPartitionName);

    int nResult = ModalDialog(IDD_SALVAGE, hParent, (DLGPROC)SalvageDlgProc);

    if (nResult != IDOK)
        return FALSE;

    // Create a thread to perform the salvage
    DWORD dwThreadID;
    g_CfgData.hSalvageThread = CreateThread(0, 0, Salvage, 0, 0, &dwThreadID);

    return (g_CfgData.hSalvageThread != 0);
}


/*
 * Dialog Proc _________________________________________________________________
 *
 */
BOOL CALLBACK SalvageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    if (AfsAppLib_HandleHelp(IDD_SALVAGE, hwndDlg, msg, wp, lp))
	return TRUE;

    switch (msg) {
    case WM_INITDIALOG:
	OnInitDialog(hwndDlg);
	break;

    case WM_COMMAND:
	switch (LOWORD(wp)) {
	case IDC_VOLUME_NAME:
	case IDC_NUM_PROCESSES:
	case IDC_LOG_FILE:
	case IDC_TEMP_DIR:
	    if (HIWORD(wp) == EN_CHANGE)
		UpdateControls();
	    break;

	case IDC_SERVER:
	case IDC_PARTITION:
	case IDC_VOLUME:
	case IDC_NUM_PROCESSES_CHECKBOX:
	    UpdateControls();
	    break;

	case IDC_ADVANCED:
	    OnAdvanced();
	    break;

	case IDCANCEL:
	    EndDialog(hDlg, IDCANCEL);
	    break;

	case IDOK:
	    if (OnSalvage())
		EndDialog(hDlg, IDOK);
	    break;
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
    hDlg = hwndDlg;

    bAdvanced = TRUE;

    TCHAR szNumProcesses[32];
    _itot(DEFAULT_NUM_PROCESSES, szNumProcesses, 10);

    SetWndText(hDlg, IDC_NUM_PROCESSES, szNumProcesses);
    SetCheck(hDlg, IDC_NUM_PROCESSES_CHECKBOX);
    SetWndText(hDlg, IDC_LOG_FILE, A2S(DEFAULT_LOG_FILE));

    // If a partition name isn't selected, then only allow the salvage server option
    if (szPartitionName[0] == 0) {
	SetEnable(hDlg, IDC_PARTITION, ES_DISABLE);
	SetEnable(hDlg, IDC_VOLUME, ES_DISABLE);
	SetCheck(hDlg, IDC_SERVER);
    } else
	SetCheck(hDlg, IDC_PARTITION);
	
    // Close the Advanced portion of the dialog
    OnAdvanced();
}

static void OnAdvanced()
{
    static int nOffset = 0;

    bAdvanced = !bAdvanced;

    ShowAndEnable(hDlg, IDC_ADVANCED_FRAME, bAdvanced);	
    ShowAndEnable(hDlg, IDC_LOG_FILE, bAdvanced);
    ShowAndEnable(hDlg, IDC_LOG_FILE_LABEL, bAdvanced);
    ShowAndEnable(hDlg, IDC_NUM_PROCESSES, bAdvanced);
    ShowAndEnable(hDlg, IDC_TEMP_DIR, bAdvanced);
    ShowAndEnable(hDlg, IDC_TEMP_DIR_LABEL, bAdvanced);
    ShowAndEnable(hDlg, IDC_NUM_PROCESSES_CHECKBOX, bAdvanced);
    ShowAndEnable(hDlg, IDC_DAMAGED_VOLUMES, bAdvanced);
    ShowAndEnable(hDlg, IDC_SMALL_BLOCK_READS, bAdvanced);
    ShowAndEnable(hDlg, IDC_FORCE_SALVAGE, bAdvanced);
    ShowAndEnable(hDlg, IDC_FORCE_REBUILD, bAdvanced);
    ShowAndEnable(hDlg, IDC_LIST_DAMAGED_INODES, bAdvanced);
    ShowAndEnable(hDlg, IDC_LIST_OWNED_INDOES, bAdvanced);

    // To show or hide the advanced section, we have to resize the dialog

    // Get current position of the dialog
    RECT rectDlg;
    GetWindowRect(hDlg, &rectDlg);

    // Figure out offset between full dialog and short dialog
    if (nOffset == 0) {
        // Find the frame containing the things we will hide or show
        HWND hFrame = GetDlgItem(hDlg, IDC_ADVANCED_FRAME);
    
        // Get its dimensions
        RECT rectFrame;
        GetWindowRect(hFrame, &rectFrame);

        // Find the distance between the bottom of the dialog and the top of the frame    
        nOffset = rectDlg.bottom - rectFrame.top - 3;
    }

    int nCurOffset = nOffset;

    if (!bAdvanced)
	nCurOffset *= -1;
	
    // Adjust dialog position
    MoveWindow(hDlg, rectDlg.left, rectDlg.top, rectDlg.right - rectDlg.left, rectDlg.bottom - rectDlg.top + nCurOffset, TRUE);

    SetWndText(hDlg, IDC_ADVANCED, bAdvanced ? IDS_ADVANCED_OPEN : IDS_ADVANCED_CLOSED);
}

static void UpdateControls()
{
    // Update volume name controls
    BOOL bVolume = IsButtonChecked(hDlg, IDC_VOLUME);
    ENABLE_STATE es = bVolume ? ES_ENABLE : ES_DISABLE;
    SetEnable(hDlg, IDC_VOLUME_NAME, es);
    SetEnable(hDlg, IDC_VOLUME_NAME_LABEL, es);
    GetWndText(hDlg, IDC_VOLUME_NAME, szVolumeName);

    // Num processes edit control
    BOOL bParallel = IsButtonChecked(hDlg, IDC_NUM_PROCESSES_CHECKBOX);
    SetEnable(hDlg, IDC_NUM_PROCESSES, (ENABLE_STATE)bParallel);
    GetWndText(hDlg, IDC_NUM_PROCESSES, szNumProcesses, NUM_PROCS_BUF_SIZE);

    GetWndText(hDlg, IDC_LOG_FILE, g_CfgData.szSalvageLogFileName, _MAX_PATH);
    GetWndText(hDlg, IDC_TEMP_DIR, szTempDir, _MAX_PATH);

    // Should OK button be enabled or disabled?
    BOOL bEnable = TRUE;

    if (bVolume)
	bEnable = !!lstrlen(szVolumeName);

    if (bEnable && bParallel)
	bEnable = !!lstrlen(szNumProcesses);

    SetEnable(hDlg, IDOK, (ENABLE_STATE)bEnable);
}	

/*
 * Utility Functions _________________________________________________________________
 *
 */
static BOOL OnSalvage()
{
    if (IsButtonChecked(hDlg, IDC_SERVER)) {
	pszPartitionName = 0;
	pszVolumeName = 0;
    } else if (IsButtonChecked(hDlg, IDC_PARTITION)) {
	pszPartitionName = szPartitionName;
	pszVolumeName = 0;
    } else if (IsButtonChecked(hDlg, IDC_VOLUME)) {
	pszPartitionName = szPartitionName;
	pszVolumeName = szVolumeName;
    }

    nNumProcesses = DEFAULT_NUM_PROCESSES;
    if (IsButtonChecked(hDlg, IDC_NUM_PROCESSES_CHECKBOX)) {
	nNumProcesses = _ttoi(szNumProcesses);
	if ((nNumProcesses < MIN_NUM_PROCESSES) || (nNumProcesses > MAX_NUM_PROCESSES)) {
	    ShowError(hDlg, 0, IDS_INVALID_NUM_SALVAGE_PROCESSSES);
	    return FALSE;
	}
    }

    if (!g_CfgData.bReuseAdminInfo) {
        if (!GetAdminInfo(hDlg, GAIO_LOGIN_ONLY))
	    return FALSE;

        if (!GetHandles(hDlg))
            return FALSE;
    }	

    return TRUE;
}

static DWORD WINAPI Salvage(LPVOID param)
{
    afs_status_t nStatus;
    void *hServer;
    int nResult; 

    nResult = bos_ServerOpen(g_hCell, GetHostnameA(), &hServer, &nStatus);
    if (!nResult) {
	ShowError(hDlg, nStatus, IDS_BOS_OPEN_FAILED);
	return FALSE;
    }

    nResult = bos_Salvage(g_hCell, hServer, S2A(pszPartitionName), S2A(pszVolumeName), nNumProcesses, S2A(szTempDir), 0, VOS_NORMAL,
			   BOS_SALVAGE_DAMAGED_VOLUMES, BOS_SALVAGE_DONT_WRITE_INODES, BOS_SALVAGE_DONT_WRITE_ROOT_INODES,
			   BOS_SALVAGE_DONT_FORCE_DIRECTORIES, BOS_SALVAGE_DONT_FORCE_BLOCK_READS, &nStatus);
    if (!nResult)
    	ShowError(hDlg, nStatus, IDS_SALVAGE_ERROR);

    bos_ServerClose(hServer, &nStatus);
    
    g_CfgData.bReuseAdminInfo = nResult;

    return nResult;
}

