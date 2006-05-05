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


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static BOOL OnInitDialog(HWND hwndDlg);


/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */

/*
 * Dialog Procs _________________________________________________________________
 *
 */
BOOL CALLBACK IntroPageDlgProc(HWND hRHS, UINT msg, WPARAM wp, LPARAM lp)
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
				g_pWiz->SetState(sidSTEP_TWO);
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
static BOOL OnInitDialog(HWND hwndDlg)
{
	hDlg = hwndDlg;

	g_pWiz->EnableButtons(NEXT_BUTTON);

	return TRUE;
}

