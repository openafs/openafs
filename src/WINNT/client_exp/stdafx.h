/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#if !defined(AFX_STDAFX_H__601A9D0D_6CD3_11D1_BAE7_00C04FD140D2__INCLUDED_)
#define AFX_STDAFX_H__601A9D0D_6CD3_11D1_BAE7_00C04FD140D2__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

// Don't include stuff we don't need.
#define _AFX_NO_DB_SUPPORT
#define _AFX_NO_DAO_SUPPORT

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxole.h>         // MFC OLE classes
#include <afxodlgs.h>       // MFC OLE dialog classes
#include <afxdisp.h>        // MFC OLE automation classes
#endif // _AFX_NO_OLE_SUPPORT


#ifndef _AFX_NO_DB_SUPPORT
#include <afxdb.h>			// MFC ODBC database classes
#endif // _AFX_NO_DB_SUPPORT

#ifndef _AFX_NO_DAO_SUPPORT
#include <afxdao.h>			// MFC DAO database classes
#endif // _AFX_NO_DAO_SUPPORT

#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#define UNCHECKED	0
#define CHECKED		1

#include "help.h"

#include <WINNT/TaLocale.h>

#if defined (_DEBUG) && defined (AFS_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

#endif // !defined(AFX_STDAFX_H__601A9D0D_6CD3_11D1_BAE7_00C04FD140D2__INCLUDED_)
