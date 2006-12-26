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
static void ShowStatusMsg(UINT nMsgID);


/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */

/*
 * Dialog Proc _________________________________________________________________
 *
 */
BOOL CALLBACK ReplicationPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp)
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
	    g_pWiz->SetState(sidSTEP_ELEVEN);
	    break;

	case IDBACK:
	    g_pWiz->SetState(sidSTEP_NINE);
	    break;

	case IDC_DONT_REPLICATE:
	    g_CfgData.configRep = CS_DONT_CONFIGURE;
	    break;

	case IDC_REPLICATE:
	    g_CfgData.configRep = CS_CONFIGURE;
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
	ShowStatusMsg(IDS_MUST_REPLICATE);
	g_CfgData.configRep = CS_CONFIGURE;
	return;
    }

    if (g_CfgData.configRep == CS_ALREADY_CONFIGURED) {
	ShowStatusMsg(IDS_ALREADY_REPLICATED);
        return;
    }

    // If the replication of the root volumes could not be determined, we'll
    // ask the user if they want to create them if they don't already exist.
    if (!g_CfgData.bRootVolumesReplicationKnown) {
	SetWndText(hDlg, IDC_REPLICATE_QUESTION, IDS_REP_ROOT_VOLUMES_IF_NECESSARY_PROMPT);
        g_CfgData.configRep = CS_CONFIGURE;
	SetCheck(hDlg, IDC_REPLICATE);
        return;
    }

    // Should this step be disabled?  Yes, if this machine does
    // not have a root.afs volume.
    if (!ConfiguredOrConfiguring(g_CfgData.configRootVolumes)) {
	ShowStatusMsg(IDS_ROOT_AFS_DOESNT_EXIST);
	EnableStep(g_CfgData.configRep, FALSE);
	return;
    }

    // Must do this in case it was disabled on the last run through
    EnableStep(g_CfgData.configRep);

    if (g_CfgData.configRep == CS_DONT_CONFIGURE)
	SetCheck(hDlg, IDC_DONT_REPLICATE);
    else if (g_CfgData.configRep == CS_CONFIGURE)
	SetCheck(hDlg, IDC_REPLICATE);
}	


/*
 * Utility Functions _________________________________________________________________
 *
 */
static void ShowStatusMsg(UINT nMsgID)
{
    TCHAR szMsg[cchRESOURCE];

    GetString(szMsg, nMsgID);

    ShowWnd(hDlg, IDC_REPLICATE_QUESTION, FALSE);
    ShowWnd(hDlg, IDC_REPLICATE, FALSE);
    ShowWnd(hDlg, IDC_DONT_REPLICATE, FALSE);

    ShowWnd(hDlg, IDC_CANT_REPLICATE_MSG);
    SetWndText(hDlg, IDC_CANT_REPLICATE_MSG, szMsg);
}

