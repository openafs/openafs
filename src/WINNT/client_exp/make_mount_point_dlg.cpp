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
#include "make_mount_point_dlg.h"
#include "gui2fs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMakeMountPointDlg dialog


CMakeMountPointDlg::CMakeMountPointDlg(CWnd* pParent /*=NULL*/)
	: CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CMakeMountPointDlg::IDD), pParent);

	//{{AFX_DATA_INIT(CMakeMountPointDlg)
	m_nType = -1;
	//}}AFX_DATA_INIT

	m_bMade = FALSE;
}


void CMakeMountPointDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMakeMountPointDlg)
	DDX_Control(pDX, IDOK, m_Ok);
	DDX_Control(pDX, IDC_VOLUME, m_Vol);
	DDX_Control(pDX, IDC_RW, m_RW);
	DDX_Control(pDX, IDC_DIR, m_Dir);
	DDX_Control(pDX, IDC_CELL, m_Cell);
	DDX_Radio(pDX, IDC_REGULAR, m_nType);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CMakeMountPointDlg, CDialog)
	//{{AFX_MSG_MAP(CMakeMountPointDlg)
	ON_EN_CHANGE(IDC_VOLUME, OnChangeVolume)
	ON_EN_CHANGE(IDC_DIR, OnChangeDir)
	ON_EN_CHANGE(IDC_CELL, OnChangeCell)
	ON_BN_CLICKED(IDHELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMakeMountPointDlg message handlers

void CMakeMountPointDlg::OnOK() 
{
	UpdateData(TRUE);
	
	BOOL bRW = m_nType == 1;
	
	m_bMade = MakeMount(m_strDir, m_strVol, m_strCell, bRW);
		
	CDialog::OnOK();
}

void CMakeMountPointDlg::OnChangeVolume() 
{
	CString strVol;
	m_Vol.GetWindowText(strVol);
	if (strVol.GetLength() > 63) {
		MessageBeep((UINT)-1);
		m_Vol.SetWindowText(m_strVol);
	} else
		m_strVol = strVol;

	CheckEnableOk();
}

void CMakeMountPointDlg::OnChangeDir() 
{
	m_Dir.GetWindowText(m_strDir);

	CheckEnableOk();
}

void CMakeMountPointDlg::OnChangeCell() 
{
	m_Cell.GetWindowText(m_strCell);
	
	CheckEnableOk();
}

void CMakeMountPointDlg::CheckEnableOk()
{
	BOOL bEnable = FALSE;
	
	if ((m_strVol.GetLength() > 0) && (m_strDir.GetLength() > 0))
		bEnable = TRUE;

	m_Ok.EnableWindow(bEnable);
}

BOOL CMakeMountPointDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_Dir.SetWindowText(m_strDir);

	m_nType = 0;

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CMakeMountPointDlg::OnHelp() 
{
	ShowHelp(m_hWnd, MAKE_MOUNT_POINT_HELP_ID);
}

