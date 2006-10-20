/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
// MyFrame.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CMyFrame frame
#ifndef	_MYFRAME_H_
#define	_MYFRAME_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols
#define LOGSHOWWINDOW 1
#define LOGSHOWPRINT 2
#define ONPARMCLOSE 0
#define ONPARMDISCONNECT 1
#define ONPARMCONNECT 2

class CProgBarDlg;
class CDatalog;

///////////////////// CMyThread stuff //////////////////////////

class CMyUIThread: public CWinThread {
	DECLARE_DYNCREATE(CMyUIThread)
	CMyUIThread();           // protected constructor used by dynamic creation
protected:
	CDatalog *m_pLog;
	CFile m_cPrint;
	CFileException m_cPrintException;
// Attributes
public:
	static HANDLE m_hEventThreadKilled;

	void operator delete(void* p);

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMyUIThread)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CMyUIThread();

	// Generated message map functions
	//{{AFX_MSG(CMyUIThread)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	afx_msg void OnParm( UINT, LONG ); 
	afx_msg void OnConnect( UINT, LONG ); 
	afx_msg void OnLog( UINT, LONG ); 
	DECLARE_MESSAGE_MAP()
};
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif
