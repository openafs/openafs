/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}


/*
 * INCLUDES ___________________________________________________________________
 *
 */
#include "afscfg.h"		// Main header for this application
#include "resource.h"
#include "admin_info_dlg.h"


/*
 * DEFINITIONS _________________________________________________________________
 *
 */
static HWND hDlg = 0;						// HWND for this page's dialog
static GET_ADMIN_INFO_OPTIONS eOptions;		// Are we asking user for another server?


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg);
static void CheckEnableButtons();
static void SaveDlgInfo();
static void ShowPageInfo();

BOOL CALLBACK AdminInfoDlgProc(HWND hRHS, UINT msg, WPARAM wp, LPARAM lp);


/*
 * EXPORTED FUNCTIONS _________________________________________________________
 *
 */
BOOL GetAdminInfo(HWND hParent, GET_ADMIN_INFO_OPTIONS options)
{	
	eOptions = options;

	int nResult = ModalDialog(IDD_ADMIN_INFO, hParent, (DLGPROC)AdminInfoDlgProc);

	return (nResult == IDOK);
}


/*
 * Dialog Proc _________________________________________________________________
 *
 */
BOOL CALLBACK AdminInfoDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	if (AfsAppLib_HandleHelp(IDD_ADMIN_INFO, hwndDlg, msg, wp, lp))
		return TRUE;

	switch (msg) {
		case WM_INITDIALOG:
			OnInitDialog(hwndDlg);
			break;

		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDC_ADMIN_NAME:
				case IDC_ADMIN_PW:
				case IDC_HOSTNAME:
				case IDC_SCS:
					if (HIWORD(wp) == EN_CHANGE)
						CheckEnableButtons();
					break;

				case IDCANCEL:
					EndDialog(hDlg, IDCANCEL);
					break;
					

				case IDOK:
					SaveDlgInfo();
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
    static int nOffset = 0;

	hDlg = hwndDlg;
	
	// Hide the additional server stuff if we don't need it
	if (eOptions == GAIO_LOGIN_ONLY) {
		HideAndDisable(hDlg, IDC_HOSTNAME_FRAME);
		HideAndDisable(hDlg, IDC_HOSTNAME_PROMPT);
		HideAndDisable(hDlg, IDC_HOSTNAME_LABEL);
		HideAndDisable(hDlg, IDC_HOSTNAME);

        if (nOffset == 0) {
            // Get dimensions of the frame containing the things we will hide or show
            RECT rectFrame;
            GetWindowRect(GetDlgItem(hDlg, IDC_HOSTNAME_FRAME), &rectFrame);

            // Get original position of the buttons
            RECT rectButton;
            GetWindowRect(GetDlgItem(hDlg, IDCANCEL), &rectButton);

            // Figure out how far the buttons will have to move to be at the top
            // of the frame.
            nOffset = rectButton.top - rectFrame.top;
        }

        // Move the buttons		
		MoveWnd(hDlg, IDOK, 0, -nOffset);
		MoveWnd(hDlg, IDCANCEL, 0, -nOffset);
		MoveWnd(hDlg, IDHELP, 0, -nOffset);

        // Resize the dialog
        RECT rectDlg;
        GetWindowRect(hDlg, &rectDlg);
		MoveWindow(hDlg, rectDlg.left, rectDlg.top, rectDlg.right - rectDlg.left + 1, rectDlg.bottom - rectDlg.top - nOffset, TRUE);

	}

	ShowPageInfo();
}

/*
 * Utility Functions _________________________________________________________________
 *
 */
static void CheckEnableButtons()
{
	BOOL bDisable = FALSE;

	TCHAR szDummy[cchRESOURCE];

	bDisable |= lstrlen(GetWndText(hDlg, IDC_ADMIN_NAME, szDummy)) == 0;
	bDisable |= lstrlen(GetWndText(hDlg, IDC_ADMIN_PW, szDummy)) == 0;

	SetEnable(hDlg, IDOK, (ENABLE_STATE)!bDisable);
}

static void SaveDlgInfo()
{
	TCHAR szText[cchRESOURCE];
	
	lstrncpy(g_CfgData.szAdminName, GetWndText(hDlg, IDC_ADMIN_NAME, szText), MAX_ADMIN_NAME_LEN);
	lstrncpy(g_CfgData.szAdminPW, GetWndText(hDlg, IDC_ADMIN_PW, szText), MAX_ADMIN_PW_LEN);

	if (eOptions == GAIO_GET_SCS)
		lstrncpy(g_CfgData.szSysControlMachine, GetWndText(hDlg, IDC_HOSTNAME, szText), MAX_MACHINE_NAME_LEN);
}

static void ShowPageInfo()
{
	SetWndText(hDlg, IDC_ADMIN_NAME, g_CfgData.szAdminName);
	SetWndText(hDlg, IDC_ADMIN_PW, g_CfgData.szAdminPW);

	if (eOptions == GAIO_GET_SCS)
		SetWndText(hDlg, IDC_HOSTNAME, g_CfgData.szSysControlMachine);
}

