/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#if !defined(AFX_DATALOG_H__C973E961_F990_11D3_A374_00105A6BCA62__INCLUDED_)
#define AFX_DATALOG_H__C973E961_F990_11D3_A374_00105A6BCA62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Datalog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDatalog dialog

class CDatalog : public CDialog
{
// Construction
public:
	CDatalog(CWnd* pParent = NULL);   // standard constructor
	BOOL Create();
	void OnCancel();

// Dialog Data
	//{{AFX_DATA(CDatalog)
	enum { IDD = IDD_ACTIVITYLOG };
	CEdit	m_cEdit;
	CString	m_sEdit;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDatalog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void PostNcDestroy();
	virtual void OnSysCommand(UINT nID, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
protected:
	CWnd* m_pParent;

	// Generated message map functions
	//{{AFX_MSG(CDatalog)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DATALOG_H__C973E961_F990_11D3_A374_00105A6BCA62__INCLUDED_)
