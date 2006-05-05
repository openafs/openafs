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
#include "auth_dlg.h"
#include "klog_dlg.h"
#include "unlog_dlg.h"
#include "gui2fs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAuthDlg dialog


CAuthDlg::CAuthDlg(CWnd* pParent /*=NULL*/)
	: CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CAuthDlg::IDD), pParent);

	//{{AFX_DATA_INIT(CAuthDlg)
	//}}AFX_DATA_INIT
}


void CAuthDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAuthDlg)
	DDX_Control(pDX, IDC_TOKEN_LIST, m_TokenList);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAuthDlg, CDialog)
	//{{AFX_MSG_MAP(CAuthDlg)
	ON_BN_CLICKED(ID_GET_TOKENS, OnGetTokens)
	ON_BN_CLICKED(ID_DISCARD_TOKENS, OnDiscardTokens)
	ON_BN_CLICKED(IDHELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAuthDlg message handlers

BOOL CAuthDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	// The final tab is for an extra CellName entry that is 
	// there so we can easily parse it out when someone wants
	// to get or discard tokens.  It is placed so the user can't
	// see it.  It's kind of a hack, but not too bad.
	int tabs[] = { 93, 211, 700 };
	
	m_TokenList.SetTabStops(sizeof(tabs) / sizeof(int), tabs);

	FillTokenList();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CAuthDlg::OnGetTokens() 
{
	CKlogDlg dlg;

	dlg.SetCellName(GetSelectedCellName());

	if (dlg.DoModal() == IDOK)
		FillTokenList();
}

void CAuthDlg::OnDiscardTokens() 
{
	CUnlogDlg dlg;

	dlg.SetCellName(GetSelectedCellName());

	if (dlg.DoModal() == IDOK)
		FillTokenList();
}

void CAuthDlg::FillTokenList()
{
	m_TokenList.ResetContent();
	
	CStringArray tokenInfo;
	if (!GetTokenInfo(tokenInfo))
		return;

	for (int i = 0; i < tokenInfo.GetSize(); i++)
		m_TokenList.AddString(tokenInfo[i]);
}

CString CAuthDlg::GetSelectedCellName()
{
	int nIndex = m_TokenList.GetCurSel();
	if (nIndex < 0)
		return "";

	CString strTokenInfo;
	m_TokenList.GetText(nIndex, strTokenInfo);

	int nPos = strTokenInfo.ReverseFind('\t');

	return strTokenInfo.Mid(nPos + 1);
}

void CAuthDlg::OnHelp() 
{
	ShowHelp(m_hWnd, AUTHENTICATION_HELP_ID);
}

