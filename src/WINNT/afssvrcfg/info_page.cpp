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
static HWND hDlg = 0;				// HWND for this page's dialog


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg);
static void CheckEnableButtons();
static BOOL SavePageInfo();
static void ShowPageInfo();
static void IsFirstServer(BOOL bIs = TRUE);


/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */

/*
 * Dialog Procs _________________________________________________________________
 *
 */
BOOL CALLBACK InfoPageDlgProc(HWND hRHS, UINT msg, WPARAM wp, LPARAM lp)
{
    if (WizStep_Common_DlgProc (hRHS, msg, wp, lp))
        return FALSE;

	switch (msg) {
		case WM_INITDIALOG:
			OnInitDialog(hRHS);
			break;

		case WM_COMMAND:
			switch (LOWORD(wp)) {
			case IDNEXT:
				if (SavePageInfo())
					g_pWiz->SetState (sidSTEP_THREE);
				break;

			case IDBACK:
				if (SavePageInfo())
					g_pWiz->SetState (sidSTEP_ONE);
				break;

			case IDC_FIRST_SERVER:
				IsFirstServer();
				break;

			case IDC_JOIN_EXISTING_CELL:
				IsFirstServer(FALSE);
				break;

			case IDC_CELL_NAME:
			case IDC_SERVER_PW:
			case IDC_VERIFY_PW:
				if (HIWORD(wp) == EN_CHANGE)
					CheckEnableButtons();
				break;

			}
		break;
    }

    return FALSE;
}

/*
 * STATIC FUNCTIONS ________________________________________________________________________
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

	ShowPageInfo();

	if (g_CfgData.bFirstServer)
		IsFirstServer();

	g_pWiz->SetDefaultControl(IDC_CELL_NAME);
}


/*
 * Utility Functions _________________________________________________________________
 *
 */
static void CheckEnableButtons()
{
	BOOL bDisable = FALSE;

	TCHAR szCellName[cchRESOURCE];
	TCHAR szPW[cchRESOURCE];
	TCHAR szVerifyPW[cchRESOURCE];

	bDisable = lstrlen(GetWndText(hDlg, IDC_CELL_NAME, szCellName)) == 0;

	GetWndText(hDlg, IDC_SERVER_PW, szPW);
	GetWndText(hDlg, IDC_VERIFY_PW, szVerifyPW);

	if (IsButtonChecked(hDlg, IDC_FIRST_SERVER))
		bDisable |= !lstrlen(szPW) || !lstrlen(szVerifyPW) || lstrcmp(szPW, szVerifyPW);

	if (bDisable)
		g_pWiz->EnableButtons(BACK_BUTTON);
	else
		g_pWiz->EnableButtons(BACK_BUTTON | NEXT_BUTTON);
}

static BOOL SavePageInfo()
{
	TCHAR szText[cchRESOURCE];
	
	GetWndText(hDlg, IDC_CELL_NAME, szText);
	if (lstrlen(szText) > MAX_CELL_NAME_LEN) {
		MsgBox(hDlg, IDS_CELL_NAME_LEN_ERROR, GetAppTitleID(), MB_ICONSTOP | MB_OK);
		return FALSE;
	}

	lstrcpy(g_CfgData.szCellName, szText);
	lstrncpy(g_CfgData.szServerPW, GetWndText(hDlg, IDC_SERVER_PW, szText), MAX_SERVER_PW_LEN);
	g_CfgData.bFirstServer = IsButtonChecked(hDlg, IDC_FIRST_SERVER);

	return TRUE;
}

static void ShowPageInfo()
{
	SetWndText(hDlg, IDC_CELL_NAME, g_CfgData.szCellName);
	SetWndText(hDlg, IDC_SERVER_PW, g_CfgData.szServerPW);
	SetWndText(hDlg, IDC_VERIFY_PW, g_CfgData.szServerPW);

	if (g_CfgData.bFirstServer)
		SetCheck(hDlg, IDC_FIRST_SERVER);
	else
		SetCheck(hDlg, IDC_JOIN_EXISTING_CELL);
}

static void IsFirstServer(BOOL bIs)
{
	ENABLE_STATE es;

	if (bIs)
		es = ES_ENABLE;
	else
		es = ES_DISABLE;

	SetEnable(hDlg, IDC_PRINCIPAL_LABEL, es);
	SetEnable(hDlg, IDC_PRINCIPAL, es);
	
	SetEnable(hDlg, IDC_SERVER_PW_FRAME, es);
	SetEnable(hDlg, IDC_SERVER_PW_LABEL, es);
	SetEnable(hDlg, IDC_SERVER_PW_PROMPT, es);
	SetEnable(hDlg, IDC_SERVER_PW, es);
	SetEnable(hDlg, IDC_VERIFY_PW_LABEL, es);
	SetEnable(hDlg, IDC_VERIFY_PW, es);
	
	CheckEnableButtons();
}

