/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__75E145B7_F5C0_11D3_A374_00105A6BCA62__INCLUDED_)
#define AFX_STDAFX_H__75E145B7_F5C0_11D3_A374_00105A6BCA62__INCLUDED_

#ifdef _WIN32_WINDOWS
#undef _WIN32_WINDOWS		//required when you do a make because it includes win32.mak 
#endif

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.
#include <afxtempl.h>		// add this for templets CList
#ifdef WINSOCK2
#include "winsock2.h"
#endif
#include <afxsock.h>		// MFC socket extensions
#include "wait.h"

#endif // !defined(AFX_STDAFX_H__75E145B7_F5C0_11D3_A374_00105A6BCA62__INCLUDED_)
