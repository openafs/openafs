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
#include "mount_points_dlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMountPointsDlg dialog


CMountPointsDlg::CMountPointsDlg(CWnd* pParent /*=NULL*/)
	: CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CMountPointsDlg::IDD), pParent);

	//{{AFX_DATA_INIT(CMountPointsDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CMountPointsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMountPointsDlg)
	DDX_Control(pDX, IDC_LIST, m_List);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CMountPointsDlg, CDialog)
	//{{AFX_MSG_MAP(CMountPointsDlg)
	ON_BN_CLICKED(IDHELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMountPointsDlg message handlers

BOOL CMountPointsDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	int tabs[] = { 64, 145, 220 };
	
	m_List.SetTabStops(sizeof(tabs) / sizeof(int), tabs);

	for (int i = 0; i < m_MountPoints.GetSize(); i++)
		m_List.AddString(m_MountPoints[i]);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CMountPointsDlg::SetMountPoints(const CStringArray& mountPoints)
{
	m_MountPoints.RemoveAll();

	m_MountPoints.Copy(mountPoints);
}

void CMountPointsDlg::OnHelp() 
{
	ShowHelp(m_hWnd, MOUNT_POINTS_HELP_ID);
}

