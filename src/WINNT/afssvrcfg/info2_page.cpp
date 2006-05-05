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


/*
 * DEFINITIONS _________________________________________________________________
 *
 */
static HWND hDlg = 0;				// HWND for this page's dialog

#define	FIRST_SERVER_STEP		sidSTEP_THREE
#define	NOT_FIRST_SERVER_STEP	sidSTEP_FOUR

#define MIN_AFS_UID		1
#define MAX_AFS_UID		UD_MAXVAL


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg);
static void CheckEnableButtons();
static void SavePageInfo();
static void ShowPageInfo();
static void UseNextUid(BOOL bUseNext);


/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */

/*
 * Dialog Procs _________________________________________________________________
 *
 */
BOOL CALLBACK InfoPage2DlgProc(HWND hRHS, UINT msg, WPARAM wp, LPARAM lp)
{
    if (WizStep_Common_DlgProc (hRHS, msg, wp, lp))
        return FALSE;

	switch (msg) {
		case WM_INITDIALOG:
			OnInitDialog(hRHS);
			break;

		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDC_WIZARD:
					if (HIWORD(wp) == wcIS_STATE_DISABLED) {
						int nState = g_pWiz->GetState();
						
						// Disable step 3 if we are not the first server
						if (nState == sidSTEP_THREE)
							return !g_CfgData.bFirstServer;

						// Disable step 4 if we are the first server
						if (nState == sidSTEP_FOUR)
							return g_CfgData.bFirstServer;
					}
					break;

				case IDNEXT:
					SavePageInfo();
					g_pWiz->SetState (sidSTEP_FIVE);
					break;

				case IDBACK:
					SavePageInfo();
					g_pWiz->SetState (sidSTEP_TWO);
					break;

				case IDC_USE_NEXT_UID:
					UseNextUid(TRUE);
					CheckEnableButtons();
					break;

				case IDC_USE_THIS_UID:
					UseNextUid(FALSE);
					CheckEnableButtons();
					break;

				case IDC_ADMIN_NAME:
				case IDC_ADMIN_PW:
				case IDC_VERIFY_ADMIN_PW:
				case IDC_HOSTNAME:
					if (HIWORD(wp) == EN_CHANGE)
						CheckEnableButtons();
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

	g_pWiz->EnableButtons(BACK_BUTTON);

	SetUpDownRange(hDlg, IDC_AFS_UID_SPINNER, MIN_AFS_UID, MAX_AFS_UID);

	ShowPageInfo();

	g_pWiz->SetDefaultControl(IDC_ADMIN_NAME);
}


/*
 * Utility Functions _________________________________________________________________
 *
 */
static void CheckEnableButtons()
{
	BOOL bDisable = FALSE;

	TCHAR szDummy[cchRESOURCE];
	TCHAR szPW[cchRESOURCE];
	TCHAR szVerifyPW[cchRESOURCE];

	bDisable |= lstrlen(GetWndText(hDlg, IDC_ADMIN_NAME, szDummy)) == 0;

	bDisable |= lstrlen(GetWndText(hDlg, IDC_ADMIN_PW, szPW)) == 0;

	if (IsWindowEnabled(GetDlgItem(hDlg, IDC_VERIFY_ADMIN_PW))) {
		GetWndText(hDlg, IDC_VERIFY_ADMIN_PW, szVerifyPW);
		bDisable |= !lstrlen(szVerifyPW) || lstrcmp(szPW, szVerifyPW);
	}

	if (IsWindowEnabled(GetDlgItem(hDlg, IDC_ADMIN_UID)))
		bDisable |= lstrlen(GetWndText(hDlg, IDC_ADMIN_UID, szDummy)) == 0;

	if (IsWindowEnabled(GetDlgItem(hDlg, IDC_HOSTNAME)))
		bDisable |= lstrlen(GetWndText(hDlg, IDC_HOSTNAME, szDummy)) == 0;

	if (bDisable)
		g_pWiz->EnableButtons(BACK_BUTTON);
	else
		g_pWiz->EnableButtons(BACK_BUTTON | NEXT_BUTTON);
}

static void SavePageInfo()
{
	TCHAR szText[cchRESOURCE];
	
	lstrncpy(g_CfgData.szAdminName, GetWndText(hDlg, IDC_ADMIN_NAME, szText), MAX_ADMIN_NAME_LEN);
	lstrncpy(g_CfgData.szAdminPW, GetWndText(hDlg, IDC_ADMIN_PW, szText), MAX_ADMIN_PW_LEN);

	if (g_pWiz->GetState() == FIRST_SERVER_STEP) {
		g_CfgData.bUseNextUid = IsButtonChecked(hDlg, IDC_USE_NEXT_UID);
		if (!g_CfgData.bUseNextUid)
			lstrncpy(g_CfgData.szAdminUID, GetWndText(hDlg, IDC_ADMIN_UID, szText), MAX_UID_LEN);
	}

	if (g_pWiz->GetState() == NOT_FIRST_SERVER_STEP)
		lstrncpy(g_CfgData.szCellServDbHostname, GetWndText(hDlg, IDC_HOSTNAME, szText), MAX_MACHINE_NAME_LEN);
}

static void ShowPageInfo()
{
	SetWndText(hDlg, IDC_ADMIN_NAME, g_CfgData.szAdminName);
	SetWndText(hDlg, IDC_ADMIN_PW, g_CfgData.szAdminPW);
	SetWndText(hDlg, IDC_VERIFY_ADMIN_PW, g_CfgData.szAdminPW);
	SetWndText(hDlg, IDC_HOSTNAME, g_CfgData.szCellServDbHostname);

	if (g_pWiz->GetState() == FIRST_SERVER_STEP) {
		if (g_CfgData.bUseNextUid)
			SetCheck(hDlg, IDC_USE_NEXT_UID);
		else
			SetCheck(hDlg, IDC_USE_THIS_UID);
		UseNextUid(g_CfgData.bUseNextUid);
		SetWndText(hDlg, IDC_ADMIN_UID, g_CfgData.szAdminUID);
	}
}

static void UseNextUid(BOOL bUseNext)
{
	ENABLE_STATE es = bUseNext ? ES_DISABLE : ES_ENABLE;
	
	SetEnable(hDlg, IDC_ADMIN_UID, es);
	SetEnable(hDlg, IDC_AFS_UID_SPINNER, es);
}

