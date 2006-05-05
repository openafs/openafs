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

#include "klog_dlg.h"
#include "hourglass.h"

extern "C" {
#include <afs/param.h>
#include <afs/kautils.h>
#include "cm_config.h"
}

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define PCCHAR(str)	((char *)(const char *)str)


/////////////////////////////////////////////////////////////////////////////
// CKlogDlg dialog

int kl_Authenticate(const CString& strCellName, const CString& strName, const CString& strPassword, char **reason)
{
	afs_int32 pw_exp;

	return ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION, PCCHAR(strName), "", PCCHAR(strCellName), PCCHAR(strPassword), 0, &pw_exp, 0, reason);
}

CKlogDlg::CKlogDlg(CWnd* pParent /*=NULL*/)
	: CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CKlogDlg::IDD), pParent);

	//{{AFX_DATA_INIT(CKlogDlg)
	m_strName = _T("");
	m_strPassword = _T("");
	m_strCellName = _T("");
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
}

void CKlogDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKlogDlg)
	DDX_Control(pDX, IDOK, m_OK);
	DDX_Text(pDX, IDC_NAME, m_strName);
	DDX_Text(pDX, IDC_PASSWORD, m_strPassword);
	DDX_Text(pDX, IDC_CELL_NAME, m_strCellName);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CKlogDlg, CDialog)
	//{{AFX_MSG_MAP(CKlogDlg)
	ON_EN_CHANGE(IDC_NAME, OnChangeName)
	ON_EN_CHANGE(IDC_CELL_NAME, OnChangeCellName)
	ON_EN_CHANGE(IDC_PASSWORD, OnChangePassword)
	ON_BN_CLICKED(IDHELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKlogDlg message handlers

BOOL CKlogDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	if (m_strCellName.IsEmpty()) {
		char defaultCell[256];
		long code = cm_GetRootCellName(defaultCell);
		if (code < 0)
			AfxMessageBox("Error determining root cell name.");
		else
			m_strCellName = defaultCell;
	}

	UpdateData(FALSE);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CKlogDlg::OnOK() 
{
	char *reason;

	UpdateData();

	HOURGLASS hg;

	if (kl_Authenticate(m_strCellName, m_strName, m_strPassword, &reason)) {
		AfxMessageBox(reason);
		return;
	}

	CDialog::OnOK();
}

void CKlogDlg::OnChangeName() 
{
	CheckEnableOk();
}

void CKlogDlg::OnChangeCellName() 
{
	CheckEnableOk();
}

void CKlogDlg::	CheckEnableOk()
{
	UpdateData();
	
	m_OK.EnableWindow(!m_strCellName.IsEmpty() && !m_strName.IsEmpty() && !m_strPassword.IsEmpty());
}

void CKlogDlg::OnChangePassword() 
{
	CheckEnableOk();
}

void CKlogDlg::OnHelp() 
{
	ShowHelp(m_hWnd, GET_TOKENS_HELP_ID);
}

