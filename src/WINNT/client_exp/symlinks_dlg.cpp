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
#include "symlinks_dlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSymlinksDlg dialog


CSymlinksDlg::CSymlinksDlg(CWnd* pParent /*=NULL*/)
	: CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CSymlinksDlg::IDD), pParent);

	//{{AFX_DATA_INIT(CSymlinksDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CSymlinksDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSymlinksDlg)
	DDX_Control(pDX, IDC_LIST, m_List);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSymlinksDlg, CDialog)
	//{{AFX_MSG_MAP(CSymlinksDlg)
	ON_BN_CLICKED(IDHELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSymlinksDlg message handlers

BOOL CSymlinksDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	int tabs[] = { 64 };
	
	m_List.SetTabStops(sizeof(tabs) / sizeof(int), tabs);

	for (int i = 0; i < m_Symlinks.GetSize(); i++)
		m_List.AddString(m_Symlinks[i]);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CSymlinksDlg::SetSymlinks(const CStringArray& symlinks)
{
	m_Symlinks.RemoveAll();

	m_Symlinks.Copy(symlinks);
}

void CSymlinksDlg::OnHelp() 
{
	ShowHelp(m_hWnd, SYMLINK_HELP_ID);
}

