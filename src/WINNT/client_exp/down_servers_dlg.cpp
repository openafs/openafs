/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include "stdafx.h"
#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "down_servers_dlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDownServersDlg dialog


CDownServersDlg::CDownServersDlg(CWnd* pParent /*=NULL*/)
	: CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CDownServersDlg::IDD), pParent);

	//{{AFX_DATA_INIT(CDownServersDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

void CDownServersDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDownServersDlg)
	DDX_Control(pDX, IDC_LIST, m_ServerList);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CDownServersDlg, CDialog)
	//{{AFX_MSG_MAP(CDownServersDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDownServersDlg message handlers

BOOL CDownServersDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	for (int i = 0; i < m_ServerNames.GetSize(); i++)
		m_ServerList.AddString(m_ServerNames[i]);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CDownServersDlg::SetServerNames(const CStringArray& serverNames)
{
	m_ServerNames.RemoveAll();

	m_ServerNames.Copy(serverNames);
}

