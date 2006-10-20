/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

class CMakeMountPointDlg : public CDialog
{
	CString m_strDir;
	CString m_strVol;
	CString m_strCell;
	BOOL m_bMade;

	void CheckEnableOk();
	
// Construction
public:
	CMakeMountPointDlg(CWnd* pParent = NULL);   // standard constructor

	void SetDir(const CString& strDir)	{ m_strDir = strDir; }
	BOOL MountWasMade()					{ return m_bMade; }

// Dialog Data
	//{{AFX_DATA(CMakeMountPointDlg)
	enum { IDD = IDD_MAKE_MOUNT_POINT };
	CButton	m_Ok;
	CEdit	m_Vol;
	CButton	m_RW;
	CEdit	m_Dir;
	CEdit	m_Cell;
	int		m_nType;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMakeMountPointDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CMakeMountPointDlg)
	virtual void OnOK();
	afx_msg void OnChangeVolume();
	afx_msg void OnChangeDir();
	afx_msg void OnChangeCell();
	virtual BOOL OnInitDialog();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
