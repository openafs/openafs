/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

// results_dlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CResultsDlg dialog

class CResultsDlg : public CDialog
{
	CStringArray m_Files;
	CStringArray m_Results;
	CString m_strDlgTitle;
	CString m_strResultsTitle;
	DWORD m_nHelpID;

// Construction
public:
	CResultsDlg(DWORD nHelpID, CWnd* pParent = NULL);   // standard constructor

	void SetContents(const CString& strDlgTitle, const CString& strResultsTitle, const CStringArray& files, const CStringArray& results);

// Dialog Data
	//{{AFX_DATA(CResultsDlg)
	enum { IDD = IDD_RESULTS };
	CStatic	m_ResultsLabel;
	CListBox	m_List;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CResultsDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CResultsDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
