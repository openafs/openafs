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


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg);
static void ConfigMsg(UINT nMsgID);
static void EnableSCM(ENABLE_STATE enable);
static void SavePageInfo();
static void ShowPageInfo();


/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */

/*
 * Dialog Proc _________________________________________________________________
 *
 */
BOOL CALLBACK DBServerPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp)
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
					SavePageInfo();
					g_pWiz->SetState(sidSTEP_SEVEN);
					break;

				case IDBACK:
					SavePageInfo();
					g_pWiz->SetState(sidSTEP_FIVE);
					break;

				case IDC_DONT_CONFIG_DB_SERVER:
					g_CfgData.configDB = CS_DONT_CONFIGURE;
					EnableSCM(ES_DISABLE);
					break;

				case IDC_CONFIG_DB_SERVER:
					g_CfgData.configDB = CS_CONFIGURE;
					EnableSCM(ES_ENABLE);
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

	if (g_CfgData.bFirstServer) {
		ConfigMsg(IDS_MUST_CONFIG_DB);
		g_CfgData.configDB = CS_CONFIGURE;
		return;
	}

	ShowPageInfo();

	switch (g_CfgData.configDB) {
		case CS_ALREADY_CONFIGURED:
			ConfigMsg(IDS_ALREADY_A_DB_SERVER);
			break;

		case CS_DONT_CONFIGURE:
			SetCheck(hDlg, IDC_DONT_CONFIG_DB_SERVER);
			EnableSCM(ES_DISABLE);
			break;

		case CS_CONFIGURE:
		default:
			SetCheck(hDlg, IDC_CONFIG_DB_SERVER);
			EnableSCM(ES_ENABLE);
			break;
	}
}


/*
 * Utility Functions _________________________________________________________________
 *
 */

static void ConfigMsg(UINT nMsgID)
{
	TCHAR szMsg[cchRESOURCE];

	GetString(szMsg, nMsgID);

	// Hide the controls that are at the same position as the message
	ShowWnd(hDlg, IDC_DB_SERVER_QUESTION, FALSE);
	ShowWnd(hDlg, IDC_CONFIG_DB_SERVER, FALSE);
	ShowWnd(hDlg, IDC_SCM_PROMPT, FALSE);
	ShowWnd(hDlg, IDC_SYS_CONTROL_MACHINE_LABEL, FALSE);
	ShowWnd(hDlg, IDC_SYS_CONTROL_MACHINE, FALSE);
	
	ShowWnd(hDlg, IDC_DONT_CONFIG_DB_SERVER, FALSE);
	
	SetWndText(hDlg, IDC_MSG, szMsg);
	ShowWnd(hDlg, IDC_MSG);
}

static void EnableSCM(ENABLE_STATE enable)
{
	SetEnable(hDlg, IDC_SCM_PROMPT, enable);
	SetEnable(hDlg, IDC_SYS_CONTROL_MACHINE_LABEL, enable);
	SetEnable(hDlg, IDC_SYS_CONTROL_MACHINE, enable);
}

static void ShowPageInfo()
{
	SetWndText(hDlg, IDC_SYS_CONTROL_MACHINE, g_CfgData.szSysControlMachine);
}

static void SavePageInfo()
{
	GetWndText(hDlg, IDC_SYS_CONTROL_MACHINE, g_CfgData.szSysControlMachine);
}

