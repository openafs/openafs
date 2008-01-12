/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

class CSymlinksDlg : public CDialog
{
	CStringArray m_Symlinks;
	
// Construction
public:
	CSymlinksDlg(CWnd* pParent = NULL);   // standard constructor

	void SetSymlinks(const CStringArray& mountPoints);

// Dialog Data
	//{{AFX_DATA(CSymlinksDlg)
	enum { IDD = IDD_SYMLINKS };
	CListBox	m_List;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSymlinksDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSymlinksDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
