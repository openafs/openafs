/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
// Change.cpp : implementation file
//

#include "stdafx.h"
#include "WinAfsLoad.h"
#include "WinAfsLoadDlg.h"
#include "Change.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CChange dialog


CChange::CChange(BOOL change,CWnd* pParent /*=NULL*/)
	: CDialog(CChange::IDD, pParent)
{
	//{{AFX_DATA_INIT(CChange)
	m_sPath = _T("");
	m_sDescription = _T("");
	m_bAuto = FALSE;
	//}}AFX_DATA_INIT
	m_bChange=change;
	m_pParent=(CWinAfsLoadDlg *)pParent;
}


void CChange::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CChange)
	DDX_Control(pDX, IDC_DESCRIPTION, m_cShare);
	DDX_Control(pDX, IDC_PERSISTENT, m_cAuto);
	DDX_Control(pDX, IDC_PATH, m_cPath);
	DDX_Control(pDX, IDC_DRIVE, m_cDrive);
	DDX_Text(pDX, IDC_PATH, m_sPath);
	DDV_MaxChars(pDX, m_sPath, 512);
	DDX_Text(pDX, IDC_DESCRIPTION, m_sDescription);
	DDV_MaxChars(pDX, m_sDescription, 12);
	DDX_Check(pDX, IDC_PERSISTENT, m_bAuto);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CChange, CDialog)
	//{{AFX_MSG_MAP(CChange)
	ON_WM_HELPINFO()
	ON_BN_CLICKED(IDC_HELPMAIN, OnHelpmain)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CChange message handlers

void CChange::OnOK() 
{
	// TODO: Add extra validation here
	m_cDrive.GetLBText(m_cDrive.GetCurSel(),m_sDrive);
	m_sDrive=m_sDrive.Left(2);
	UpdateData(TRUE);
	if (IsValidSubmountName(m_sPath)) 
	{

		if (IsValidShareName(m_sDescription)) 
		{
			CDialog::OnOK();
			return;
		}
		MessageBox("Incorrect Share name","AFS Warning",MB_OK|MB_ICONWARNING);
		m_cShare.SetFocus();
		return;
	}
	MessageBox("Incorrect Path name","AFS Warning",MB_OK|MB_ICONWARNING);
	m_cPath.SetFocus();
}

void CChange::OnCancel() 
{
	// TODO: Add extra cleanup here
	m_sDrive=m_sPath=m_sDescription="";
	CDialog::OnCancel();
}

BOOL CChange::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	CWnd *win=GetDlgItem(IDOK);
	if (m_bChange)
	{
		if (win)
			win->SetWindowText("Accept Change");
		SetWindowText("AFS Change Item");
	} else {
		if (win)
			win->SetWindowText("Add Item");
		SetWindowText("AFS Add Item");
	}
	ListDrive();
	UpdateData(FALSE);
	TCHAR szDrive[] = TEXT("*:  ");
	wsprintf(szDrive,"%2s  ",m_sDrive);
	if (m_cDrive.FindString(0,szDrive)!=CB_ERR)
		m_cDrive.SetCurSel(m_cDrive.FindString(0,szDrive));
	else {
		szDrive[3]='*';
		m_cDrive.SetCurSel(m_cDrive.FindString(0,szDrive));
	}
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

VOID CChange::ListDrive()
{
	m_pParent->BuildDriveList();
	POSITION pos=m_pParent->m_Drivelist.GetHeadPosition();
	while (pos)
	{
		m_cDrive.InsertString(-1,m_pParent->m_Drivelist.GetNext(pos));
	}
}

BOOL CChange::IsValidShareName(LPCTSTR pszShare)
{
   if (!*pszShare)
      return FALSE;
   if (stricmp(pszShare,"")==0)
	   return FALSE;					//disallow adding a share name of ""
   for ( ; *pszShare; ++pszShare)
      {
      if (!isprint(*pszShare))
         return FALSE;
      if (*pszShare == TEXT(' '))
         return FALSE;
      if (*pszShare == TEXT('\t'))
         return FALSE;
      }

   return TRUE;
}

BOOL CChange::IsValidSubmountName (LPCTSTR pszSubmount)
{
   if (!*pszSubmount)
      return FALSE;
   for ( ; *pszSubmount; ++pszSubmount)
      {
      if (!isprint(*pszSubmount))
         return FALSE;
      if (*pszSubmount == TEXT(' '))
         return FALSE;
      if (*pszSubmount == TEXT('\t'))
         return FALSE;
      }

   return TRUE;
}


BOOL CChange::OnHelpInfo(HELPINFO* pHelpInfo) 
{
	// TODO: Add your message handler code here and/or call default

	::WinHelp(m_hWnd,CWINAFSLOADAPP->m_pszHelpFilePath,HELP_CONTEXT,(m_bChange)?IDH_CHANGE:IDH_ADD);
	return TRUE;
//	return CDialog::OnHelpInfo(pHelpInfo);
}

void CChange::OnHelpmain() 
{
	// TODO: Add your control notification handler code here
	::WinHelp(m_hWnd,CWINAFSLOADAPP->m_pszHelpFilePath,HELP_CONTEXT,(m_bChange)?IDH_CHANGE:IDH_ADD);
	
}
