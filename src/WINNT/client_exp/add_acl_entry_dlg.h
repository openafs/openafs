/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

// add_acl_entry_dlg.h : header file
//
class CSetAfsAcl;

/////////////////////////////////////////////////////////////////////////////
// CAddAclEntryDlg dialog

class CAddAclEntryDlg : public CDialog
{
	BOOL m_bNormal;
	CString m_Rights;
	CString m_strName;
	CSetAfsAcl *m_pAclSetDlg;

	CString MakePermString();

// Construction
public:
	CAddAclEntryDlg(CWnd* pParent = NULL);   // standard constructor

	void SetAclDlg(CSetAfsAcl *pAclSetDlg)	{ m_pAclSetDlg = pAclSetDlg; }

	CString GetName()		{ return m_strName; }
	CString GetRights()		{ return m_Rights; }
	BOOL IsNormal()			{ return m_bNormal; }

// Dialog Data
	//{{AFX_DATA(CAddAclEntryDlg)
	enum { IDD = IDD_ADD_ACL };
	CButton	m_Ok;
	CEdit	m_Name;
	CButton	m_NormalEntry;
	CButton	m_LookupPerm;
	CButton	m_LockPerm;
	CButton	m_WritePerm;
	CButton	m_AdminPerm;
	CButton	m_ReadPerm;
	CButton	m_InsertPerm;
	CButton	m_DeletePerm;
	//}}AFX_DATA

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAddAclEntryDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CAddAclEntryDlg)
	afx_msg void OnAddNegativeEntry();
	afx_msg void OnAddNormalEntry();
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnChangeName();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
