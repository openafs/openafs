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
#include "get_pw_dlg.h"


/*
 * DEFINITIONS _________________________________________________________________
 *
 */
static HWND hDlg = 0;						// HWND for this page's dialog


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg);
static void CheckEnableButtons();
static void SaveDlgInfo();
static void ShowPageInfo();

BOOL CALLBACK GetPwDlgProc(HWND hRHS, UINT msg, WPARAM wp, LPARAM lp);


/*
 * EXPORTED FUNCTIONS _________________________________________________________
 *
 */
BOOL GetAfsPrincipalPassword(HWND hParent, TCHAR *&pszServerPW)
{	
    int nResult = ModalDialog(IDD_GET_PW, hParent, (DLGPROC)GetPwDlgProc);
    if (nResult == IDOK) {
    	pszServerPW = g_CfgData.szServerPW;
        return TRUE;
    }

    pszServerPW = 0;

    return FALSE;
}


/*
 * Dialog Proc _________________________________________________________________
 *
 */
BOOL CALLBACK GetPwDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    if (AfsAppLib_HandleHelp(IDD_GET_PW, hwndDlg, msg, wp, lp))
	return TRUE;

    switch (msg) {
    case WM_INITDIALOG:
	OnInitDialog(hwndDlg);
	break;

    case WM_COMMAND:
	switch (LOWORD(wp)) {
	case IDC_PW:
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
    hDlg = hwndDlg;
}


/*
 * Utility Functions _________________________________________________________________
 *
 */
static void CheckEnableButtons()
{
    BOOL bDisable = FALSE;

    TCHAR szDummy[cchRESOURCE];

    bDisable |= lstrlen(GetWndText(hDlg, IDC_PW, szDummy)) == 0;

    SetEnable(hDlg, IDOK, (ENABLE_STATE)!bDisable);
}

static void SaveDlgInfo()
{
    TCHAR szText[cchRESOURCE];
	
    lstrncpy(g_CfgData.szServerPW, GetWndText(hDlg, IDC_PW, szText), MAX_SERVER_PW_LEN);
}

static void ShowPageInfo()
{
    SetWndText(hDlg, IDC_PW, g_CfgData.szServerPW);
}

