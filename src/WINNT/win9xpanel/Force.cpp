/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
// Force.cpp : implementation file
//

#include "stdafx.h"
#include "winafsload.h"
#include "Force.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CForce dialog


CForce::CForce(CWnd* pParent /*=NULL*/)
	: CDialog(CForce::IDD, pParent)
{
	//{{AFX_DATA_INIT(CForce)
	m_sMsg = _T("");
	//}}AFX_DATA_INIT
}


void CForce::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CForce)
	DDX_Text(pDX, IDC_STATICMSG, m_sMsg);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CForce, CDialog)
	//{{AFX_MSG_MAP(CForce)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CForce message handlers
