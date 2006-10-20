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
// CServerStatusDlg dialog

class CServerStatusDlg : public CDialog
{
	DECLARE_DYNCREATE(CServerStatusDlg)

	CString GetCellNameText();
	void CheckEnableShowStatus();

// Construction
public:
	CServerStatusDlg();
	~CServerStatusDlg();

	BOOL Save();

// Dialog Data
	//{{AFX_DATA(CServerStatusDlg)
	enum { IDD = IDD_SERVERSTATUS };
	CButton	m_ShowStatus;
	CEdit	m_CellName;
	BOOL	m_bFast;
	int		m_nCell;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CServerStatusDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CServerStatusDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnShowStatus();
	afx_msg void OnSpecifiedCell();
	afx_msg void OnLocalCell();
	afx_msg void OnAllCells();
	afx_msg void OnChangeCellName();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};
