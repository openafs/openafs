/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

// set_afs_acl.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSetAfsAcl dialog

class CSetAfsAcl : public CDialog
{
	CString m_strDir;
	CString m_strCellName;
	BOOL m_bShowingNormal;
	CStringArray m_Normal, m_Negative;
	BOOL m_bChanges;
	int m_nCurSel;

	void ShowRights(const CString& strRights);
	CString MakeRightsString();
	void EnablePermChanges(BOOL bEnable);

	void OnNothingSelected();
	void OnSelection();

	// Construction
public:
	CSetAfsAcl(CWnd* pParent = NULL);   // standard constructor

	void SetDir(const CString strDir)		{ m_strDir = strDir; }

	BOOL IsNameInUse(BOOL bNormal, const CString& strName);

// Dialog Data
	//{{AFX_DATA(CSetAfsAcl)
	enum { IDD = IDD_SET_AFS_ACL };
	CButton	m_Remove;
	CButton	m_AdminPerm;
	CButton	m_LockPerm;
	CButton	m_InsertPerm;
	CButton	m_DeletePerm;
	CButton	m_LookupPerm;
	CButton	m_WritePerm;
	CButton	m_ReadPerm;
	CStatic	m_DirName;
	CListBox	m_NegativeRights;
	CListBox	m_NormalRights;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSetAfsAcl)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSetAfsAcl)
	afx_msg void OnClear();
	afx_msg void OnAdd();
	afx_msg void OnCopy();
	virtual BOOL OnInitDialog();
	afx_msg void OnSelChangeNormalRights();
	afx_msg void OnSelChangeNegativeEntries();
	afx_msg void OnPermChange();
	afx_msg void OnRemove();
	virtual void OnOK();
	afx_msg void OnClean();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
