/*
 * Copyright (C) 1998  Transarc Corporation.
 * All rights reserved.
 *
 */


#ifndef	_TOOLBOX_H_
#define _TOOLBOX_H_

enum ENABLE_STATE { ES_DISABLE, ES_ENABLE, ES_TOGGLE };

void SetEnable(HWND hDlg, UINT nControl, ENABLE_STATE eState = ES_ENABLE);

void SetElapsedTime(HWND hwnd, DWORD nCtrlID, ULONG ulMin, ULONG ulMax, ULONG ulTime);
void GetElapsedTime(HWND hwnd, DWORD nCtrlID, DWORD& dwTime);
LPCTSTR SecondsToElapsedTime(UINT uiNumSeconds);

void ShowWnd(HWND hDlg, UINT uiCtrlID, BOOL bShow = TRUE);
void EnableWnd(HWND hDlg, UINT uiCtrlID, BOOL bEnable = TRUE);
void HideAndDisable(HWND hDlg, UINT uiCtrlID);
void ShowAndEnable(HWND hDlg, UINT uiCtrlID, BOOL bShowAndEnable = TRUE);
void MoveWnd(HWND hDlg, UINT nCtrlID, int xOffset, int yOffset);

void SetWndText(HWND hDlg, UINT uiCtrlID, LPCTSTR pszMsg);
void SetWndText(HWND hDlg, UINT uiCtrlID, UINT uiMsgID);
TCHAR *GetWndText(HWND hDlg, UINT uiCtrlID, TCHAR *pszTextBuffer, int nTextLen = cchRESOURCE);
inline int GetWndTextLength(HWND hDlg, UINT uiCtrlID)	{ return GetWindowTextLength(GetDlgItem(hDlg, uiCtrlID)); }

void ForceUpdateWindow(HWND hWnd);
void ForceUpdateWindow(HWND hDlg, UINT uiCtrlID);

void SetCheck(HWND hDlg, UINT uiCtrlID, BOOL bChecked = TRUE);
BOOL IsButtonChecked(HWND hDlg, UINT uiCtrlID);
int GetButtonState(HWND hDlg, UINT uiCtrlID);

int AddLBString(HWND hDlg, UINT uiCtrlID, LPCTSTR pszString);
int ClearListBox(HWND hDlg, UINT uiCtrlID);

TCHAR *GetResString(UINT nMsgID, TCHAR *pszMsg, UINT nLen = cchRESOURCE);

void ShowWnd(HWND hDlg, UINT uiCtrlID, BOOL bShow);

void SetUpDownRange(HWND hDlg, UINT uiCtrlID, int nMinVal, int nMaxVal);

void MakeBold(HWND hWnd);
inline void MakeBold(HWND hDlg, UINT uiCtrlID)		{ MakeBold(GetDlgItem(hDlg, uiCtrlID)); }

int MsgBox(HWND hParent, UINT uiTextID, UINT uiCaptionID, UINT uiType);
		   

#endif	// _TOOLBOX_H_

