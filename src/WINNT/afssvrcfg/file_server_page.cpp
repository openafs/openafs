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
static HWND hDlg = 0;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg);
static void ConfigMsg(UINT nMsgID);


/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */

/*
 * Dialog Proc _________________________________________________________________
 *
 */
BOOL CALLBACK FileServerPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp)
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
	    g_pWiz->SetState(sidSTEP_SIX);
	    break;

	case IDBACK:
	    g_pWiz->SetState(sidSTEP_FOUR);
	    break;

	case IDC_DONT_CONFIG_FILE_SERVER:
	    g_CfgData.configFS = CS_DONT_CONFIGURE;
	    break;

	case IDC_SHOULD_CONFIG_FILE_SERVER:
	    g_CfgData.configFS = CS_CONFIGURE;
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
	ConfigMsg(IDS_MUST_CONFIG_FS);
	g_CfgData.configFS = CS_CONFIGURE;
	return;
    }

    switch (g_CfgData.configFS) {
    case CS_ALREADY_CONFIGURED:
	ConfigMsg(IDS_ALREADY_A_FS_SERVER);
	break;

    case CS_DONT_CONFIGURE:
	SetCheck(hDlg, IDC_DONT_CONFIG_FILE_SERVER);
	break;

    case CS_CONFIGURE:
    default:
	SetCheck(hDlg, IDC_SHOULD_CONFIG_FILE_SERVER);
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
    ShowWnd(hDlg, IDC_CREATE_FS_QUESTION, FALSE);
    ShowWnd(hDlg, IDC_SHOULD_CONFIG_FILE_SERVER, FALSE);
    ShowWnd(hDlg, IDC_DONT_CONFIG_FILE_SERVER, FALSE);
	
    SetWndText(hDlg, IDC_MSG, szMsg);
    ShowWnd(hDlg, IDC_MSG);
}

