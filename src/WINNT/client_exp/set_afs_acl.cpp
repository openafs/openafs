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
#include "set_afs_acl.h"
#include "clear_acl_dlg.h"
#include "add_acl_entry_dlg.h"
#include "copy_acl_dlg.h"
#include "gui2fs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSetAfsAcl dialog


CSetAfsAcl::CSetAfsAcl(CWnd* pParent /*=NULL*/)
	: CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CSetAfsAcl::IDD), pParent);

	//{{AFX_DATA_INIT(CSetAfsAcl)
	//}}AFX_DATA_INIT

	m_bChanges = FALSE;
	m_nCurSel = -1;
}

void CSetAfsAcl::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSetAfsAcl)
	DDX_Control(pDX, IDC_REMOVE, m_Remove);
	DDX_Control(pDX, IDC_ADMINISTER, m_AdminPerm);
	DDX_Control(pDX, IDC_LOCK, m_LockPerm);
	DDX_Control(pDX, IDC_INSERT, m_InsertPerm);
	DDX_Control(pDX, IDC_DELETE, m_DeletePerm);
	DDX_Control(pDX, IDC_LOOKUP, m_LookupPerm);
	DDX_Control(pDX, IDC_WRITE, m_WritePerm);
	DDX_Control(pDX, IDC_READ, m_ReadPerm);
	DDX_Control(pDX, IDC_DIR_NAME, m_DirName);
	DDX_Control(pDX, IDC_NEGATIVE_ENTRIES, m_NegativeRights);
	DDX_Control(pDX, IDC_NORMAL_RIGHTS, m_NormalRights);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CSetAfsAcl, CDialog)
	//{{AFX_MSG_MAP(CSetAfsAcl)
	ON_BN_CLICKED(IDC_CLEAR, OnClear)
	ON_BN_CLICKED(IDC_ADD, OnAdd)
	ON_BN_CLICKED(IDC_COPY, OnCopy)
	ON_LBN_SELCHANGE(IDC_NORMAL_RIGHTS, OnSelChangeNormalRights)
	ON_LBN_SELCHANGE(IDC_NEGATIVE_ENTRIES, OnSelChangeNegativeEntries)
	ON_BN_CLICKED(IDC_ADMINISTER, OnPermChange)
	ON_BN_CLICKED(IDC_REMOVE, OnRemove)
	ON_BN_CLICKED(IDC_CLEAN, OnClean)
	ON_BN_CLICKED(IDC_DELETE, OnPermChange)
	ON_BN_CLICKED(IDC_INSERT, OnPermChange)
	ON_BN_CLICKED(IDC_LOCK, OnPermChange)
	ON_BN_CLICKED(IDC_LOOKUP, OnPermChange)
	ON_BN_CLICKED(IDC_READ, OnPermChange)
	ON_BN_CLICKED(IDC_WRITE, OnPermChange)
	ON_BN_CLICKED(IDHELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSetAfsAcl message handlers

BOOL CSetAfsAcl::OnInitDialog() 
{
	CDialog::OnInitDialog();

	if (!GetRights(m_strDir, m_Normal, m_Negative)) {
		EndDialog(0);
		return TRUE;
	}

	m_DirName.SetWindowText(m_strDir);

	EnablePermChanges(FALSE);

	m_NormalRights.SetTabStops(58);
	m_NegativeRights.SetTabStops(58);

        int i;
	for (i = 0; i < m_Normal.GetSize(); i += 2)
		m_NormalRights.AddString(m_Normal[i + 1] + "\t" + m_Normal[i]);

	for (i = 0; i < m_Negative.GetSize(); i += 2)
		m_NegativeRights.AddString(m_Negative[i + 1] + "\t" + m_Negative[i]);

	CenterWindow();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CSetAfsAcl::OnClear() 
{
	CClearAclDlg dlg;

	if (dlg.DoModal() == IDCANCEL)
		return;

	BOOL bNormal, bNegative, bClearRights = FALSE;

	dlg.GetSettings(bNormal, bNegative);

	if (bNormal) {
		m_Normal.RemoveAll();
		m_NormalRights.ResetContent();
		if (m_bShowingNormal)
			bClearRights = TRUE;
	}

	if (bNegative) {
		m_Negative.RemoveAll();
		m_NegativeRights.ResetContent();
		if (!m_bShowingNormal)
			bClearRights = TRUE;
	}
	
	if (bClearRights)
		OnNothingSelected();
}

void CSetAfsAcl::OnAdd() 
{
	CAddAclEntryDlg dlg;

	dlg.SetAclDlg(this);

	if (dlg.DoModal() == IDCANCEL)
		return;

	OnNothingSelected();

	CString name = dlg.GetName();
	CString rights = dlg.GetRights();
	BOOL bNormal = dlg.IsNormal();

	if (bNormal) {
		m_Normal.Add(name);
		m_Normal.Add(rights);
		m_nCurSel = m_NormalRights.AddString(rights + "\t" + name);
		m_NormalRights.SetSel(m_nCurSel);
		m_bShowingNormal = TRUE;
	} else {
		m_Negative.Add(name);
		m_Negative.Add(rights);
		m_nCurSel = m_NegativeRights.AddString(rights + "\t" + name);
		m_NegativeRights.SetSel(m_nCurSel);
		m_bShowingNormal = FALSE;
	}

	ShowRights(rights);
	EnablePermChanges(TRUE);

	m_bChanges = TRUE;
}

void CSetAfsAcl::OnCopy() 
{
	CCopyAclDlg dlg;

	dlg.SetFromDir(m_strDir);

	if (dlg.DoModal() == IDCANCEL)
		return;

	CString strToDir = dlg.GetToDir();
	BOOL bClear = dlg.GetClear();

	CopyACL(strToDir, m_Normal, m_Negative, bClear);
}

void CSetAfsAcl::ShowRights(const CString& strRights)
{
	m_ReadPerm.SetCheck((strRights.Find("r") == -1) ? UNCHECKED : CHECKED);
	m_WritePerm.SetCheck((strRights.Find("w") == -1) ? UNCHECKED : CHECKED);
	m_LookupPerm.SetCheck((strRights.Find("l") == -1) ? UNCHECKED : CHECKED);
	m_DeletePerm.SetCheck((strRights.Find("d") == -1) ? UNCHECKED : CHECKED);
	m_InsertPerm.SetCheck((strRights.Find("i") == -1) ? UNCHECKED : CHECKED);
	m_LockPerm.SetCheck((strRights.Find("k") == -1) ? UNCHECKED : CHECKED);
	m_AdminPerm.SetCheck((strRights.Find("a") == -1) ? UNCHECKED : CHECKED);
}

void CSetAfsAcl::OnSelChangeNormalRights() 
{
	m_NegativeRights.SetSel(-1, FALSE);

	m_bShowingNormal = TRUE;

	int nNum = m_NormalRights.GetSelCount();
	if (nNum != 1) {
		ShowRights("");
		EnablePermChanges(FALSE);
		return;
	}

	ASSERT(nNum == 1);		// I'm paranoid

	VERIFY(m_NormalRights.GetSelItems(1, &m_nCurSel) == 1);

	CString strRights = m_Normal[(m_nCurSel * 2) + 1];
	ShowRights(strRights);

	OnSelection();
}

void CSetAfsAcl::OnSelChangeNegativeEntries() 
{
	m_NormalRights.SetSel(-1, FALSE);

	m_bShowingNormal = FALSE;

	int nNum = m_NegativeRights.GetSelCount();
	if (nNum != 1) {
		ShowRights("");
		EnablePermChanges(FALSE);
		return;
	}

	ASSERT(nNum == 1);		// I'm paranoid

	VERIFY(m_NegativeRights.GetSelItems(1, &m_nCurSel) == 1);

	CString strRights = m_Negative[(m_nCurSel * 2) + 1];
	ShowRights(strRights);

	OnSelection();
}

CString CSetAfsAcl::MakeRightsString()
{
	CString str;

	if (m_ReadPerm.GetCheck() == CHECKED)
		str += "r";
	if (m_LookupPerm.GetCheck() == CHECKED)
		str += "l";
	if (m_InsertPerm.GetCheck() == CHECKED)
		str += "i";
	if (m_DeletePerm.GetCheck() == CHECKED)
		str += "d";
	if (m_WritePerm.GetCheck() == CHECKED)
		str += "w";
	if (m_LockPerm.GetCheck() == CHECKED)
		str += "k";
	if (m_AdminPerm.GetCheck() == CHECKED)
		str += "a";

	return str;
}

void CSetAfsAcl::OnPermChange() 
{
	CListBox *pRightsList;
	CStringArray *pRights;

	if (m_bShowingNormal) {
		pRightsList = &m_NormalRights;
		pRights = &m_Normal;
	} else {
		pRightsList = &m_NegativeRights;
		pRights = &m_Negative;
	}

	ASSERT(m_nCurSel >= 0);

	CString str = MakeRightsString();
	(*pRights)[(2 * m_nCurSel) + 1] = str;
	str += "\t" + (*pRights)[(2 * m_nCurSel)];

	pRightsList->DeleteString(m_nCurSel);
	pRightsList->InsertString(m_nCurSel, str);
	pRightsList->SetSel(m_nCurSel);

	m_bChanges = TRUE;
}

void CSetAfsAcl::OnRemove() 
{
	CListBox *pRightsList;
	CStringArray *pRights;

	if (m_bShowingNormal) {
		pRightsList = &m_NormalRights;
		pRights = &m_Normal;
	} else {
		pRightsList = &m_NegativeRights;
		pRights = &m_Negative;
	}

	int nNum = pRightsList->GetSelCount();
	if (nNum < 0) {
		ASSERT(FALSE);
		return;
	}

	if (nNum == 0) {
		ASSERT(FALSE);		// How can this ever be 0?
		ASSERT(m_nCurSel >= 0);
		pRightsList->DeleteString(m_nCurSel);
		pRights->RemoveAt(m_nCurSel * 2, 2);
	} else {
		int *pIndexes = new int[nNum];
		VERIFY(pRightsList->GetSelItems(nNum, pIndexes) != LB_ERR);

		for (int i = nNum - 1; i >= 0; i--) {
			pRightsList->DeleteString(pIndexes[i]);
			pRights->RemoveAt(pIndexes[i] * 2, 2);
		}

		delete [] pIndexes;
	}

	OnNothingSelected();

	m_bChanges = TRUE;
}

void CSetAfsAcl::EnablePermChanges(BOOL bEnable)
{
	m_ReadPerm.EnableWindow(bEnable);
	m_WritePerm.EnableWindow(bEnable);
	m_LookupPerm.EnableWindow(bEnable);
	m_DeletePerm.EnableWindow(bEnable);
	m_InsertPerm.EnableWindow(bEnable);
	m_LockPerm.EnableWindow(bEnable);
	m_AdminPerm.EnableWindow(bEnable);
}

void CSetAfsAcl::OnNothingSelected()
{
    //m_NormalRights.SetSel(-1,FALSE);	// Unselect any selected normal rights
    //m_NegativeRights.SetSel(-1,FALSE);	// Unselect any selected negative rights

    for (int i=0;i < m_NormalRights.GetCount();i++)
    {
	m_NormalRights.SetSel(i, FALSE);
    }
    for (int i=0;i < m_NegativeRights.GetCount();i++)
    {
	m_NegativeRights.SetSel(i, FALSE);
    }
    ShowRights("");				// Show no rights
    EnablePermChanges(FALSE);		// Allow no rights changes
    m_Remove.EnableWindow(FALSE);		// Disable remove button
}

void CSetAfsAcl::OnSelection()
{
	EnablePermChanges(TRUE);
	m_Remove.EnableWindow(TRUE);
}

void CSetAfsAcl::OnOK() 
{
	if (m_bChanges && !SaveACL(m_strCellName, m_strDir, m_Normal, m_Negative))
		return;
	
	CDialog::OnOK();
}

void CSetAfsAcl::OnClean() 
{
	CStringArray dirs;

	dirs.Add(m_strDir);

	CleanACL(dirs);
}

BOOL CSetAfsAcl::IsNameInUse(BOOL bNormal, const CString& strName)
{
	if (bNormal) {
		for (int i = 0; i < m_Normal.GetSize(); i += 2)
			if (m_Normal[i] == strName)
				return TRUE;

		return FALSE;
	}

	for (int i = 0; i < m_Negative.GetSize(); i += 2)
		if (m_Negative[i] == strName)
			return TRUE;

	return FALSE;
}

void CSetAfsAcl::OnHelp() 
{
	ShowHelp(m_hWnd, SET_AFS_ACL_HELP_ID);
}

