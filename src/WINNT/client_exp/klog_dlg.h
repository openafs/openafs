/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CKlogDlg dialog

class CKlogDlg : public CDialog
{
	void CheckEnableOk();
	
// Construction
public:
	CKlogDlg(CWnd* pParent = NULL);	// standard constructor

	void SetCellName(const CString& strCellName)	{ m_strCellName = strCellName; }

// Dialog Data
	//{{AFX_DATA(CKlogDlg)
	enum { IDD = IDD_KLOG_DIALOG };
	CButton	m_OK;
	CString	m_strName;
	CString	m_strPassword;
	CString	m_strCellName;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKlogDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CKlogDlg)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnChangeName();
	afx_msg void OnChangeCellName();
	afx_msg void OnChangePassword();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
