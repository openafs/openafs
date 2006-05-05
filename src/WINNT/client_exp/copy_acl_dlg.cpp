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
#include "copy_acl_dlg.h"
#include "io.h"
#include "msgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CCopyAclDlg dialog


CCopyAclDlg::CCopyAclDlg(CWnd* pParent /*=NULL*/)
	: CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CCopyAclDlg::IDD), pParent);

	//{{AFX_DATA_INIT(CCopyAclDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	m_bClear = FALSE;
}


void CCopyAclDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CCopyAclDlg)
	DDX_Control(pDX, IDOK, m_Ok);
	DDX_Control(pDX, IDC_FROM_DIR, m_FromDir);
	DDX_Control(pDX, IDC_TO_DIR, m_ToDir);
	DDX_Control(pDX, IDC_CLEAR, m_Clear);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CCopyAclDlg, CDialog)
	//{{AFX_MSG_MAP(CCopyAclDlg)
	ON_EN_CHANGE(IDC_TO_DIR, OnChangeToDir)
	ON_BN_CLICKED(IDC_BROWSE, OnBrowse)
	ON_BN_CLICKED(IDHELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCopyAclDlg message handlers

void CCopyAclDlg::OnOK() 
{
	m_bClear = m_Clear.GetCheck() == 1;
	m_ToDir.GetWindowText(m_strToDir);
	
	if (access(m_strToDir, 0) == -1) {
		ShowMessageBox(IDS_DIR_DOES_NOT_EXIST_ERROR, MB_ICONEXCLAMATION, IDS_DIR_DOES_NOT_EXIST_ERROR, m_strToDir);
		return;
	}

	CDialog::OnOK();
}

BOOL CCopyAclDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	m_FromDir.SetWindowText(m_strFromDir);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CCopyAclDlg::OnChangeToDir() 
{
	m_ToDir.GetWindowText(m_strToDir);
	
	BOOL bEnable = m_strToDir.GetLength() > 0;
	m_Ok.EnableWindow(bEnable);
}

void CCopyAclDlg::OnBrowse() 
{
	CFileDialog dlg(TRUE, 0, "*.*", OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY, 0, 0);

	if (dlg.DoModal() == IDCANCEL)
		return;

	CString strPath = dlg.GetPathName();
	
	// Remove file name (last component of path)
	int nFirstSlash = strPath.Find('\\');
	int nLastSlash = strPath.ReverseFind('\\');
	if (nFirstSlash != nLastSlash)
		strPath = strPath.Left(nLastSlash);
	else
		strPath = strPath.Left(nFirstSlash + 1);

	m_ToDir.SetWindowText(strPath);
}

void CCopyAclDlg::OnHelp() 
{
	ShowHelp(m_hWnd, COPY_ACL_HELP_ID);
}

