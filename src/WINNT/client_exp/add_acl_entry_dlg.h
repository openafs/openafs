/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */
#ifndef __ADD_ACL_ENTRY_DLG_H__
#define __ADD_ACL_ENTRY_DLG_H__

class CSetACLInterface
{
public:
    virtual BOOL IsNameInUse(BOOL bNormal, const CString& strName) = 0;
};

/////////////////////////////////////////////////////////////////////////////
// CAddAclEntryDlg dialog

class CAddAclEntryDlg : public CDialog
{
	BOOL m_bNormal;
	CString m_Rights;
	CString m_strName;
	CSetACLInterface *m_pAclSetDlg;

	CString MakePermString();

// Construction
public:
	CAddAclEntryDlg(CWnd* pParent = NULL);   // standard constructor

	void SetAclDlg(CSetACLInterface *pAclSetDlg)	{ m_pAclSetDlg = pAclSetDlg; }

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

#endif
