/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

// partition_info_dlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CPartitionInfoDlg dialog

class CPartitionInfoDlg : public CDialog
{
	LONG m_nSize;
	LONG m_nFree;
	
// Construction
public:
	CPartitionInfoDlg(CWnd* pParent = NULL);   // standard constructor

	void SetValues(LONG nSize, LONG nFree)	{ m_nSize = nSize; m_nFree = nFree; }

// Dialog Data
	//{{AFX_DATA(CPartitionInfoDlg)
	enum { IDD = IDD_PARTITION_INFO };
	CEdit	m_Size;
	CEdit	m_PercentUsed;
	CEdit	m_Free;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPartitionInfoDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CPartitionInfoDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
