/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

// unlog_dlg.h : header file
//
#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CUnlogDlg dialog

class CUnlogDlg : public CDialog
{
// Construction
public:
	CUnlogDlg(CWnd* pParent = NULL);	// standard constructor

	void SetCellName(const CString& strCellName)	{ m_strCellName = strCellName; }

// Dialog Data
	//{{AFX_DATA(CUnlogDlg)
	enum { IDD = IDD_UNLOG_DIALOG };
	CButton	m_OK;
	CString	m_strCellName;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CUnlogDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CUnlogDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeCellName();
	virtual void OnOK();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
