/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

// submounts_dlg.h : header file
//
#ifndef _SUBMOUNTSDLG_H_
#define _SUBMOUNTSDLG_H_

#include "resource.h"

#include "submount_info.h"

/////////////////////////////////////////////////////////////////////////////
// CSubmountsDlg dialog

class CSubmountsDlg : public CDialog
{
	DECLARE_DYNCREATE(CSubmountsDlg)

	BOOL m_bAddOnlyMode;
	CString m_strAddOnlyPath;

	SUBMT_INFO_ARRAY m_ToDo;

	BOOL FillSubmtList();
	BOOL FixSubmts();
	void AddWork(CSubmountInfo *pInfo);
	CSubmountInfo *FindWork(const CString& strShareName);

// Construction
public:
	CSubmountsDlg();
	~CSubmountsDlg();

	void SetAddOnlyMode(const CString& strAddOnlyPath);

// Dialog Data
	//{{AFX_DATA(CSubmountsDlg)
	enum { IDD = IDD_SUBMTINFO };
	CButton	m_Delete;
	CButton	m_Change;
	CListBox	m_SubmtList;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSubmountsDlg)
	public:
	virtual void WinHelp(DWORD dwData, UINT nCmd = HELP_CONTEXT);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSubmountsDlg)
	afx_msg void OnAdd();
	afx_msg void OnChange();
	virtual BOOL OnInitDialog();
	afx_msg void OnDelete();
	afx_msg void OnSelChangeList();
	afx_msg void OnOk();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

#endif	// _SUBMOUNTSDLG_H_

