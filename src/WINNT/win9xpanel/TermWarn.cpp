/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
// TermWarn.cpp : implementation file
//

#include "stdafx.h"
#include "winafsload.h"
#include "TermWarn.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTermWarn dialog


CTermWarn::CTermWarn(CWnd* pParent /*=NULL*/)
	: CDialog(CTermWarn::IDD, pParent)
{
	//{{AFX_DATA_INIT(CTermWarn)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CTermWarn::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTermWarn)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTermWarn, CDialog)
	//{{AFX_MSG_MAP(CTermWarn)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTermWarn message handlers

void CTermWarn::PostNcDestroy() 
{
	// TODO: Add your specialized code here and/or call the base class
	delete this;
}

BOOL CTermWarn::Create()
{
	return CDialog::Create(IDD, NULL);
}

void CTermWarn::OnCancel()
{
	DestroyWindow();
}

