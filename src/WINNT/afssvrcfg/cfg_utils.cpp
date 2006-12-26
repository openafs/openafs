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

#include "afscfg.h"
#include "resource.h"
#include "cfg_utils.h"


/*
 * EXPORTED FUNCTIONS _________________________________________________________
 *
 */
BOOL IsStepEnabled(const CONFIG_STATE& step)
{
    return ((step & CS_DISABLED) == 0);
}

void EnableStep(CONFIG_STATE& step, BOOL bEnable)
{	
    if (bEnable)
	step &= ~CS_DISABLED;
    else
	step |= CS_DISABLED;		
}	

BOOL ShouldConfig(const CONFIG_STATE& state)		
{
    return (state == CS_CONFIGURE);
}

BOOL DontConfig(const CONFIG_STATE& state)
{
    return (state == CS_DONT_CONFIGURE);
}

BOOL ShouldUnconfig(const CONFIG_STATE& state)
{
    return (state == CS_UNCONFIGURE);
}

BOOL ConfiguredOrConfiguring(const CONFIG_STATE& step)
{
    return (step == CS_CONFIGURE) || (step == CS_ALREADY_CONFIGURED);
}

BOOL Configured(const CONFIG_STATE& step)
{
    return (step == CS_ALREADY_CONFIGURED);
}

void ToggleConfig(CONFIG_STATE& state)
{
    if (ShouldConfig(state))
	state = CS_DONT_CONFIGURE;
    else if (DontConfig(state))
	state = CS_CONFIGURE;
}	

void RedrawGraphic()
{
    HWND hWiz = g_pWiz->GetWindow();

    HWND hBg = GetDlgItem(hWiz, IDC_WIZARD_LEFTPANE);

    RECT rect;
    GetClientRect(hBg, &rect);

    InvalidateRect(hBg, &rect, FALSE);
    UpdateWindow(hBg);
}	

UINT GetAppTitleID()
{
    if (g_CfgData.bWizard)
	return IDS_WIZARD_APP_TITLE;

    return IDS_CFG_TOOL_APP_TITLE;
}

const char *GetAdminLibErrorCodeMessage(afs_status_t nErrorCode)
{
    afs_status_t nStatus;
    const char *pszMsg = 0;

    int nResult = util_AdminErrorCodeTranslate(nErrorCode, 0, &pszMsg, &nStatus);
    if (!nResult)
        return 0;

    return pszMsg;
}

void LogError(afs_status_t nErrorCode)
{
    const char *pszMsg = GetAdminLibErrorCodeMessage(nErrorCode);
    
    if (pszMsg)
        g_LogFile.Write("Error 0x%0x has occurred: %s.\r\n", (UINT)nErrorCode, pszMsg);
    else
        g_LogFile.Write("Error 0x%0x has occurred.\r\n", (UINT)nErrorCode);
}

void ShowError(HWND hDlg, afs_status_t nErrorCode, UINT uiErrorMsgID)
{
    TCHAR szMsg[cchRESOURCE];

    LogError(nErrorCode);

    GetString(szMsg, uiErrorMsgID);

    ErrorDialog(nErrorCode, szMsg);

    SetWndText(hDlg, IDC_STATUS_MSG, IDS_CONFIG_FAILED);
}

int ShowWarning(HWND hDlg, UINT uiMsgID)
{
    TCHAR szMsg[cchRESOURCE];
    TCHAR szTitle[cchRESOURCE];

    GetString(szTitle, GetAppTitleID());
    GetString(szMsg, uiMsgID);

    return MessageBox(hDlg, szMsg, szTitle, MB_OKCANCEL | MB_ICONWARNING);
}

