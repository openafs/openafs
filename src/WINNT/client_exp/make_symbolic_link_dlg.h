/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

class CMakeSymbolicLinkDlg : public CDialog
{
	void CheckEnableOk();
	
// Construction
public:
	CMakeSymbolicLinkDlg(CWnd* pParent = NULL);   // standard constructor
	void Setbase(const char *msg){m_sBase=msg;}
// Dialog Data
	//{{AFX_DATA(CMakeSymbolicLinkDlg)
	enum { IDD = IDD_SYMBOLICLINK_ADD };
	CButton	m_OK;
	CEdit	m_Name;
	CEdit	m_Dir;
	CString	m_strName;
	CString	m_strDir;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMakeSymbolicLinkDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CString m_sBase;	//Base directory
	// Generated message map functions
	//{{AFX_MSG(CMakeSymbolicLinkDlg)
	afx_msg void OnChangeDir();
	afx_msg void OnChangeName();
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
