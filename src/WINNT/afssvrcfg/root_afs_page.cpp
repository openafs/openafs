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
static void ShowStatusMsg(UINT nMsgID);


/*
 * EXPORTED FUNCTIONS __________________________________________________________
 *
 */

/*
 * Dialog Proc _________________________________________________________________
 *
 */
BOOL CALLBACK RootAfsPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp)
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
					g_pWiz->SetState(sidSTEP_TEN);
					break;

				case IDBACK:
				   g_pWiz->SetState(sidSTEP_EIGHT);
				   break;

				case IDC_DONT_CREATE_ROOT_VOLUMES:
					g_CfgData.configRootVolumes = CS_DONT_CONFIGURE;
					break;

				case IDC_CREATE_ROOT_VOLUMES:
					g_CfgData.configRootVolumes = CS_CONFIGURE;
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
		ShowStatusMsg(IDS_MUST_CREATE_ROOT_AFS);
		g_CfgData.configRootVolumes = CS_CONFIGURE;
		return;
	}

	if (g_CfgData.configRootVolumes == CS_ALREADY_CONFIGURED) {
		ShowStatusMsg(IDS_ROOT_AFS_ALREADY_EXISTS);
        return;
	}

	// If the existence of the root volumes could not be determined, we'll
	// ask the user if they want to create them if they don't already exist.
	if (!g_CfgData.bRootVolumesExistanceKnown) {
    	SetWndText(hDlg, IDC_ROOT_AFS_QUESTION, IDS_CREATE_ROOT_VOLUMES_IF_NECESSARY_PROMPT);
		SetCheck(hDlg, IDC_CREATE_ROOT_VOLUMES);
        g_CfgData.configRootVolumes = CS_CONFIGURE;
        return;
	}

	// Should this step be disabled?  Yes, if this machine does
	// not have a partition to make root.afs on.
	if (!ConfiguredOrConfiguring(g_CfgData.configPartition)) {
		ShowStatusMsg(IDS_NO_PARTITION_EXISTS);
		EnableStep(g_CfgData.configRootVolumes, FALSE);
		return;
	}

    // If root.afs exists already but root.cell does not exist, then 
    // the wizard cannot make root.cell and must disable this option.  
    // However, since the root volumes don't both exist, we will leave 
    // this option enabled, and only disable the yes check box.
    // TODO:  We should handle this better in a future version where we can
    // add new messages.  The message catalog is frozen for this version
    // so we have to handle this case without adding new messages.
	if (g_CfgData.bRootAfsExists && !g_CfgData.bRootCellExists) {
        EnableWnd(hDlg, IDC_CREATE_ROOT_VOLUMES, FALSE);
		SetCheck(hDlg, IDC_DONT_CREATE_ROOT_VOLUMES);
        g_CfgData.configRootVolumes = CS_DONT_CONFIGURE;
        return;
	}

	// Must do this in case it was disabled on the last run through
	EnableStep(g_CfgData.configRootVolumes);

	if (g_CfgData.configRootVolumes == CS_DONT_CONFIGURE)
		SetCheck(hDlg, IDC_DONT_CREATE_ROOT_VOLUMES);
	else
		SetCheck(hDlg, IDC_CREATE_ROOT_VOLUMES);
}


/*
 * Utility Functions _________________________________________________________________
 *
 */
static void ShowStatusMsg(UINT nMsgID)
{
	TCHAR szMsg[cchRESOURCE];

	GetString(szMsg, nMsgID);

	// Hide the controls that are at the same position as the message
	ShowWnd(hDlg, IDC_ROOT_AFS_QUESTION, FALSE);
	ShowWnd(hDlg, IDC_CREATE_ROOT_VOLUMES, FALSE);
	ShowWnd(hDlg, IDC_DONT_CREATE_ROOT_VOLUMES, FALSE);
	
	SetWndText(hDlg, IDC_ROOT_AFS_MSG, szMsg);
	ShowWnd(hDlg, IDC_ROOT_AFS_MSG);
}

