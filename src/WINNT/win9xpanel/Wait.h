/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/

#if !defined(AFX_WAIT_H__F7E2A603_FBFD_11D3_A374_00105A6BCA62__INCLUDED_)
#define AFX_WAIT_H__F7E2A603_FBFD_11D3_A374_00105A6BCA62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Wait.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CWait window

class CWait : public CWnd
{
// Construction
public:
	CWait(LPCTSTR cursor=IDC_WAIT);

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWait)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CWait();
	static BOOL IsBusy();

	// Generated message map functions
protected:
	static int m_RefCount;
	HCURSOR m_Cursor;
	//{{AFX_MSG(CWait)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WAIT_H__F7E2A603_FBFD_11D3_A374_00105A6BCA62__INCLUDED_)
