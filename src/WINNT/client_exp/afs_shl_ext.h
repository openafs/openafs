/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

// afs_shl_ext.h : main header file for the FME DLL
//

#if !defined(AFX_AFSSHELL_H__DC515C1F_6CAC_11D1_BAE7_00C04FD140D2__INCLUDED_)
#define AFX_AFSSHELL_H__DC515C1F_6CAC_11D1_BAE7_00C04FD140D2__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CAfsShlExt
// See afs_shl_ext.cpp for the implementation of this class
//

class CAfsShlExt : public CWinApp
{
public:
	CAfsShlExt();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAfsShlExt)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CAfsShlExt)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

extern CAfsShlExt theApp;

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_AFSSHELL_H__DC515C1F_6CAC_11D1_BAE7_00C04FD140D2__INCLUDED_)
