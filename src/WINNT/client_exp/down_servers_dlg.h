/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

// down_servers_dlg.h : header file
//
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
