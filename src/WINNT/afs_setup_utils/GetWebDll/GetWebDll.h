// GetWebDll.h : main header file for the GETWEBDLL DLL
//

#if !defined(AFX_GETWEBDLL_H__470FBE70_389E_11D5_A375_00105A6BCA62__INCLUDED_)
#define AFX_GETWEBDLL_H__470FBE70_389E_11D5_A375_00105A6BCA62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CGetWebDllApp
// See GetWebDll.cpp for the implementation of this class
//

class CGetWebDllApp : public CWinApp
{
public:
	CGetWebDllApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGetWebDllApp)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CGetWebDllApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

class CTearSession : public CInternetSession
{
public:
	CTearSession(LPCTSTR pszAppName, int nMethod);
	virtual void OnStatusCallback(DWORD dwContext, DWORD dwInternetStatus,
		LPVOID lpvStatusInfomration, DWORD dwStatusInformationLen);
};


class CTearException : public CException
{
	DECLARE_DYNCREATE(CTearException)

public:
	CTearException(int nCode = 0);
	~CTearException() { }

	int m_nErrorCode;
};

#endif // !defined(AFX_GETWEBDLL_H__470FBE70_389E_11D5_A375_00105A6BCA62__INCLUDED_)


