/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
// Datalog.cpp : implementation file
//

#include "stdafx.h"
#include "WinAfsLoad.h"
#include "WinAfsLoadDlg.h"
#include "Datalog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDatalog dialog


CDatalog::CDatalog(CWnd* pParent /*=NULL*/)
	: CDialog(CDatalog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDatalog)
	m_sEdit = _T("");
	//}}AFX_DATA_INIT
	m_pParent=pParent;
}


void CDatalog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDatalog)
	DDX_Control(pDX, IDC_EDIT, m_cEdit);
	DDX_Text(pDX, IDC_EDIT, m_sEdit);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDatalog, CDialog)
	//{{AFX_MSG_MAP(CDatalog)
	ON_WM_SYSCOMMAND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDatalog message handlers

void CDatalog::PostNcDestroy() 
{
	// TODO: Add your specialized code here and/or call the base class
	
	delete this;
}

BOOL CDatalog::Create()
{
	return CDialog::Create(IDD, m_pParent);
}

void CDatalog::OnCancel()
{
	ShowWindow(SW_HIDE);
}


BOOL CDatalog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	
	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDM_CLEAR);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_CLEAR, strAboutMenu);
		}
	}
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}



// Responds to system menu calls (best make menu mod 16 value because lower 4 bits are used by os
// Select Menu item on system menu (title bar) will end up here
void CDatalog::OnSysCommand(UINT nID, LPARAM lParam)
{
	switch (nID & 0xFFF0)
	{
	case IDM_VISIBLE:	// make current application visible
		AfxGetApp()->m_pMainWnd->PostMessage(WM_COMMAND,IDM_APP_OPEN,0);
		return;
/*
			CWnd *PrevCWnd, *ChildCWnd;
			PrevCWnd=CWnd::FromHandle(m_hAfsDialog);
			DWORD s=PrevCWnd->GetStyle();
			if (!(WS_VISIBLE & s) )
				PrevCWnd->ShowWindow(SW_RESTORE);      // If iconic, restore the main window
			ChildCWnd=PrevCWnd->GetLastActivePopup(); // if so, does it have any popups?
			PrevCWnd->BringWindowToTop();             // Bring the main window to the top
			if (PrevCWnd != ChildCWnd) 
				ChildCWnd->BringWindowToTop();         // If there are popups, bring them along too!
*/
	case IDM_CLEAR:
		{
			m_sEdit.Empty();
			m_sEdit="";
			UpdateData(FALSE);
			SendMessage(WM_PAINT);
		}
		break;
	default:
		CDialog::OnSysCommand(nID, lParam);
		break;
	}
}
