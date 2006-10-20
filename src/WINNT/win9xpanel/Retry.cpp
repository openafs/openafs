/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
// Retry.cpp : implementation file
//

#include "stdafx.h"
#include "winafsload.h"
#include "Retry.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CRetry dialog


CRetry::CRetry(BOOL force,CWnd* pParent /*=NULL*/)
	: CDialog(CRetry::IDD, pParent)
{
	//{{AFX_DATA_INIT(CRetry)
	m_sMsg = _T("");
	//}}AFX_DATA_INIT
	m_force=force;
}


void CRetry::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRetry)
	DDX_Control(pDX, IDC_STATICOPTIONS, m_cOptions);
	DDX_Control(pDX, IDC_FORCE, m_cForce);
	DDX_Text(pDX, IDC_STATICMSG, m_sMsg);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CRetry, CDialog)
	//{{AFX_MSG_MAP(CRetry)
	ON_BN_CLICKED(IDC_FORCE, OnForceDismont)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRetry message handlers

void CRetry::OnForceDismont() 
{
	// TODO: Add your control notification handler code here
	EndDialog(IDC_FORCE); 
}

BOOL CRetry::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	
	if (m_force)
	{
		m_cForce.ModifyStyle(WS_DISABLED,0,0);
		m_cOptions.SetWindowText("You map shut down any applications that are using the drive letter and then press Retry.  Or press Cancel to the bypass the disconnect.\nYou may over-ride the file protection (possible loss of 'opened' file data) and slect Force to disconnect.");
	}else {
		m_cForce.ModifyStyle(0,WS_DISABLED,0);
		m_cOptions.SetWindowText("You may shut down any applications that are using the drive letter and then press Retry.  Or press Cancel to the bypass the disconnect.\n('Force' is not premitted.)");
	}
		
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
