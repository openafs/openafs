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


/*
 * DEFINITIONS _________________________________________________________________
 *
 */
static HWND hDlg = 0;
extern int nOptionButtonSeparationHeight;   // Comes from backup_server_page.cpp


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg);
static void CantConfig(UINT nMsgID);
static void EnableSysControlMachine(BOOL bEnable);
static void ShowSysControlMachine(TCHAR *pszSysControlMachine);
static void CheckEnableNextButton();


/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */

/*
 * Dialog Proc _________________________________________________________________
 *
 */
BOOL CALLBACK SysControlPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	if (WizStep_Common_DlgProc (hwndDlg, msg, wp, lp))
		return FALSE;

	switch (msg) {
		case WM_INITDIALOG:
	         OnInitDialog(hwndDlg);
		     break;

		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDNEXT:
					g_pWiz->SetState(sidSTEP_TWELVE);
					break;

				case IDBACK:
					g_pWiz->SetState(sidSTEP_TEN);
					break;

				case IDC_SYS_CONTROL_SERVER:
					g_CfgData.configSCS = CS_CONFIGURE;
					g_CfgData.configSCC = CS_DONT_CONFIGURE;
					EnableSysControlMachine(FALSE);
					break;

				case IDC_SYS_CONTROL_CLIENT:
					g_CfgData.configSCS = CS_DONT_CONFIGURE;
					g_CfgData.configSCC = CS_CONFIGURE;
					EnableSysControlMachine(TRUE);
					SetFocus(GetDlgItem(hDlg, IDC_SYS_CONTROL_MACHINE));
					break;

				case IDC_DONT_CONFIGURE:
					g_CfgData.configSCS = CS_DONT_CONFIGURE;
					g_CfgData.configSCC = CS_DONT_CONFIGURE;
					EnableSysControlMachine(FALSE);
					break;

				case IDC_SYS_CONTROL_MACHINE:
					if (HIWORD(wp) == EN_CHANGE)
						CheckEnableNextButton();
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

	g_pWiz->EnableButtons(BACK_BUTTON | NEXT_BUTTON);
	g_pWiz->SetButtonText(IDNEXT, IDS_NEXT);
	g_pWiz->SetDefaultControl(IDNEXT);

	if (g_CfgData.configSCS == CS_ALREADY_CONFIGURED) {
		CantConfig(IDS_ALREADY_A_SYS_CONTROL_SERVER);
		return;
	} else if (g_CfgData.configSCC == CS_ALREADY_CONFIGURED) {
		CantConfig(IDS_ALREADY_A_SYS_CONTROL_CLIENT);
		return;
	}

	// Should this step be disabled?  Yes, if this machine
	// is not configured as a database or file server.
	if (!ConfiguredOrConfiguring(g_CfgData.configFS) && !ConfiguredOrConfiguring(g_CfgData.configDB)) {
		CantConfig(IDS_SC_NOT_A_DB_OR_FS_SERVER);
		EnableStep(g_CfgData.configSCS, FALSE);
		EnableStep(g_CfgData.configSCC, FALSE);
		return;
	}

	// Do this in case they were disabled on the last run through
	EnableStep(g_CfgData.configSCS);
	EnableStep(g_CfgData.configSCC);

	// If this is the first server, then it can't be a SCC
	if (g_CfgData.bFirstServer) {
		// Disable the SCC step
		EnableStep(g_CfgData.configSCC, FALSE);
		
		// Hide the SCC controls
		ShowWnd(hDlg, IDC_SYS_CONTROL_CLIENT_DESC, FALSE);
		ShowWnd(hDlg, IDC_SYS_CONTROL_CLIENT, FALSE);
		ShowWnd(hDlg, IDC_SYS_CONTROL_MACHINE_LABEL, FALSE);
		ShowWnd(hDlg, IDC_SYS_CONTROL_MACHINE, FALSE);

		// Move remaining controls to fill the holes left from
		// hiding the SCC controls
        
        // Get position of the "Do not configure" option button; we will position
        // the other controls relative to this one
        RECT rectDNC;
        GetWindowRect(GetDlgItem(hDlg, IDC_DONT_CONFIGURE), &rectDNC);
   
        // Get position of the SCS option button
        RECT rectSCS;
        GetWindowRect(GetDlgItem(hDlg, IDC_SYS_CONTROL_SERVER), &rectSCS);

        // Calc offset between the two
        int nOffset = rectDNC.top - rectSCS.top;

        // Separate the two option controls
        nOffset -= nOptionButtonSeparationHeight;

        // Move the controls
		MoveWnd(hDlg, IDC_SYS_CONTROL_MACHINE_QUESTION, 0, nOffset);
		MoveWnd(hDlg, IDC_SYS_CONTROL_SERVER, 0, nOffset);

		SetWndText(hDlg, IDC_TITLE, IDS_CONFIG_SCS);
		SetWndText(hDlg, IDC_SYS_CONTROL_MACHINE_QUESTION, IDS_SYS_CONTROL_SERVER_ONLY_MSG);
		SetWndText(hDlg, IDC_DONT_CONFIGURE, IDS_DONT_CONFIG_SYS_CONTROL_SERVER_MSG);
	}

	if (g_CfgData.configSCS == CS_CONFIGURE)
		SetCheck(hDlg, IDC_SYS_CONTROL_SERVER);
	else if (g_CfgData.configSCC == CS_CONFIGURE) {
		SetCheck(hDlg, IDC_SYS_CONTROL_CLIENT);
		EnableSysControlMachine(TRUE);
	} else
		SetCheck(hDlg, IDC_DONT_CONFIGURE);
}

/*
 * Utility Functions _________________________________________________________________
 *
 */
static void CantConfig(UINT nMsgID)
{
	TCHAR szMsg[cchRESOURCE];

	GetString(szMsg, nMsgID);

	ShowWnd(hDlg, IDC_SYS_CONTROL_MACHINE_QUESTION, FALSE);
	ShowWnd(hDlg, IDC_SYS_CONTROL_SERVER, FALSE);
	ShowWnd(hDlg, IDC_SYS_CONTROL_CLIENT, FALSE);
	ShowWnd(hDlg, IDC_SYS_CONTROL_MACHINE_LABEL, FALSE);
	ShowWnd(hDlg, IDC_SYS_CONTROL_MACHINE, FALSE);
	ShowWnd(hDlg, IDC_DONT_CONFIGURE, FALSE);

	ShowWnd(hDlg, IDC_CANT_CONFIG_MSG);
	SetWndText(hDlg, IDC_CANT_CONFIG_MSG, szMsg);
}

static void EnableSysControlMachine(BOOL bEnable)
{
	EnableWnd(hDlg, IDC_SYS_CONTROL_MACHINE_LABEL, bEnable);
	EnableWnd(hDlg, IDC_SYS_CONTROL_MACHINE, bEnable);

	if (bEnable)
		ShowSysControlMachine(g_CfgData.szSysControlMachine);
	else
		ShowSysControlMachine(TEXT(""));

	CheckEnableNextButton();
}

static void ShowSysControlMachine(TCHAR *pszSysControlMachine)
{
	SetWndText(hDlg, IDC_SYS_CONTROL_MACHINE, pszSysControlMachine);
}

static void CheckEnableNextButton()
{
	BOOL bEnable = TRUE;

	if (IsButtonChecked(hDlg, IDC_SYS_CONTROL_CLIENT)) {
		GetWndText(hDlg, IDC_SYS_CONTROL_MACHINE, g_CfgData.szSysControlMachine, MAX_MACHINE_NAME_LEN);

		if (lstrlen(g_CfgData.szSysControlMachine) == 0)
			bEnable = FALSE;
	}

	if (bEnable)
		g_pWiz->EnableButtons(BACK_BUTTON | NEXT_BUTTON);
	else
		g_pWiz->EnableButtons(BACK_BUTTON);
}

