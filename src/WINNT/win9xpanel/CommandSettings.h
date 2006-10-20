/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#if !defined(AFX_COMMANDSETTINGS_H__3CDB0E81_7A6D_11D4_A374_00105A6BCA62__INCLUDED_)
#define AFX_COMMANDSETTINGS_H__3CDB0E81_7A6D_11D4_A374_00105A6BCA62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CommandSettings.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CCommandSettings dialog

class CCommandSettings : public CDialog
{
// Construction
public:
	CCommandSettings(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CCommandSettings)
	enum { IDD = IDD_SETTINGS };
	CButton	m_cOptionLine;
	CButton	m_cCheckAdvanceDisplay;
	BOOL	m_ConnectOnStart;
	BOOL	m_LogToFile;
	BOOL	m_LogToWindow;
	CString	m_UserName;
	UINT	m_uMaxLoginTime;
	UINT	m_uMaxPowerRestartDelay;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCommandSettings)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CRect m_OriginalRect;
	UINT m_DialogShrink;
	// Generated message map functions
	//{{AFX_MSG(CCommandSettings)
	afx_msg void OnSettings();
	afx_msg void OnCheckadvanced();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_COMMANDSETTINGS_H__3CDB0E81_7A6D_11D4_A374_00105A6BCA62__INCLUDED_)
