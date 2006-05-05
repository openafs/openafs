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
#include "add_acl_entry_dlg.h"
#include "set_afs_acl.h"
#include "msgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAddAclEntryDlg dialog


CAddAclEntryDlg::CAddAclEntryDlg(CWnd* pParent /*=NULL*/)
	: CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CAddAclEntryDlg::IDD), pParent);

	//{{AFX_DATA_INIT(CAddAclEntryDlg)
	//}}AFX_DATA_INIT

	m_pAclSetDlg = 0;
}


void CAddAclEntryDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddAclEntryDlg)
	DDX_Control(pDX, IDOK, m_Ok);
	DDX_Control(pDX, IDC_NAME, m_Name);
	DDX_Control(pDX, IDC_ADD_NORMAL_ENTRY, m_NormalEntry);
	DDX_Control(pDX, IDC_LOOKUP2, m_LookupPerm);
	DDX_Control(pDX, IDC_LOCK2, m_LockPerm);
	DDX_Control(pDX, IDC_WRITE, m_WritePerm);
	DDX_Control(pDX, IDC_ADMINISTER, m_AdminPerm);
	DDX_Control(pDX, IDC_READ, m_ReadPerm);
	DDX_Control(pDX, IDC_INSERT, m_InsertPerm);
	DDX_Control(pDX, IDC_DELETE, m_DeletePerm);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAddAclEntryDlg, CDialog)
	//{{AFX_MSG_MAP(CAddAclEntryDlg)
	ON_BN_CLICKED(IDC_ADD_NEGATIVE_ENTRY, OnAddNegativeEntry)
	ON_BN_CLICKED(IDC_ADD_NORMAL_ENTRY, OnAddNormalEntry)
	ON_EN_CHANGE(IDC_NAME, OnChangeName)
	ON_BN_CLICKED(IDHELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAddAclEntryDlg message handlers

void CAddAclEntryDlg::OnAddNegativeEntry() 
{
	m_bNormal = FALSE;
}

void CAddAclEntryDlg::OnAddNormalEntry() 
{
	m_bNormal = TRUE;
}

BOOL CAddAclEntryDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	ASSERT_VALID(m_pAclSetDlg);

	m_NormalEntry.SetCheck(CHECKED);
	m_Ok.EnableWindow(FALSE);

	m_Name.SetFocus();

	m_bNormal = TRUE;
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

CString CAddAclEntryDlg::MakePermString()
{
	CString str;

	if (m_ReadPerm.GetCheck() == CHECKED)
		str += "r";
	if (m_LookupPerm.GetCheck() == CHECKED)
		str += "l";
	if (m_InsertPerm.GetCheck() == CHECKED)
		str += "i";
	if (m_DeletePerm.GetCheck() == CHECKED)
		str += "d";
	if (m_WritePerm.GetCheck() == CHECKED)
		str += "w";
	if (m_LockPerm.GetCheck() == CHECKED)
		str += "k";
	if (m_AdminPerm.GetCheck() == CHECKED)
		str += "a";

	return str;
}

void CAddAclEntryDlg::OnOK() 
{
	m_Rights = MakePermString();
	m_Name.GetWindowText(m_strName);

	if (m_pAclSetDlg->IsNameInUse(m_bNormal, m_strName)) {
		ShowMessageBox(IDS_ACL_ENTRY_NAME_IN_USE, MB_ICONEXCLAMATION, IDS_ACL_ENTRY_NAME_IN_USE);
		return;
	}

	CDialog::OnOK();
}

void CAddAclEntryDlg::OnChangeName() 
{
	m_Name.GetWindowText(m_strName);

	if (m_strName.GetLength() > 0)
		m_Ok.EnableWindow(TRUE);
}

void CAddAclEntryDlg::OnHelp() 
{
	ShowHelp(m_hWnd, ADD_ACL_ENTRY_HELP_ID);
}

