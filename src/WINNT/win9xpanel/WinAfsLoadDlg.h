// WinAfsLoadDlg.h : header file
//
/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/

#if !defined(AFX_WINAFSLOADDLG_H__75E145B5_F5C0_11D3_A374_00105A6BCA62__INCLUDED_)
#define AFX_WINAFSLOADDLG_H__75E145B5_F5C0_11D3_A374_00105A6BCA62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "trayicon.h"
#include "datalog.h"
#include "cafs.h"
#include <afxtempl.h>
#include "share.h"
#include "transbmp.h"
/////////////////////////////////////////////////////////////////////////////
// CWinAfsLoadDlg dialog
class CSettings;
class CEncript;

class CWinAfsLoadDlg : public CDialog
{
// Construction
	friend CSettings;
public:
	CWinAfsLoadDlg(const char *user,const char *pass,CWnd* pParent = NULL);	// standard constructor
	void HandleError(const char *s,BOOL b=TRUE);
	void BuildDriveList(BOOL newone=FALSE);
	CList<CString,CString&> m_Drivelist;
	void OnShowAddMenus();
	LRESULT OnNotifyReturn(WPARAM wParam, LPARAM lParam);
	LRESULT OnAfsEvent(WPARAM wParam, LPARAM lParam);
	BOOL IsGetTokens(){return (!CWINAFSLOADAPP->m_bNoID) && (m_sUsername!="");}

// Dialog Data
	//{{AFX_DATA(CWinAfsLoadDlg)
	enum { IDD = IDD_WINAFSLOAD_DIALOG };
	CButton	m_Down;
	CButton	m_Enable;
	CButton	m_cWorld;
	CStatic	m_cAuthWarn;
	CButton	m_cAuthenicate;
	CButton	m_cSaveUsername;
	CListCtrl	m_cMountlist;
	CButton	m_cChange;
	CButton	m_cRemove;
	CButton	m_cOptionLine;
	CButton	m_cCheckAdvanceDisplay;
	CButton	m_cConnect;
	CButton	m_cCancel;
	CEdit	m_cPassword;
	CEdit	m_cUsername;
	CString	m_sPassword;
	CString	m_sUsername;
	CString	m_sMountDisplay;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWinAfsLoadDlg)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

// Implementation
protected:
	CString m_VersionString;
	CAfs m_cAfs;
	CTrayIcon	m_trayIcon;		// my tray icon
	HICON m_hIcon;
	BOOL m_bHomepath;
	BOOL m_bServiceIsActive;		//service is active
	PROCESS_INFORMATION m_procInfo;
	UINT m_DialogShrink;
	CRect m_OriginalRect;
	CString	m_sComputername;
	BOOL TerminateBackground(BOOL bDisplay=TRUE,CString *emsg=NULL);
	BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	BOOL RegPassword(CString &user,CString &pass,BOOL fetch);
	CImageList *m_pImagelist;
	BOOL ProfileData(BOOL put);
	void AddToList(const char *sDrive,const char *sPath,const char *sShare,const char *sAuto);
	void ExtractDrive(CString &zdrive,const char *request);
	int m_iActiveItem;
	BOOL  DismountAll(CString &msg,INT mode);
	HACCEL  m_hAccelTable; 
	CString m_sLoginName;
	BOOL m_bSaveUsername;		//TRUE means a click will save, FALSE will mean to clear
	void UpdateMountDisplay();
	UINT m_nDirTimer;				//Timer for directory update
	UINT m_nAutTimer;			//Timer for authenicate testing
	BOOL RegLastUser(CString &user,BOOL fetch);
	void UpdateConnect();
	UINT m_nShown;			//SET according level of warning has occured
	CBrush  m_bkBrush;
	INT m_seqIndex;			//index count for whirling world
	INT m_seqCount;			//count in each sequence
	INT m_dirCount;			//number of 1/2 seconds to wait before checking changes in directory
	CTransBmp m_bmpWorld;    // Bitmap to display, special bit map that does transparent painting
	CRect m_WorldRect;
	BOOL m_bRestartAFSD;		//were we suspended???
	BOOL m_bConnect;			//option to automatically start connected
	INT m_iMode;
	CEncript *m_pEncript;
	sockaddr_in m_sHostIP;
	OSVERSIONINFO m_OSVersion;
	BOOL IsWin95(){
		return (
			(m_OSVersion.dwPlatformId==VER_PLATFORM_WIN32_WINDOWS) 
			&& (m_OSVersion.dwMinorVersion==0)
			);
	}
	void ErrorDisplayState();
	int m_PowerResumeDelay;
	void AddMenu(const char *,const char *);
	void RemoveMenu(const char *);
	
//	int m_PowerDelay;			//power delay hack

	// Generated message map functions
	//{{AFX_MSG(CWinAfsLoadDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg void OnConnect();
	afx_msg void OnCancel();
	afx_msg void OnCheckadvanced();
	afx_msg void OnAppOpen();
	afx_msg void OnChange();
	afx_msg void OnAdd();
	afx_msg void OnRemove();
	afx_msg void OnClickDrivemountlist(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemchangedDrivemountlist(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg BOOL OnQueryEndSession( );
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnAuthenicate();
	afx_msg void OnDestroy();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnShow();
	afx_msg void OnSuspend();
	afx_msg void OnResume();
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	afx_msg void OnHelpmain();
	afx_msg void OnSettings();
	//}}AFX_MSG
	afx_msg void OnTrayButton0();
	afx_msg void OnTrayButton1();
	afx_msg void OnTrayButton2();
	afx_msg void OnTrayButton3();
	afx_msg void OnTrayButton4();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg LRESULT OnPowerBroadcast(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnErrorMessage(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WINAFSLOADDLG_H__75E145B5_F5C0_11D3_A374_00105A6BCA62__INCLUDED_)
