/*
 * Copyright (C) 1998  Transarc Corporation.
 * All rights reserved.
 *
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

