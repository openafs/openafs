// WinAfsLoad.h : main header file for the WINAFSLOAD application
//
/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#define AFSTITLE "AFS Control Panel"

#if !defined(AFX_WINAFSLOAD_H__75E145B3_F5C0_11D3_A374_00105A6BCA62__INCLUDED_)
#define AFX_WINAFSLOAD_H__75E145B3_F5C0_11D3_A374_00105A6BCA62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

#define LOG ((CWinAfsLoadApp *)AfxGetApp())->Log
//#define LOG1(p) ((CWinAfsLoadApp *)AfxGetApp())->Log(p)
//#define LOG2(p1,p2) ((CWinAfsLoadApp *)AfxGetApp())->Log(p1,p2)
//#define LOG3(p1,p2,p3) ((CWinAfsLoadApp *)AfxGetApp())->Log(p1,p2,p3)
#define SHOWLOG(p1) ((CWinAfsLoadApp *)AfxGetApp())->ShowLog(p1)
#define PROGRESS(p1,p2) ((CWinAfsLoadApp *)AfxGetApp())->Progress(p1,p2)
#define REQUESTUIPOSTEVENT ((CWinAfsLoadApp *)AfxGetApp())->RequestUIPostEvent
#define REQUESTUISUSPENDEVENT ((CWinAfsLoadApp *)AfxGetApp())->RequestUISuspendEvent

/////////////////////////////////////////////////////////////////////////////
// CWinAfsLoadApp:
// See WinAfsLoad.cpp for the implementation of this class
//
class CDatalog;
class CMyUIThread;

class CWinAfsLoadApp : public CWinApp
{
	friend class CProgBarDlg;
	friend class CMyUIThread;
	void WSANotifyFromUI(WPARAM, const char *);
	void NotifyFromUI(WPARAM, const char *);
public:
	CString m_sMsg;		//used to pass error messages to calling thread
	CMyUIThread* m_pCMyUIThread;
	BOOL m_bShowAfs,m_bLog,m_bLogWindow,m_bConnect;
	static CString m_sDosAppName;
	CWinAfsLoadApp();
	void Log(const char *);
	void Log(const char *,const char*);
	void Log(const char *,const DWORD);
	void Log(const char *,const DWORD,const DWORD);
	void Log(const char *,const char*,const char*);
	void Log(const char *f,const char*,const char*,const char*);
	void ShowLog(BOOL);
	void ShowPrint(BOOL);
	void Progress(VOID *wnd, UINT mode);
	void WaitForEvent(UINT sec,WPARAM *wp,CString *msg);
	void GetNotifyState(WPARAM *wp,LPARAM *lp);
	void RequestUISuspendEvent(UINT mode);
	void RequestUIPostEvent(UINT mode,WPARAM wp,LPARAM lp);
	void SetNotifyEventSuspend();
	void SetNotifyEventPostMessage(){m_uNotifyMessage=2;}
	void ClearNotify(){m_uNotifyMessage=0;}
	BOOL RegOptions(BOOL fetch);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWinAfsLoadApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

// Implementation
	BOOL static CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam);
	HWND static m_hAfsLoad,m_hAfsLoadFinish;
	CString m_sTargetDir;	//location of afsd.exe
	UINT m_uEvent;
	UINT m_uNotifyMessage;
	WPARAM m_wParam;
	BOOL m_bNoID;

	//{{AFX_MSG(CWinAfsLoadApp)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	afx_msg LRESULT OnNotifyReturn(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnAfsEvent(WPARAM wParam, LPARAM lParam);
};

#define CWINAFSLOADAPP ((CWinAfsLoadApp *)AfxGetApp())
/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WINAFSLOAD_H__75E145B3_F5C0_11D3_A374_00105A6BCA62__INCLUDED_)
