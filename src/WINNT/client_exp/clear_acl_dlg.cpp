/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "stdafx.h"
#include "afs_shl_ext.h"
#include "clear_acl_dlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CClearAclDlg dialog


CClearAclDlg::CClearAclDlg(CWnd* pParent /*=NULL*/)
	: CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CClearAclDlg::IDD), pParent);

	//{{AFX_DATA_INIT(CClearAclDlg)
	m_bNegative = FALSE;
	m_bNormal = FALSE;
	//}}AFX_DATA_INIT
}


void CClearAclDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CClearAclDlg)
	DDX_Check(pDX, IDC_NEGATIVE, m_bNegative);
	DDX_Check(pDX, IDC_NORMAL, m_bNormal);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CClearAclDlg, CDialog)
	//{{AFX_MSG_MAP(CClearAclDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CClearAclDlg message handlers

void CClearAclDlg::GetSettings(BOOL& bNormal, BOOL& bNegative)
{
	bNormal = m_bNormal;
	bNegative = m_bNegative;
}

