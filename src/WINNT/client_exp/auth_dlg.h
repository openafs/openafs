/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

// auth_dlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CAuthDlg dialog

class CAuthDlg : public CDialog
{
	void FillTokenList();
	CString GetSelectedCellName();

// Construction
public:
	CAuthDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CAuthDlg)
	enum { IDD = IDD_AUTHENTICATION };
	CListBox	m_TokenList;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAuthDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CAuthDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnGetTokens();
	afx_msg void OnDiscardTokens();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
