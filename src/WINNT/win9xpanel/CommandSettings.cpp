/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
// CommandSettings.cpp : implementation file
//

#include "stdafx.h"
#include "winafsload.h"
#include "CommandSettings.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CCommandSettings dialog


CCommandSettings::CCommandSettings(CWnd* pParent /*=NULL*/)
	: CDialog(CCommandSettings::IDD, pParent)
{
	//{{AFX_DATA_INIT(CCommandSettings)
	m_ConnectOnStart = FALSE;
	m_LogToFile = FALSE;
	m_LogToWindow = FALSE;
	m_UserName = _T("");
	m_uMaxLoginTime = 0;
	m_uMaxPowerRestartDelay = 0;
	//}}AFX_DATA_INIT
}


void CCommandSettings::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CCommandSettings)
	DDX_Control(pDX, IDC_OPTIONLINE, m_cOptionLine);
	DDX_Control(pDX, IDC_CHECKADVANCED, m_cCheckAdvanceDisplay);
	DDX_Check(pDX, IDC_CONNECTONSTART, m_ConnectOnStart);
	DDX_Check(pDX, IDC_LOGTOFILE, m_LogToFile);
	DDX_Check(pDX, IDC_LOGTOWINDOW, m_LogToWindow);
	DDX_Text(pDX, IDC_USERNAME, m_UserName);
	DDX_Text(pDX, IDC_MAXLOGINTIME, m_uMaxLoginTime);
	DDV_MinMaxUInt(pDX, m_uMaxLoginTime, 1, 300);
	DDX_Text(pDX, IDC_POWERRESTARTDELAY, m_uMaxPowerRestartDelay);
	DDV_MinMaxUInt(pDX, m_uMaxPowerRestartDelay, 0, 120);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CCommandSettings, CDialog)
	//{{AFX_MSG_MAP(CCommandSettings)
	ON_BN_CLICKED(IDH_SETTINGS, OnSettings)
	ON_BN_CLICKED(IDC_CHECKADVANCED, OnCheckadvanced)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCommandSettings message handlers

void CCommandSettings::OnSettings() 
{
	// TODO: Add your control notification handler code here
	::WinHelp(m_hWnd,CWINAFSLOADAPP->m_pszHelpFilePath,HELP_CONTEXT,IDH_SETTINGS);
	
}

void CCommandSettings::OnCheckadvanced() 
{
	// TODO: Add your control notification handler code here
	if (m_cCheckAdvanceDisplay.GetCheck()) 
	{
		SetWindowPos(&wndTop,0,0,m_OriginalRect.right-m_OriginalRect.left,m_OriginalRect.bottom-m_OriginalRect.top,SWP_NOMOVE|SWP_NOZORDER);
	} else {
		SetWindowPos(&wndTop,0,0,m_OriginalRect.right-m_OriginalRect.left,m_DialogShrink,SWP_NOMOVE|SWP_NOZORDER);
	}
	
}

BOOL CCommandSettings::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	
	CRect rect;
	GetWindowRect(&m_OriginalRect);
	m_cOptionLine.GetWindowRect(&rect);
	m_DialogShrink=rect.top-m_OriginalRect.top+5;	//make it above the edit box
	SetWindowPos(&wndTop,0,0,m_OriginalRect.right-m_OriginalRect.left,m_DialogShrink,SWP_NOMOVE|SWP_NOZORDER);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
