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

#include "afscfg.h"		// Main header for this application
#include "resource.h"
#include "salvage_results_dlg.h"
extern "C" {
#include "afs_bosAdmin.h"
}


/*
 * DEFINITIONS _________________________________________________________________
 *
 */
static const int     UPDATE_INTERVAL        = 5000;             // milleseconds
static const int     LOG_BUF_SIZE           = 25000;            // bytes
static const int     EDIT_CONTROL_MAX_CHARS = 64000;            // Max chars an edit control can hold
static const char   *SALVAGE_LOG_FILE_NAME  = "SalvageLog.txt";

static rwWindowData arwDialog[] = {
	{ IDC_LOG,	raSizeX | raSizeY,	MAKELONG(350, 150), 0 },
	{ IDC_CLOSE,	raMoveX | raMoveY,	0,	0 },
	{ 9,		raMoveX | raMoveY,	0,	0 },
	{ idENDLIST,	0,			0,	0 }
};


/*
 * Variables _________________________________________________________________
 *
 */
static HWND hDlg = 0;						// HWND for this page's dialog
static HWND hLogo;
static afs_status_t nStatus;
static void *hServer;
static int nResult; 
static BOOL bSalvageComplete;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg);
static void OnClose();
static void SetMessages(int nStatusMsgID, int nLogMsgID);
static DWORD WINAPI ShowResults(LPVOID param);
static void SaveLogToDisk(char *pszLogBuf, char *pszFileName);
static char *AddCarriageReturnsToLog(char *pszInBuf, char *&pszOutBuf);
static char *GetMaxPartOfLogWeCanShow(char *pszLogBuf);
static BOOL AllocMemory(char *&pszLogBuf, int nBufSize);

BOOL CALLBACK SalvageResultsDlgProc(HWND hRHS, UINT msg, WPARAM wp, LPARAM lp);


/*
 * EXPORTED FUNCTIONS _________________________________________________________
 *
 */
BOOL ShowSalvageResults(HWND hParent)
{	
    int nResult = ModalDialog(IDD_SALVAGE_RESULTS, hParent, (DLGPROC)SalvageResultsDlgProc);

    return (nResult == IDOK);
}


/*
 * Dialog Proc _________________________________________________________________
 *
 */
BOOL CALLBACK SalvageResultsDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    if (AfsAppLib_HandleHelp(IDD_SALVAGE_RESULTS, hwndDlg, msg, wp, lp))
	return TRUE;

    switch (msg) {
    case WM_INITDIALOG:
	OnInitDialog(hwndDlg);
	break;

    case WM_COMMAND:
	switch (LOWORD(wp)) {
	case IDC_CLOSE:
	    OnClose();
	    break;

	case IDCANCEL:
	    if (bSalvageComplete)
		OnClose();
	}
	break;

    case WM_SIZE:	
	if (lp != 0)
	    ResizeWindow(hwndDlg, arwDialog, rwaFixupGuts);
	break;
    }

    return FALSE;
}	


/*
 * STATIC FUNCTIONS _________________________________________________________________
 *
 */

/*
 * Event Handler Functions __________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg)
{
    hDlg = hwndDlg;

    bSalvageComplete = FALSE;

    ResizeWindow(hDlg, arwDialog, rwaFixupGuts);

    hLogo = GetDlgItem(hDlg, IDC_LOGO);

    AfsAppLib_StartAnimation(hLogo, 8);

    SetMessages(IDS_SALVAGING, IDS_CURRENT_SALVAGE_LOG);

    nResult = bos_ServerOpen(g_hCell, GetHostnameA(), &hServer, &nStatus);
    if (!nResult) {
	ShowError(hDlg, nStatus, IDS_BOS_OPEN_FAILED);
	return;
    }

    // Remove the start menu - we do this so the user can't close
    // the dialog while salvage is being performed.
    DWORD dw;
    dw = GetWindowLong(hDlg, GWL_STYLE);
    dw &= ~WS_SYSMENU;
    SetWindowLong(hDlg, GWL_STYLE, dw);

    // Create a thread to keep the view of the log up to date
    DWORD dwThreadID;
    HANDLE hThread = CreateThread(0, 0, ShowResults, 0, 0, &dwThreadID);
    CloseHandle(hThread);
}

static void OnClose()
{
    bos_ServerClose(hServer, &nStatus);

    EndDialog(hDlg, IDOK);
}


/*
 * Utility Functions _________________________________________________________________
 *
 */
static void SetMessages(int nStatusMsgID, int nLogMsgID)
{
    TCHAR szMsgBuf[cchRESOURCE];

    SetDlgItemText(hDlg, IDC_SALVAGE_STATUS, GetResString(nStatusMsgID, szMsgBuf));
    SetDlgItemText(hDlg, IDC_LOG_TITLE, GetResString(nLogMsgID, szMsgBuf));
}

static char *AddCarriageReturnsToLog(char *pszInBuf, char *& pszOutBuf)
{
    ASSERT(pszInBuf);

    char *pInBuf = pszInBuf;

    // How man new lines are there?
    int nNumNLs = 0;

    while (*pInBuf) {
        if (*pInBuf == '\n')
            nNumNLs++;
	pInBuf++;
    }

    // Allocate enough memory for the log buffer plus CRs plus a NULL
    if (!AllocMemory(pszOutBuf, strlen(pszInBuf) + nNumNLs + 1))
        return 0;

    // Copy over the whole buffer, adding \r's as we go.  The edit control
    // seems to need \r before \n, so we set it up that way.
    pInBuf = pszInBuf;
    char *pOutBuf = pszOutBuf;

    while (*pInBuf) {
        if (*pInBuf == '\n') {
            *pOutBuf++ = '\r';
            *pOutBuf++ = '\n';
        } else
            *pOutBuf++ = *pInBuf;
		
	pInBuf++;
    }

    *pOutBuf = 0;

    return pszOutBuf;
}

static char *GetMaxPartOfLogWeCanShow(char *pszLogBuf)
{
    ASSERT(pszLogBuf);

    // Find out how much of the log buffer is in use
    int nLen = strlen(pszLogBuf);

    if (nLen < EDIT_CONTROL_MAX_CHARS)
        return pszLogBuf;

    // Buffer is bigger than maximum, so find the last full line
    // under the limit and return it.
    char *psz = pszLogBuf + nLen - EDIT_CONTROL_MAX_CHARS;
    
    // Make sure we return the beginning of a line
    while (*psz && (*psz != '\n'))
        psz++;

    if (*psz == '\n')
	psz++;

    return psz;
}    

static BOOL AllocMemory(char *&pszLogBuf, int nBufSize)
{
    if (pszLogBuf)
        delete pszLogBuf;

    pszLogBuf = new char[nBufSize];

    if (!pszLogBuf) {
        g_LogFile.Write("Failed to allocate memory (%d bytes) for the salvage log buffer.\r\n", nBufSize);
        return FALSE;
    }

    return TRUE;
}

static void SaveLogToDisk(char *pszLogBuf, char *pszFileName)
{
    // If no log file was specified by the user, then do nothing
    if (!pszFileName[0])
        return;

    FILE *fp = fopen(pszFileName, "w");
    if (!fp) {
	ShowError(hDlg, 0, IDS_ERROR_SAVING_SALVAGE_LOG_TO_DISK);
	return;
    }

    fprintf(fp, "%s", pszLogBuf);

    fclose(fp);
}

static DWORD WINAPI ShowResults(LPVOID param)
{
    ULONG nBufSize = LOG_BUF_SIZE;
    char *pszLogBuf = 0;
    char *pszLogBufWithCRs = 0;
    DWORD dwWait = WAIT_TIMEOUT;
    DWORD rc = 1;
    ULONG nLogSize = LOG_BUF_SIZE;

    if (!AllocMemory(pszLogBuf, nBufSize))
        return 0;
    
    while (dwWait != WAIT_OBJECT_0) {
        // Wait for either the salvage thread to terminate or our update
        // interval to pass.
        dwWait = WaitForSingleObject(g_CfgData.hSalvageThread, UPDATE_INTERVAL);

        // In either case, update the log display for the user

        // Get the salvage log as it currently exists
      getlog: 
	nResult = bos_LogGet(hServer, "SalvageLog", &nLogSize, pszLogBuf, &nStatus);
        if (!nResult) {
            if (nStatus == ADMMOREDATA) {
                // If salvage isn't done, then get a bigger buffer than we need to
                // prevent future ADMMOREDATA errors.
                nBufSize = nLogSize;
                if (dwWait != WAIT_OBJECT_0) {
                    nBufSize += LOG_BUF_SIZE;
                    nLogSize = nBufSize;
                }
                if (AllocMemory(pszLogBuf, nBufSize))
                    goto getlog;
            }

            rc = 0;
            break;            
        }

        // NULL-terminate the log
        pszLogBuf[nLogSize] = 0;

        // The edit control that we use to show the log to the user needs \r\n at the end
        // of each line, but the salvage log only has \n.  We must replace the \n with
        // \r\n before showing the log.
        if (!AddCarriageReturnsToLog(pszLogBuf, pszLogBufWithCRs)) {
            rc = 0;
            break;
        }
        
        // The edit control has a limit to the number of characters it can hold.  If the log
        // is bigger than that limit, then we will show the maximum portion of the end of the
        // log that we can fit in the edit control.
        char *pszLogToShow = GetMaxPartOfLogWeCanShow(pszLogBufWithCRs);

        // Put it into the edit control so the user can see it
        SetDlgItemText(hDlg, IDC_LOG, A2S(pszLogToShow));
    }

    AfsAppLib_StopAnimation(hLogo);

    if (rc) {
        SetMessages(IDS_SALVAGE_COMPLETE, IDS_FINAL_SALVAGE_LOG);
	SaveLogToDisk(pszLogBufWithCRs, GetSalvageLogFileNameA());
        MsgBox(hDlg, IDS_SALVAGE_COMPLETE, GetAppTitleID(), MB_OK);
    } else {
        SetMessages(IDS_SALVAGING, IDS_CANT_GET_SALVAGE_LOG);
        ShowError(hDlg, nStatus, IDS_CANT_GET_SALVAGE_LOG);
    }

    CloseHandle(g_CfgData.hSalvageThread);

    if (pszLogBuf)
        delete pszLogBuf;

    if (pszLogBufWithCRs)
        delete pszLogBufWithCRs;

    SetEnable(hDlg, IDC_CLOSE);

    bSalvageComplete = TRUE;

    return rc;
}

