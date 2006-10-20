/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#if !defined(AFX_TERMWARN_H__EBCBE030_03C3_11D4_A374_00105A6BCA62__INCLUDED_)
#define AFX_TERMWARN_H__EBCBE030_03C3_11D4_A374_00105A6BCA62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TermWarn.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CTermWarn dialog

class CTermWarn : public CDialog
{
// Construction
public:
	CTermWarn(CWnd* pParent = NULL);   // standard constructor
	BOOL Create();
	void OnCancel();

// Dialog Data
	//{{AFX_DATA(CTermWarn)
	enum { IDD = IDD_DIALOGTERM };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTermWarn)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CTermWarn)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TERMWARN_H__EBCBE030_03C3_11D4_A374_00105A6BCA62__INCLUDED_)
