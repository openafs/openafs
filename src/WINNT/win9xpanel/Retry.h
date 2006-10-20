/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#if !defined(AFX_RETRY_H__E69A224D_0AE0_11D4_A374_00105A6BCA62__INCLUDED_)
#define AFX_RETRY_H__E69A224D_0AE0_11D4_A374_00105A6BCA62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Retry.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CRetry dialog

class CRetry : public CDialog
{
// Construction
public:
	CRetry(BOOL force,CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CRetry)
	enum { IDD = IDD_DIALOGRETRY };
	CStatic	m_cOptions;
	CButton	m_cForce;
	CString	m_sMsg;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRetry)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	BOOL m_force;

	// Generated message map functions
	//{{AFX_MSG(CRetry)
	afx_msg void OnForceDismont();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_RETRY_H__E69A224D_0AE0_11D4_A374_00105A6BCA62__INCLUDED_)
