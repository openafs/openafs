/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
// Settings.cpp : implementation file
//

#include "stdafx.h"
#include "winafsload.h"
#include "WinAfsLoadDlg.h"
#include "Settings.h"
#include "datalog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSettings dialog


CSettings::CSettings(CWinAfsLoadDlg* pParent /*=NULL*/)
	: CDialog(CSettings::IDD, pParent)
{
	//{{AFX_DATA_INIT(CSettings)
	//}}AFX_DATA_INIT
	m_pParent=pParent;

}


BEGIN_MESSAGE_MAP(CSettings, CDialog)
	//{{AFX_MSG_MAP(CSettings)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSettings message handlers


BOOL CSettings::OnInitDialog() 
{
	CDialog::OnInitDialog();
	//set check mark if background window is visable
	// TODO: Add extra initialization here
	
	EnableToolTips(TRUE);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CSettings::OnOK() 
{
	// TODO: Add extra validation here
	CDialog::OnOK();
}



