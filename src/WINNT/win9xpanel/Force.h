/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#if !defined(AFX_FORCE_H__EBCBE049_03C3_11D4_A374_00105A6BCA62__INCLUDED_)
#define AFX_FORCE_H__EBCBE049_03C3_11D4_A374_00105A6BCA62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Force.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CForce dialog

class CForce : public CDialog
{
// Construction
public:
	CForce(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CForce)
	enum { IDD = IDD_DIALOGFORCE };
	CString	m_sMsg;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CForce)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CForce)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FORCE_H__EBCBE049_03C3_11D4_A374_00105A6BCA62__INCLUDED_)
