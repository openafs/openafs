/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

// clear_acl_dlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CClearAclDlg dialog

class CClearAclDlg : public CDialog
{
// Construction
public:
	CClearAclDlg(CWnd* pParent = NULL);   // standard constructor

	void GetSettings(BOOL& bNormal, BOOL& bNegative);

// Dialog Data
	//{{AFX_DATA(CClearAclDlg)
	enum { IDD = IDD_CLEAR_ACL };
	BOOL	m_bNegative;
	BOOL	m_bNormal;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CClearAclDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CClearAclDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
