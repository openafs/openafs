/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CDownServersDlg dialog

class CDownServersDlg : public CDialog
{
	CStringArray m_ServerNames;
	
// Construction
public:
	CDownServersDlg(CWnd* pParent = NULL);   // standard constructor

	void SetServerNames(const CStringArray& serverNames);

// Dialog Data
	//{{AFX_DATA(CDownServersDlg)
	enum { IDD = IDD_DOWN_SERVERS };
	CListBox	m_ServerList;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDownServersDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDownServersDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
