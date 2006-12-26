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
int nOptionButtonSeparationHeight;  // This is used by sys_control_page.cpp


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg);
static void CantBackup(UINT nMsgID);
static void CalcOptionButtonSeparationHeight();


/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */

/*
 * Dialog Proc _________________________________________________________________
 *
 */
BOOL CALLBACK BackupPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp)
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
	    g_pWiz->SetState(sidSTEP_EIGHT);
	    break;

	case IDBACK:
	    g_pWiz->SetState(sidSTEP_SIX);
	    break;

	case IDC_DONT_CONFIG_BACKUP_SERVER:
	    g_CfgData.configBak = CS_DONT_CONFIGURE;
	    break;

	case IDC_CONFIG_BACKUP_SERVER:
	    g_CfgData.configBak = CS_CONFIGURE;
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

    CalcOptionButtonSeparationHeight();

    g_pWiz->EnableButtons(BACK_BUTTON | NEXT_BUTTON);
    g_pWiz->SetButtonText(IDNEXT, IDS_NEXT);
    g_pWiz->SetDefaultControl(IDNEXT);

    if (g_CfgData.configBak == CS_ALREADY_CONFIGURED) {
	CantBackup(IDS_ALREADY_A_BACKUP_SERVER);
	return;
    }

    // Should this step be disabled?  Yes, if this machine
    // is not configured as a database server.
    if (!ConfiguredOrConfiguring(g_CfgData.configDB)) {
	CantBackup(IDS_NOT_A_DB_SERVER);
	EnableStep(g_CfgData.configBak, FALSE);
	return;
    }

    // Enable this in case it was disabled the last time
    EnableStep(g_CfgData.configBak);

    if (g_CfgData.configBak == CS_DONT_CONFIGURE)
	SetCheck(hDlg, IDC_DONT_CONFIG_BACKUP_SERVER);
    else if (g_CfgData.configBak == CS_CONFIGURE)
	SetCheck(hDlg, IDC_CONFIG_BACKUP_SERVER);
}	


/*
 * Utility Functions _________________________________________________________________
 *
 */
static void CantBackup(UINT nMsgID)
{
    TCHAR szMsg[cchRESOURCE];

    GetString(szMsg, nMsgID);

    ShowWnd(hDlg, IDC_BACKUP_SERVER_QUESTION, FALSE);
    ShowWnd(hDlg, IDC_CONFIG_BACKUP_SERVER, FALSE);
    ShowWnd(hDlg, IDC_DONT_CONFIG_BACKUP_SERVER, FALSE);

    ShowWnd(hDlg, IDC_CANT_BACKUP_MSG);
    SetWndText(hDlg, IDC_CANT_BACKUP_MSG, szMsg);
}

static void CalcOptionButtonSeparationHeight()
{
    RECT rectOB1, rectOB2;

    GetWindowRect(GetDlgItem(hDlg, IDC_CONFIG_BACKUP_SERVER), &rectOB1);
    GetWindowRect(GetDlgItem(hDlg, IDC_DONT_CONFIG_BACKUP_SERVER), &rectOB2);

    nOptionButtonSeparationHeight = rectOB2.top - rectOB1.top;
}

