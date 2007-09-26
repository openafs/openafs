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
#include "results_dlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CResultsDlg dialog


CResultsDlg::CResultsDlg(DWORD nHelpID, CWnd* pParent /*=NULL*/)
	: CDialog()
{
    InitModalIndirect (TaLocale_GetDialogResource (CResultsDlg::IDD), pParent);

    //{{AFX_DATA_INIT(CResultsDlg)
    // NOTE: the ClassWizard will add member initialization here
    //}}AFX_DATA_INIT

    m_nHelpID = nHelpID;
}


void CResultsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CResultsDlg)
    DDX_Control(pDX, IDC_RESULTS_LABEL, m_ResultsLabel);
    DDX_Control(pDX, IDC_LIST, m_List);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CResultsDlg, CDialog)
    //{{AFX_MSG_MAP(CResultsDlg)
    ON_BN_CLICKED(IDHELP, OnHelp)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CResultsDlg message handlers

BOOL CResultsDlg::OnInitDialog() 
{
    CDialog::OnInitDialog();
	
    SetWindowText(m_strDlgTitle);
    m_ResultsLabel.SetWindowText(m_strResultsTitle);

    ASSERT(m_Files.GetSize() == m_Results.GetSize());

    m_List.SetTabStops(118);

    for (int i = 0; i < m_Files.GetSize(); i++) {
	CString strItem = m_Files[i] + "\t" + m_Results[i];
	m_List.AddString(strItem);
    }

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void CResultsDlg::SetContents(const CString& strDlgTitle, const CString& strResultsTitle, const CStringArray& files, const CStringArray& results)
{
    m_strDlgTitle = strDlgTitle;
    m_strResultsTitle = strResultsTitle;

    m_Files.RemoveAll();
    m_Files.Copy(files);

    m_Results.RemoveAll();
    m_Results.Copy(results);
}

void CResultsDlg::OnHelp() 
{
    ShowHelp(m_hWnd, m_nHelpID);
}

