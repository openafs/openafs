/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include "resource.h"

class CSubmountInfo;

/////////////////////////////////////////////////////////////////////////////
// CAddSubmtDlg dialog

class CAddSubmtDlg : public CDialog
{
	BOOL m_bAdd;
	BOOL m_bSave;
	
	void CheckEnableOk();
	
// Construction
public:
	CAddSubmtDlg(CWnd* pParent = NULL);   // standard constructor

	void SetAddMode(BOOL bAddMode)	{ m_bAdd = bAddMode; }

	void SetSubmtInfo(CSubmountInfo *pInfo);
	CSubmountInfo *GetSubmtInfo();

// Dialog Data
	//{{AFX_DATA(CAddSubmtDlg)
	enum { IDD = IDD_ADD_SUBMOUNT };
	CButton	m_Ok;
	CString	m_strShareName;
	CString	m_strPathName;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAddSubmtDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CAddSubmtDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeShareName();
	afx_msg void OnChangePathName();
	virtual void OnOK();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
