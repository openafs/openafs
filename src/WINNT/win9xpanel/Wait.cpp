/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
// Wait.cpp : implementation file
//

#include "stdafx.h"
#include "winafsload.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CWait

int CWait::m_RefCount=0;

CWait::CWait(LPCTSTR cursor)
{
	m_RefCount++;
	m_Cursor=SetCursor(LoadCursor(NULL,cursor));
}

BOOL CWait::IsBusy()
{
	return (m_RefCount!=0);
}

CWait::~CWait()
{
	SetCursor(m_Cursor);
	m_RefCount--;
}


BEGIN_MESSAGE_MAP(CWait, CWnd)
	//{{AFX_MSG_MAP(CWait)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CWait message handlers
