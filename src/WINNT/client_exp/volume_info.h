/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

// volume_info.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CVolumeInfo dialog

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
