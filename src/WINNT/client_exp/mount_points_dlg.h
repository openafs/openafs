/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

// mount_points_dlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CMountPointsDlg dialog

class CMountPointsDlg : public CDialog
{
	CStringArray m_MountPoints;
	
// Construction
public:
	CMountPointsDlg(CWnd* pParent = NULL);   // standard constructor

	void SetMountPoints(const CStringArray& mountPoints);

// Dialog Data
	//{{AFX_DATA(CMountPointsDlg)
	enum { IDD = IDD_MOUNT_POINTS };
	CListBox	m_List;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMountPointsDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CMountPointsDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
