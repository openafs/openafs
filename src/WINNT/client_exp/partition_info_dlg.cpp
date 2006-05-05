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

#include "afs_shl_ext.h"
#include "partition_info_dlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPartitionInfoDlg dialog


CPartitionInfoDlg::CPartitionInfoDlg(CWnd* pParent /*=NULL*/)
	: CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CPartitionInfoDlg::IDD), pParent);

	//{{AFX_DATA_INIT(CPartitionInfoDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	m_nSize = 0;
	m_nFree = 0;
}

void CPartitionInfoDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPartitionInfoDlg)
	DDX_Control(pDX, IDC_TOTAL_SIZE, m_Size);
	DDX_Control(pDX, IDC_PERCENT_USED, m_PercentUsed);
	DDX_Control(pDX, IDC_BLOCKS_FREE, m_Free);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CPartitionInfoDlg, CDialog)
	//{{AFX_MSG_MAP(CPartitionInfoDlg)
	ON_BN_CLICKED(IDHELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPartitionInfoDlg message handlers

BOOL CPartitionInfoDlg::OnInitDialog() 
{
    double percentUsed;     // because partition sizes are big

	CDialog::OnInitDialog();
	
	ASSERT(m_nSize != 0);

	CString strSize;
	strSize.Format("%ld", m_nSize);
	
	CString strFree;
	strFree.Format("%ld", m_nFree);
	
	CString strPerUsed;
	strPerUsed.Format("%d", ((m_nSize - m_nFree) * 100) / m_nSize);

	m_Size.SetWindowText(strSize);
	m_Free.SetWindowText(strFree);
    percentUsed = ( double(m_nSize - m_nFree) * 100.0l ) / double(m_nSize);
    strPerUsed.Format("%2.2lf", percentUsed );

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CPartitionInfoDlg::OnHelp() 
{
	ShowHelp(m_hWnd, PARTITION_INFO_HELP_ID);
}

