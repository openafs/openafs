/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

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
