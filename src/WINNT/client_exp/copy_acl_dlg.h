/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

class CCopyAclDlg : public CDialog
{
	CString m_strToDir;
	BOOL m_bClear;
	CString m_strFromDir;

// Construction
public:
	CCopyAclDlg(CWnd* pParent = NULL);   // standard constructor

	const CString& GetToDir()		{ return m_strToDir; }
	BOOL GetClear()					{ return m_bClear; }

	void SetFromDir(const CString& strFromDir)	{ m_strFromDir = strFromDir; }

// Dialog Data
	//{{AFX_DATA(CCopyAclDlg)
	enum { IDD = IDD_COPY_ACL };
	CButton	m_Ok;
	CEdit	m_FromDir;
	CEdit	m_ToDir;
	CButton	m_Clear;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCopyAclDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CCopyAclDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeToDir();
	afx_msg void OnBrowse();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
