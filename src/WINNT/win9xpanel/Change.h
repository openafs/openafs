/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#if !defined(AFX_CHANGE_H__CF7462A3_F999_11D3_A374_00105A6BCA62__INCLUDED_)
#define AFX_CHANGE_H__CF7462A3_F999_11D3_A374_00105A6BCA62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Change.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CChange dialog

class CChange : public CDialog
{
// Construction
public:
	CChange(BOOL change,CWnd* pParent = NULL);   // standard constructor
	CString m_sDrive;

// Dialog Data
	//{{AFX_DATA(CChange)
	enum { IDD = IDD_AFSCHANGE };
	CEdit	m_cShare;
	CButton	m_cAuto;
	CEdit	m_cPath;
	CComboBox	m_cDrive;
	CString	m_sPath;
	CString	m_sDescription;
	BOOL	m_bAuto;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChange)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	BOOL m_bChange;
	void ListDrive();
	BOOL IsValidSubmountName (LPCTSTR pszSubmount);
	BOOL IsValidShareName (LPCTSTR pszSubmount);
	CWinAfsLoadDlg *m_pParent;
	// Generated message map functions
	//{{AFX_MSG(CChange)
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	afx_msg void OnHelpmain();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CHANGE_H__CF7462A3_F999_11D3_A374_00105A6BCA62__INCLUDED_)
