/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

class CVolInfo;


class CVolumeInfo : public CDialog
{
	CStringArray m_Files;
	CVolInfo *m_pVolInfo;

	int GetCurVolInfoIndex();

	void ShowInfo();

	int m_nCurIndex;

// Construction
public:
	CVolumeInfo(CWnd* pParent = NULL);   // standard constructor
	~CVolumeInfo();

	void SetFiles(const CStringArray& files);

// Dialog Data
	//{{AFX_DATA(CVolumeInfo)
	enum { IDD = IDD_VOLUME_INFO };
	CSpinButtonCtrl	m_QuotaSpin;
	CButton	m_Ok;
	CButton	m_ShowPartInfo;
	CListBox	m_List;
	long	m_nNewQuota;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVolumeInfo)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CVolumeInfo)
	virtual BOOL OnInitDialog();
	afx_msg void OnSelChangeList();
	afx_msg void OnPartitionInfo();
	afx_msg void OnChangeNewQuota();
	virtual void OnOK();
	afx_msg void OnDeltaPosQuotaSpin(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
