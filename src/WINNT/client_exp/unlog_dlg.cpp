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

#include "unlog_dlg.h"

extern "C" {
#include <afs/auth.h>
#include <cm_config.h>
}

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CUnlogDlg dialog

CUnlogDlg::CUnlogDlg(CWnd* pParent /*=NULL*/)
	: CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CUnlogDlg::IDD), pParent);

	//{{AFX_DATA_INIT(CUnlogDlg)
	m_strCellName = _T("");
	//}}AFX_DATA_INIT
}

void CUnlogDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUnlogDlg)
	DDX_Control(pDX, IDOK, m_OK);
	DDX_Text(pDX, IDC_CELL_NAME, m_strCellName);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CUnlogDlg, CDialog)
	//{{AFX_MSG_MAP(CUnlogDlg)
	ON_EN_CHANGE(IDC_CELL_NAME, OnChangeCellName)
	ON_BN_CLICKED(IDHELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CUnlogDlg message handlers

BOOL CUnlogDlg::OnInitDialog()
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

int kl_Unlog(const CString& strCellName)
{
	struct ktc_principal server;
	int code;
	static char xreason[100];

	if (strCellName.IsEmpty())
		code = ktc_ForgetAllTokens();
	else {
		strcpy(server.cell, strCellName);
		server.instance[0] = '\0';
		strcpy(server.name, "afs");
		code = ktc_ForgetToken(&server);
	}
	
	if (code == KTC_NOCM)
		AfxMessageBox("AFS service may not have started");
	else if (code) {
		sprintf(xreason, "Unexpected error, code %d", code);
		AfxMessageBox(xreason);
	}
	
	return code;
}

void CUnlogDlg::OnChangeCellName() 
{
	UpdateData();
	
	m_OK.EnableWindow(!m_strCellName.IsEmpty());
}

void CUnlogDlg::OnOK() 
{
	if (kl_Unlog(m_strCellName))
		return;
	
	CDialog::OnOK();
}

void CUnlogDlg::OnHelp() 
{
	ShowHelp(m_hWnd, DISCARD_TOKENS_HELP_ID);
}

