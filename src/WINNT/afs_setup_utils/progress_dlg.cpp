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
extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <windows.h>
#include <WINNT/talocale.h>
#include "resource.h"
#include "progress_dlg.h"
#include "animate_icon.h"


/*
 * DEFINITIONS _________________________________________________________________
 *
 */


/*
 * Variables _________________________________________________________________
 *
 */
static HWND hDlg = 0;						// HWND for this page's dialog
static char *pszProgressMsg = 0;
static HWND hLogo;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
BOOL CALLBACK ProgressDlgProc(HWND hRHS, UINT msg, WPARAM wp, LPARAM lp);
static DWORD WINAPI DisplayProgressDlg(LPVOID param);
static void OnInitDialog(HWND hwndDlg);
static void OnQuit();


/*
 * EXPORTED FUNCTIONS _________________________________________________________
 *
 */
BOOL ShowProgressDialog(char *pszMsg)
{
	DWORD dwThreadID;
	
    pszProgressMsg = pszMsg;

	// Create a thread to show the dialog
	HANDLE hThread = CreateThread(0, 0, DisplayProgressDlg, 0, 0, &dwThreadID);

	CloseHandle(hThread);

    return (hThread != 0);
}

void HideProgressDialog(void)
{
    PostMessage(hDlg, WM_QUIT, 0, 0);
}


/*
 * Dialog Proc _________________________________________________________________
 *
 */
static BOOL CALLBACK ProgressDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
		case WM_INITDIALOG:
            OnInitDialog(hwndDlg);
			hDlg = hwndDlg;
            SetWindowText(GetDlgItem(hDlg, IDC_MSG), pszProgressMsg);
			break;
    
        case WM_QUIT:
            OnQuit();            
            break;
    }

    return FALSE;
}


/*
 * Event Handler Functions __________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg)
{
    hDlg = hwndDlg;

    SetWindowText(GetDlgItem(hDlg, IDC_MSG), pszProgressMsg);

    hLogo = GetDlgItem(hDlg, IDC_LOGO);

    StartAnimation(hLogo, 8);
}

static void OnQuit()
{
    StopAnimation(hLogo);

    EndDialog(hDlg, IDOK);
}


/*
 * OTHER FUNCTIONS _________________________________________________________________
 *
 */
static DWORD WINAPI DisplayProgressDlg(LPVOID param)
{
   	ModalDialog (IDD_PROGRESS, 0, (DLGPROC)ProgressDlgProc);

	return 0;
}

