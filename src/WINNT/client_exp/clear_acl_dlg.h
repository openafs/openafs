/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

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
