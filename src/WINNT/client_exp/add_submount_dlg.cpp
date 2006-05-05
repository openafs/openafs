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

#include "add_submount_dlg.h"
#include "submount_info.h"
#include "help.h"
#include "msgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAddSubmtDlg dialog


CAddSubmtDlg::CAddSubmtDlg(CWnd* pParent /*=NULL*/)
	: CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CAddSubmtDlg::IDD), pParent);

	//{{AFX_DATA_INIT(CAddSubmtDlg)
	m_strShareName = _T("");
	m_strPathName = _T("");
	//}}AFX_DATA_INIT

	m_bAdd = TRUE;
	m_bSave = FALSE;
}


void CAddSubmtDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddSubmtDlg)
	DDX_Control(pDX, IDOK, m_Ok);
	DDX_Text(pDX, IDC_SHARE_NAME, m_strShareName);
	DDX_Text(pDX, IDC_PATH_NAME, m_strPathName);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAddSubmtDlg, CDialog)
	//{{AFX_MSG_MAP(CAddSubmtDlg)
	ON_EN_CHANGE(IDC_SHARE_NAME, OnChangeShareName)
	ON_EN_CHANGE(IDC_PATH_NAME, OnChangePathName)
	ON_BN_CLICKED(IDHELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAddSubmtDlg message handlers

BOOL CAddSubmtDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	if (!m_bAdd) {
		SetWindowText(GetMessageString(IDS_EDIT_PATH_NAME));
		((CEdit *)GetDlgItem(IDC_SHARE_NAME))->EnableWindow(FALSE);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CAddSubmtDlg::CheckEnableOk()
{
	UpdateData(TRUE);

	m_Ok.EnableWindow(!m_strShareName.IsEmpty() && !m_strPathName.IsEmpty());
}

void CAddSubmtDlg::OnChangeShareName() 
{
	CheckEnableOk();
}

void CAddSubmtDlg::OnChangePathName() 
{
	CheckEnableOk();
}

void CAddSubmtDlg::OnOK()
{
	UpdateData(TRUE);

	m_bSave = TRUE;

	CDialog::OnOK();
}

void CAddSubmtDlg::SetSubmtInfo(CSubmountInfo *pInfo)
{
	ASSERT_VALID(pInfo);
	
	m_strShareName = pInfo->GetShareName();
	m_strPathName = pInfo->GetPathName();
}

CSubmountInfo *CAddSubmtDlg::GetSubmtInfo()
{
	if (!m_bSave)
		return 0;
	
	SUBMT_INFO_STATUS status;

	if (m_bAdd)
		status = SIS_ADDED;
	else
		status = SIS_CHANGED;
	
	return new CSubmountInfo(m_strShareName, m_strPathName, status);
}

void CAddSubmtDlg::OnHelp() 
{
	ShowHelp(m_hWnd, (m_bAdd ? ADD_SUBMT_HELP_ID : EDIT_PATH_NAME_HELP_ID));
}

