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
#include <WINNT\afsapplib.h>
#include <stdio.h>
#include "toolbox.h"


/*
 * EXPORTED FUNCTIONS _________________________________________________________
 *
 */
void SetEnable(HWND hDlg, UINT nControl, ENABLE_STATE eState)
{
    HWND hControl = GetDlgItem(hDlg, nControl);

    BOOL bEnable;

    switch (eState) {
    case ES_ENABLE:
	bEnable = TRUE;
	break;

    case ES_DISABLE:
	bEnable = FALSE;
	break;

    case ES_TOGGLE:
	bEnable = !IsWindowEnabled(hControl);
	break;
    }

    EnableWindow(hControl, bEnable);
}

void SetElapsedTime(HWND hwnd, DWORD nCtrlID, ULONG ulMin, ULONG ulMax, ULONG ulTime)
{
    SYSTEMTIME stMin;
    SET_ELAPSED_TIME_FROM_SECONDS (&stMin, ulMin);

    SYSTEMTIME stMax;
    SET_ELAPSED_TIME_FROM_SECONDS (&stMax, ulMax);

    SYSTEMTIME st;
    SET_ELAPSED_TIME_FROM_SECONDS (&st, ulTime);

    HWND hElapsed = ::GetDlgItem(hwnd, nCtrlID);
    EL_SetRange (hElapsed, &stMin, &stMax);
    EL_SetTime (hElapsed, &st);
}

void GetElapsedTime(HWND hwnd, DWORD nCtrlID, DWORD& dwTime)
{
    SYSTEMTIME stFinal;
	
    HWND hElapsed = ::GetDlgItem(hwnd, nCtrlID);
    EL_GetTime (hElapsed, &stFinal);
	
    dwTime = GET_SECONDS_FROM_ELAPSED_TIME(&stFinal);
}

LPCTSTR SecondsToElapsedTime(UINT uiNumSeconds)
{
    static TCHAR szTime[64], sz[32];

    *szTime = 0;
    *sz = 0;

    int nHours, nMinutes, nSeconds;

    nHours = uiNumSeconds / 3600;
    nMinutes = (uiNumSeconds % 3600) / 60;
    nSeconds = (uiNumSeconds % 3600) % 60;

    if (nHours)
	_stprintf(szTime, TEXT("%d hours"), nHours);

    if (nMinutes) {
	if (nHours)
	    lstrcat(szTime, TEXT(", "));
	_stprintf(sz, TEXT("%d minutes"), nMinutes); 
	lstrcat(szTime, sz);
    }

    if (nSeconds) {
	if (nHours || nMinutes)
	    lstrcat(szTime, TEXT(", "));
	_stprintf(sz, TEXT("%d seconds"), nSeconds);
	lstrcat(szTime, sz);
    }

    return szTime;
}

BOOL IsButtonChecked(HWND hDlg, UINT uiCtrlID)
{
    return SendMessage(GetDlgItem(hDlg, uiCtrlID), BM_GETCHECK, 0, 0) == BST_CHECKED;
}

int GetButtonState(HWND hDlg, UINT uiCtrlID)
{	
    return SendMessage(GetDlgItem(hDlg, uiCtrlID), BM_GETCHECK, 0, 0);
}

void SetCheck(HWND hDlg, UINT uiCtrlID, int nChecked)
{
    SendMessage(GetDlgItem(hDlg, uiCtrlID), BM_SETCHECK, nChecked, 0);
}

TCHAR *GetResString(UINT nMsgID, TCHAR *pszMsg, UINT nLen)
{
    GetString(pszMsg, nMsgID, nLen);

    return pszMsg;
}

void ShowWnd(HWND hDlg, UINT uiCtrlID, BOOL bShow)
{
    ShowWindow(GetDlgItem(hDlg, uiCtrlID), bShow ? SW_SHOW : SW_HIDE);
}

void EnableWnd(HWND hDlg, UINT uiCtrlID, BOOL bEnable)
{
    EnableWindow(GetDlgItem(hDlg, uiCtrlID), bEnable);
}

void SetWndText(HWND hDlg, UINT uiCtrlID, LPCTSTR pszMsg)
{
    SetWindowText(GetDlgItem(hDlg, uiCtrlID), pszMsg);
}

void SetWndText(HWND hDlg, UINT uiCtrlID, UINT nMsgID)
{
    TCHAR szMsg[cchRESOURCE];

    GetString(szMsg, nMsgID);
	
    SetWndText(hDlg, uiCtrlID, szMsg);
}

TCHAR *GetWndText(HWND hDlg, UINT uiCtrlID, TCHAR *pszTextBuffer, int nTextLen)
{
    GetWindowText(GetDlgItem(hDlg, uiCtrlID), pszTextBuffer, nTextLen);

    return pszTextBuffer;
}

void ForceUpdateWindow(HWND hWnd)
{
    InvalidateRect(hWnd, 0, TRUE);
    UpdateWindow(hWnd);
}

void ForceUpdateWindow(HWND hDlg, UINT uiCtrlID)
{
    ForceUpdateWindow(GetDlgItem(hDlg, uiCtrlID));
}

int AddLBString(HWND hDlg, UINT uiCtrlID, LPCTSTR pszString)
{
    return SendMessage(GetDlgItem(hDlg, uiCtrlID), LB_ADDSTRING, 0, (LONG)pszString);
}

int ClearListBox(HWND hDlg, UINT uiCtrlID)
{
    return SendMessage(GetDlgItem(hDlg, uiCtrlID), LB_RESETCONTENT, 0, 0);
}

void SetUpDownRange(HWND hDlg, UINT uiCtrlID, int nMinVal, int nMaxVal)
{
    SendMessage(GetDlgItem(hDlg, uiCtrlID), UDM_SETRANGE, 0, (LPARAM)MAKELONG((short)nMaxVal, (short)nMinVal));
}

void MakeBold(HWND hWnd)
{
    HFONT hFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);

    LOGFONT logFont;

    GetObject(hFont, sizeof(LOGFONT), &logFont);

    logFont.lfWeight = FW_BOLD;

    HFONT hNewFont = CreateFontIndirect(&logFont);
    ASSERT(hNewFont);

    SendMessage(hWnd, WM_SETFONT, (WPARAM)hNewFont, MAKELPARAM(TRUE, 0));
}

int MsgBox(HWND hParent, UINT uiTextID, UINT uiCaptionID, UINT uType)
{
    TCHAR szText[cchRESOURCE];
    TCHAR szCaption[cchRESOURCE];

    GetString(szText, uiTextID);
    GetString(szCaption, uiCaptionID);

    return MessageBox(hParent, szText, szCaption, uType);
}

void HideAndDisable(HWND hDlg, UINT uiCtrlID)
{
    ShowWnd(hDlg, uiCtrlID, SW_HIDE);
    SetEnable(hDlg, uiCtrlID, ES_DISABLE);
}

void ShowAndEnable(HWND hDlg, UINT uiCtrlID, BOOL bShowAndEnable)
{
    int nShow = SW_SHOW;
    ENABLE_STATE es = ES_ENABLE;

    if (!bShowAndEnable) {
	nShow = SW_HIDE;
	es = ES_DISABLE;
    }

    ShowWnd(hDlg, uiCtrlID, nShow);
    SetEnable(hDlg, uiCtrlID, es);
}

void MoveWnd(HWND hDlg, UINT nCtrlID, int xOffset, int yOffset)
{
    HWND hCtrl = GetDlgItem(hDlg, nCtrlID);
	
    RECT rect;
    GetWindowRect(hCtrl, &rect);
	
    POINT p1, p2;
    p1.x = rect.left;
    p1.y = rect.top;
    p2.x = rect.right;
    p2.y = rect.bottom;

    ScreenToClient(hDlg, &p1);
    ScreenToClient(hDlg, &p2);

    rect.left = p1.x;
    rect.top = p1.y;
    rect.right = p2.x;
    rect.bottom = p2.y;

    MoveWindow(hCtrl, rect.left + xOffset, rect.top + yOffset, rect.right - rect.left, rect.bottom - rect.top, TRUE);
}

