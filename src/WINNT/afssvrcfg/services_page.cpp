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
#include "resource.h"
#include <stdio.h>
#include "admin_info_dlg.h"
#include "get_cur_config.h"


/*					 
 * DEFINITIONS _________________________________________________________________
 *
 */
static HWND hDlg = 0;
static BOOL bSettingScMachine;

// These indicate if the named service is currently running, and if the user wants it on
// or off.
static BOOL bFsRunning = FALSE;
static BOOL bFsOn = FALSE;

static BOOL bDbRunning = TRUE;
static BOOL bDbOn = FALSE;
static BOOL bDbParial = FALSE;

static BOOL bBakRunning = FALSE;
static BOOL bBakOn = FALSE;

static BOOL bScsRunning = FALSE;
static BOOL bScsOn = FALSE;

static BOOL bSccRunning = FALSE;
static BOOL bSccOn = FALSE;

static TCHAR szScMachine[MAX_MACHINE_NAME_LEN + 1];


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
// From config_server_page.cpp
BOOL Configure(HWND hParent, BOOL& bMustExit);

static void OnInitDialog(HWND hwndDlg);
static void ShowInitialConfig();
static void ShowServiceStates();
static void OnDbService();
static void OnBakService();
static void OnFsService();
static void OnScServer();
static void OnScClient();
static void CheckEnableSc();
static void EnableScMachine(BOOL bEnable = TRUE);
static void OnScMachineChange();
static void CheckEnableBak();
static BOOL PrepareToConfig(CONFIG_STATE& state, BOOL bRunning, BOOL bOn, UINT uiCtrlID);
static BOOL PrepareToConfig();
static void CheckEnableApply();


/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */

/*
 * Dialog Proc _________________________________________________________________
 *
 */
BOOL CALLBACK ServicesPageDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (AfsAppLib_HandleHelp(IDD_SERVICES_PAGE, hwndDlg, uMsg, wParam, lParam))
		return TRUE;

	switch (uMsg) {
		case WM_INITDIALOG:		OnInitDialog(hwndDlg);
								break;
 
		case WM_COMMAND:	switch (LOWORD(wParam)) {
								case IDC_DB_SERVICE:	OnDbService();
														break;

								case IDC_BK_SERVICE:	OnBakService();
														break;

								case IDC_FS_SERVICE:	OnFsService();
														break;

								case IDC_SCC:			OnScClient();
														break;

								case IDC_SCS:			OnScServer();
														break;

								case IDC_SC_MACHINE:	if (HIWORD(wParam) == EN_CHANGE) {
															OnScMachineChange();
														}
														break;
							
								case IDAPPLY:			PrepareToConfig();
														break;
							}
							break;
	}

	CheckEnableApply();

	return FALSE;
}	


/*
 * STATIC FUNCTIONS _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg)
{
	hDlg = hwndDlg;

	// Show the initial services config
	ShowInitialConfig();
	ShowServiceStates();

	MakeBold(hDlg, IDC_DB_SERVICE);
	MakeBold(hDlg, IDC_FS_SERVICE);
	MakeBold(hDlg, IDC_BK_SERVICE);
	MakeBold(hDlg, IDC_SCC);
	MakeBold(hDlg, IDC_SCS);
}

static void EnableScMachine(BOOL bEnable)
{
	SetEnable(hDlg, IDC_SC_MACHINE_LABEL, (ENABLE_STATE)bEnable);
	SetEnable(hDlg, IDC_SC_MACHINE, (ENABLE_STATE)bEnable);

	bSettingScMachine = TRUE;
	SetWndText(hDlg, IDC_SC_MACHINE, bEnable ? szScMachine : TEXT(""));
	bSettingScMachine = FALSE;
}

static void ShowInitialConfig()
{
	// FS
	bFsRunning = Configured(g_CfgData.configFS);
	if (bFsRunning) {
		SetCheck(hDlg, IDC_FS_SERVICE);
		SetWndText(hDlg, IDC_FS_STATUS_MSG, IDS_FS_RUNNING);
		SetWndText(hDlg, IDC_FS_ACTION_MSG, IDS_FS_HOW_TO_STOP);
	} else {
		SetCheck(hDlg, IDC_FS_SERVICE, FALSE);
		SetWndText(hDlg, IDC_FS_STATUS_MSG, IDS_FS_STOPPED);
		SetWndText(hDlg, IDC_FS_ACTION_MSG, IDS_FS_HOW_TO_RUN);
	}

	bFsOn = bFsRunning;

    // DB
	Set2State(hDlg, IDC_DB_SERVICE);
	
	bDbRunning = Configured(g_CfgData.configDB);
	if (bDbRunning) {
		SetCheck(hDlg, IDC_DB_SERVICE);
		SetWndText(hDlg, IDC_DB_STATUS_MSG, IDS_DB_RUNNING);
		SetWndText(hDlg, IDC_DB_ACTION_MSG, IDS_DB_HOW_TO_STOP);
	} else if (bDbParial) {
		SetCheck(hDlg, IDC_DB_SERVICE, BST_INDETERMINATE);
		SetWndText(hDlg, IDC_DB_STATUS_MSG, IDS_DB_PARTIAL_CONFIG);
		SetWndText(hDlg, IDC_DB_ACTION_MSG, IDS_DB_DETAILS);
		ShowWnd(hDlg, IDC_DB_DETAILS);
		Set3State(hDlg, IDC_DB_SERVICE);
	} else {
		SetCheck(hDlg, IDC_DB_SERVICE, FALSE);
		SetWndText(hDlg, IDC_DB_STATUS_MSG, IDS_DB_STOPPED);
		SetWndText(hDlg, IDC_DB_ACTION_MSG, IDS_DB_HOW_TO_RUN);
	}

	bDbOn = bDbRunning;

	// BK
	bBakRunning = Configured(g_CfgData.configBak);
	SetEnable(hDlg, IDC_BK_SERVICE, ES_ENABLE);
	if (bBakRunning) {
		SetCheck(hDlg, IDC_BK_SERVICE);
		SetWndText(hDlg, IDC_BK_STATUS_MSG, IDS_BK_RUNNING);
		SetWndText(hDlg, IDC_BK_ACTION_MSG, IDS_BK_HOW_TO_STOP);
	} else {
		SetCheck(hDlg, IDC_BK_SERVICE, FALSE);
		SetWndText(hDlg, IDC_BK_STATUS_MSG, IDS_BK_STOPPED);
		SetWndText(hDlg, IDC_BK_ACTION_MSG, IDS_BK_HOW_TO_RUN);
	}

	bBakOn = bBakRunning;

	// SC Server
	bScsRunning = Configured(g_CfgData.configSCS);
	SetEnable(hDlg, IDC_SCS, ES_ENABLE);
	if (bScsRunning) {
		SetCheck(hDlg, IDC_SCS);
		SetWndText(hDlg, IDC_SCS_ACTION_MSG, IDS_SCS_HOW_TO_STOP);
	} else {
		SetCheck(hDlg, IDC_SCS, FALSE);
		SetWndText(hDlg, IDC_SCS_STATUS_MSG, IDS_SCS_STOPPED);
		SetWndText(hDlg, IDC_SCS_ACTION_MSG, IDS_SCS_HOW_TO_RUN);
	}

	bScsOn = bScsRunning;

	// SC Client
	bSccRunning = Configured(g_CfgData.configSCC);
	SetEnable(hDlg, IDC_SCC, ES_ENABLE);
	if (bSccRunning) {
		SetCheck(hDlg, IDC_SCC);
		SetWndText(hDlg, IDC_SCC_ACTION_MSG, IDS_SCC_HOW_TO_STOP);
		EnableScMachine();
		lstrcpy(szScMachine, g_CfgData.szSysControlMachine);
	} else {
		SetCheck(hDlg, IDC_SCC, FALSE);
		SetWndText(hDlg, IDC_SCC_STATUS_MSG, IDS_SCC_STOPPED);
		SetWndText(hDlg, IDC_SCC_ACTION_MSG, IDS_SCC_HOW_TO_RUN);
		EnableScMachine(FALSE);
	}

	bSccOn = bSccRunning;
}

static void ShowServiceStates()
{
	// FS
	if (bFsOn) {
		if (bFsRunning)
			SetWndText(hDlg, IDC_FS_ACTION_MSG, IDS_FS_HOW_TO_STOP);
		else
			SetWndText(hDlg, IDC_FS_ACTION_MSG, IDS_FS_WILL_RUN);
	} else {
		if (bFsRunning)
			SetWndText(hDlg, IDC_FS_ACTION_MSG, IDS_FS_WILL_STOP);
		else
			SetWndText(hDlg, IDC_FS_ACTION_MSG, IDS_FS_HOW_TO_RUN);
	}

	// DB
	switch (GetButtonState(hDlg, IDC_DB_SERVICE)) {
		case BST_CHECKED:
				if (bDbRunning)
					SetWndText(hDlg, IDC_DB_ACTION_MSG, IDS_DB_HOW_TO_STOP);
				else
					SetWndText(hDlg, IDC_DB_ACTION_MSG, IDS_DB_WILL_RUN);
				break;

		case BST_UNCHECKED:
				if (bDbRunning)
					SetWndText(hDlg, IDC_DB_ACTION_MSG, IDS_DB_WILL_STOP);
				else
					SetWndText(hDlg, IDC_DB_ACTION_MSG, IDS_DB_HOW_TO_RUN);
				break;

		case BST_INDETERMINATE:
				SetWndText(hDlg, IDC_DB_ACTION_MSG, IDS_DB_DETAILS);
				break;
	}

	// For the ones below, in addition to setting the action message, we also
	// set the status message.  This is because the status can change to the
	// disabled state depending on how other servers are configured.  The
	// servers before this cannot have their status change except by re-
	// configuring them.

	// BK
	if (bDbOn) {
		if (IsButtonChecked(hDlg, IDC_BK_SERVICE)) {
			if (bBakRunning) {
				SetWndText(hDlg, IDC_BK_STATUS_MSG, IDS_BK_RUNNING);
				SetWndText(hDlg, IDC_BK_ACTION_MSG, IDS_BK_HOW_TO_STOP);
			} else {
				SetWndText(hDlg, IDC_BK_STATUS_MSG, IDS_BK_STOPPED);
				SetWndText(hDlg, IDC_BK_ACTION_MSG, IDS_BK_WILL_RUN);
			}
		} else {
			if (bBakRunning) {
				SetWndText(hDlg, IDC_BK_STATUS_MSG, IDS_BK_RUNNING);
				SetWndText(hDlg, IDC_BK_ACTION_MSG, IDS_BK_WILL_STOP);
			} else {
				SetWndText(hDlg, IDC_BK_STATUS_MSG, IDS_BK_STOPPED);
				SetWndText(hDlg, IDC_BK_ACTION_MSG, IDS_BK_HOW_TO_RUN);
			}
		}
	}

	CheckEnableBak();

	// SC Server
	if (bFsOn || bDbOn && !bSccOn) {
		if (bScsOn) {
			if (bScsRunning) {
				SetWndText(hDlg, IDC_SCS_STATUS_MSG, IDS_SCS_RUNNING);
				SetWndText(hDlg, IDC_SCS_ACTION_MSG, IDS_SCS_HOW_TO_STOP);
			} else {
				SetWndText(hDlg, IDC_SCS_STATUS_MSG, IDS_SCS_STOPPED);
				SetWndText(hDlg, IDC_SCS_ACTION_MSG, IDS_SCS_WILL_RUN);
			}
		} else {
			if (bScsRunning) {
				SetWndText(hDlg, IDC_SCS_STATUS_MSG, IDS_SCS_RUNNING);
				SetWndText(hDlg, IDC_SCS_ACTION_MSG, IDS_SCS_WILL_STOP);
			} else {
				SetWndText(hDlg, IDC_SCS_STATUS_MSG, IDS_SCS_STOPPED);
				SetWndText(hDlg, IDC_SCS_ACTION_MSG, IDS_SCS_HOW_TO_RUN);
			}
		}
	}

	// SC Client
	if (bFsOn || bDbOn && !bScsOn) {
		if (bSccOn) {
			if (bSccRunning) {
				SetWndText(hDlg, IDC_SCC_STATUS_MSG, IDS_SCC_RUNNING);
				SetWndText(hDlg, IDC_SCC_ACTION_MSG, IDS_SCC_HOW_TO_STOP);
			} else {
				SetWndText(hDlg, IDC_SCC_STATUS_MSG, IDS_SCC_STOPPED);
				SetWndText(hDlg, IDC_SCC_ACTION_MSG, IDS_SCC_WILL_RUN);
			}
		} else {
			if (bSccRunning) {
				SetWndText(hDlg, IDC_SCC_STATUS_MSG, IDS_SCC_RUNNING);
				SetWndText(hDlg, IDC_SCC_ACTION_MSG, IDS_SCC_WILL_STOP);
			} else {
				SetWndText(hDlg, IDC_SCC_STATUS_MSG, IDS_SCC_STOPPED);
				SetWndText(hDlg, IDC_SCC_ACTION_MSG, IDS_SCC_HOW_TO_RUN);
			}
		}

		
	}

	CheckEnableSc();
}

static void OnDbService()
{
	bDbOn = GetButtonState(hDlg, IDC_DB_SERVICE) == BST_CHECKED;

	CheckEnableBak();
	CheckEnableSc();

	ShowServiceStates();
}

static void OnBakService()
{
	bBakOn = !bBakOn;

	ShowServiceStates();
}

static void OnFsService()
{
	bFsOn = !bFsOn;

	CheckEnableSc();

	ShowServiceStates();
}

static void CheckEnableBak()
{
	// Enable/disable bk service based on db service.
	if (!bDbOn) {
		SetCheck(hDlg, IDC_BK_SERVICE, FALSE);
		SetEnable(hDlg, IDC_BK_SERVICE, ES_DISABLE);
		SetWndText(hDlg, IDC_BK_STATUS_MSG, IDS_BK_DISABLED);
		SetWndText(hDlg, IDC_BK_ACTION_MSG, IDS_BK_ENABLE);
	} else {
		SetCheck(hDlg, IDC_BK_SERVICE, bBakOn);
		SetEnable(hDlg, IDC_BK_SERVICE, ES_ENABLE);
	}
}

static void CheckEnableSc()
{
	BOOL bSccEnable = TRUE;
	UINT uiSccStatusMsg;
	UINT uiSccActionMsg;
	BOOL bScsEnable = TRUE;
	UINT uiScsStatusMsg;
	UINT uiScsActionMsg;

	// Disable SCS and SCC?
	if (!bFsOn && !bDbOn) {
		bScsEnable = FALSE;
		uiScsStatusMsg = IDS_SC_DISABLED;
		uiScsActionMsg = IDS_SC_ENABLE;

		bSccEnable = FALSE;
		uiSccStatusMsg = IDS_SC_DISABLED;
		uiSccActionMsg = IDS_SC_ENABLE;
		// Disable SCS?
	} else if (bSccOn) {
		bScsEnable = FALSE;
		uiScsStatusMsg = IDS_SC_DISABLED;
		uiScsActionMsg = IDS_SCS_ENABLE;
		// Disable SCC
	} else if (bScsOn) {
		bSccEnable = FALSE;
		uiSccStatusMsg = IDS_SC_DISABLED;
		uiSccActionMsg = IDS_SCC_ENABLE;
	}		

	// Enable the sc server GUI
	if (bScsEnable) {
		SetEnable(hDlg, IDC_SCS, ES_ENABLE);
		SetCheck(hDlg, IDC_SCS, bScsOn);
	} else {	// Disable the sc server gui
		SetEnable(hDlg, IDC_SCS, ES_DISABLE);
		SetCheck(hDlg, IDC_SCS, FALSE);
		SetWndText(hDlg, IDC_SCS_STATUS_MSG, uiScsStatusMsg);
		SetWndText(hDlg, IDC_SCS_ACTION_MSG, uiScsActionMsg);
	}

	// Enable the sc client GUI
	if (bSccEnable) {
		SetEnable(hDlg, IDC_SCC, ES_ENABLE);
		SetCheck(hDlg, IDC_SCC, bSccOn);
		EnableScMachine(bSccOn);
	} else {	// Disable the sc client gui
		SetEnable(hDlg, IDC_SCC, ES_DISABLE);
		SetCheck(hDlg, IDC_SCC, FALSE);
		SetWndText(hDlg, IDC_SCC_STATUS_MSG, uiSccStatusMsg);
		SetWndText(hDlg, IDC_SCC_ACTION_MSG, uiSccActionMsg);
		EnableScMachine(FALSE);
	}
}

static void OnScServer()
{
	bScsOn = !bScsOn;

	ShowServiceStates();
}

static void OnScClient()
{
	bSccOn = !bSccOn;

	ShowServiceStates();
}

static void OnScMachineChange()
{
	if (!bSettingScMachine && IsButtonChecked(hDlg, IDC_SCC))
		GetWndText(hDlg, IDC_SC_MACHINE, szScMachine, sizeof(szScMachine) / sizeof(szScMachine[0]));
}

static BOOL PrepareToConfig(CONFIG_STATE& state, BOOL bRunning, BOOL bOn, UINT uiCtrlID)
{
	BOOL bEnabled = IsWindowEnabled(GetDlgItem(hDlg, uiCtrlID));

	if (bRunning && (!bOn || !bEnabled))
		state = CS_UNCONFIGURE;
	else if (!bRunning && (bOn && bEnabled))
		state = CS_CONFIGURE;

    return (state == CS_UNCONFIGURE) || (state == CS_CONFIGURE);
}

static BOOL PrepareToConfig()
{
    BOOL bMustExit = FALSE;

    // Use a local copy of the config info to decide what should be configured
    // or unconfigured.  We do this so that if the user cancels for some reason,
    // the real config state will still be what the user expects (what was
    // previously read from the system plus the user's changes).
	CONFIG_STATE configFS = g_CfgData.configFS;     // File server
	CONFIG_STATE configDB = g_CfgData.configDB;     // Database server
	CONFIG_STATE configBak = g_CfgData.configBak;   // Backup server
	CONFIG_STATE configSCS = g_CfgData.configSCS;   // System Control server
	CONFIG_STATE configSCC = g_CfgData.configSCC;   // System Control client

    BOOL bWorkToDo = FALSE;

	bWorkToDo |= PrepareToConfig(configFS, bFsRunning, bFsOn, IDC_FS_SERVICE);
	bWorkToDo |= PrepareToConfig(configDB, bDbRunning, bDbOn, IDC_DB_SERVICE);
    bWorkToDo |= PrepareToConfig(configBak, bBakRunning, bBakOn, IDC_BK_SERVICE);
	bWorkToDo |= PrepareToConfig(configSCS, bScsRunning, bScsOn, IDC_SCS);
	bWorkToDo |= PrepareToConfig(configSCC, bSccRunning, bSccOn, IDC_SCC);

    // If there is nothing to do, then just return TRUE.
    if (!bWorkToDo)
        return TRUE;

	// If we are unconfiguring the last DB server:
	//		1) Warn user and ask for confirmation
	//		2) Unconfigure all other servers that are running on this machine
	//		3) Tell them (after unconfiguring) that they must run the Wizard if they
	//		   wish to reconfigure the machine, then exit the program.
	if (configDB == CS_UNCONFIGURE) {
		if (g_CfgData.bLastDBServer) {
			int nChoice = MsgBox(hDlg, IDS_LAST_DB_SERVER, GetAppTitleID(), MB_YESNO | MB_ICONEXCLAMATION);
			if (nChoice == IDNO)
				return FALSE;
	
			// Make sure these all get unconfigured as well.  If they are not configured, then
			// nothing bad will happen because the config calls are idempotent.
			configFS = CS_UNCONFIGURE;
			configBak = CS_UNCONFIGURE;
			configSCS = CS_UNCONFIGURE;
			configSCC = CS_UNCONFIGURE;
		}
	}

	// Get additional needed information from the user
	GET_ADMIN_INFO_OPTIONS eOptions;
	BOOL bDB = (ShouldConfig(configDB) || ShouldUnconfig(configDB));

	// Use this as our default
	eOptions = GAIO_LOGIN_ONLY;

	// If we already have a sys control machine, then we don't need to ask for it
	if (ShouldConfig(configSCC)) {
		if (szScMachine[0] == 0) {
			ShowWarning(hDlg, IDS_MUST_ENTER_SCS_NAME);
			return FALSE;
		}
		lstrcpy(g_CfgData.szSysControlMachine, szScMachine);
	} else if (bDB && !g_CfgData.bLastDBServer) {
        // We need to know the name of the SCM machine.  Are we the SCM machine?
        if (bScsRunning)
            lstrcpy(g_CfgData.szSysControlMachine, g_CfgData.szHostname);
        else
		    eOptions = GAIO_GET_SCS;
    }

    // If doing a login only and the admin info is reusable
    if ((eOptions != GAIO_LOGIN_ONLY) || !g_CfgData.bReuseAdminInfo) {
    	if (!GetAdminInfo(hDlg, eOptions))
	    	return FALSE;

        // Use the admin info to get new handles
        if (!GetHandles(hDlg))
            return FALSE;
    }

    // Now that we are ready to configure, copy our local config info
    // into the structure that the config engine uses.
	g_CfgData.configFS = configFS;
    g_CfgData.configDB = configDB;
	g_CfgData.configBak = configBak;
	g_CfgData.configSCS = configSCS;
	g_CfgData.configSCC = configSCC;

	// Configure the server
	BOOL bConfigSucceeded = Configure(hDlg, bMustExit);
    if (bConfigSucceeded) {
        if (bMustExit) {
		    PostQuitMessage(0);
		    return TRUE;
	    }
        g_CfgData.bReuseAdminInfo = TRUE;
    } else
        g_CfgData.szAdminPW[0] = 0;

	// Get current config status
	BOOL bCanceled = FALSE;
	DWORD dwStatus = GetCurrentConfig(hDlg, bCanceled);
	if (dwStatus || bCanceled) {
		if (!bCanceled)
			ErrorDialog(dwStatus, IDS_CONFIG_CHECK_FAILED);
	}

	// Show the initial services config
	ShowInitialConfig();
	ShowServiceStates();

	return TRUE;
}

static void CheckEnableApply()
{
	BOOL bEnable = FALSE;
	
	bEnable = (bFsRunning != bFsOn) || (bDbRunning != bDbOn) || (bBakRunning != bBakOn) ||
				(bScsRunning != bScsOn) || (bSccRunning != bSccOn);

	if (bEnable)
		PropSheet_Changed(GetParent(hDlg), hDlg);
	else
		PropSheet_UnChanged(GetParent(hDlg), hDlg);
}


