/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

// server_status_dlg.cpp : implementation file
//

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "stdafx.h"
#include "server_status_dlg.h"
#include "gui2fs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CServerStatusDlg property Dlg

IMPLEMENT_DYNCREATE(CServerStatusDlg, CDialog)

CServerStatusDlg::CServerStatusDlg() : CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CServerStatusDlg::IDD));

	//{{AFX_DATA_INIT(CServerStatusDlg)
	m_bFast = FALSE;
	m_nCell = -1;
	//}}AFX_DATA_INIT
}

CServerStatusDlg::~CServerStatusDlg()
{
}

void CServerStatusDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CServerStatusDlg)
	DDX_Control(pDX, IDC_SHOWSTATUS, m_ShowStatus);
	DDX_Control(pDX, IDC_CELL_NAME, m_CellName);
	DDX_Check(pDX, IDC_DONTPROBESERVERS, m_bFast);
	DDX_Radio(pDX, IDC_LOCALCELL, m_nCell);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CServerStatusDlg, CDialog)
	//{{AFX_MSG_MAP(CServerStatusDlg)
	ON_BN_CLICKED(IDC_SHOWSTATUS, OnShowStatus)
	ON_BN_CLICKED(IDC_SPECIFIEDCELL, OnSpecifiedCell)
	ON_BN_CLICKED(IDC_LOCALCELL, OnLocalCell)
	ON_BN_CLICKED(IDC_ALL_CELLS, OnAllCells)
	ON_EN_CHANGE(IDC_CELL_NAME, OnChangeCellName)
	ON_BN_CLICKED(IDHELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CServerStatusDlg message handlers
BOOL CServerStatusDlg::Save()
{
	return FALSE;
}

BOOL CServerStatusDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	m_CellName.EnableWindow(FALSE);
	m_nCell = 0;

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CServerStatusDlg::OnShowStatus() 
{
	UpdateData(TRUE);

	CheckServers(GetCellNameText(), (WHICH_CELLS)m_nCell, m_bFast);
}

void CServerStatusDlg::OnSpecifiedCell() 
{
	m_CellName.EnableWindow(TRUE);	

	CheckEnableShowStatus();
}

void CServerStatusDlg::OnLocalCell() 
{
	m_CellName.EnableWindow(FALSE);
	m_ShowStatus.EnableWindow(TRUE);
}

void CServerStatusDlg::OnAllCells() 
{
	m_CellName.EnableWindow(FALSE);
	m_ShowStatus.EnableWindow(TRUE);
}

void CServerStatusDlg::CheckEnableShowStatus()
{
	m_ShowStatus.EnableWindow(GetCellNameText().GetLength() > 0);
}

void CServerStatusDlg::OnChangeCellName() 
{
	CheckEnableShowStatus();
}

CString CServerStatusDlg::GetCellNameText()
{
	CString strCellName;

	m_CellName.GetWindowText(strCellName);

	return strCellName;
}

void CServerStatusDlg::OnHelp() 
{
	ShowHelp(m_hWnd, SERVER_STATUS_HELP_ID);
}

