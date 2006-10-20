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
#include "submounts_dlg.h"
#include "add_submount_dlg.h"
#include "msgs.h"
#include "submount_info.h"
#include "hourglass.h"
#include <WINNT\afsreg.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define	PCCHAR(str)		((char *)(const char *)(str))


/////////////////////////////////////////////////////////////////////////////
// CSubmountsDlg property page

IMPLEMENT_DYNCREATE(CSubmountsDlg, CDialog)

static CSubmountInfo *ReadSubmtInfo(const CString& strShareName)
{
	HOURGLASS hourglass;

	CSubmountInfo *pInfo = 0;

	DWORD len;

	char pathName[1024];

    HKEY hkSubmounts;
    RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                    AFSREG_CLT_OPENAFS_SUBKEY "\\Submounts",
                    0, 
                    "AFS", 
                    REG_OPTION_NON_VOLATILE,
                    KEY_READ,
                    NULL, 
                    &hkSubmounts,
                    NULL );

    DWORD dwType;
    DWORD status;
    len = sizeof(pathName);
    status = RegQueryValueEx( hkSubmounts, (LPCSTR)PCCHAR(strShareName), 0,
                              &dwType, (LPBYTE)pathName, &len);
    RegCloseKey( hkSubmounts );

	if (status || len == 0)
		return pInfo;

	pInfo = new CSubmountInfo();
	pInfo->SetShareName(strShareName);
	pInfo->SetPathName(pathName);

	return pInfo;
}

CSubmountsDlg::CSubmountsDlg() : CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CSubmountsDlg::IDD));

	//{{AFX_DATA_INIT(CSubmountsDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	m_bAddOnlyMode = FALSE;
}

CSubmountsDlg::~CSubmountsDlg()
{
	for (int i = 0; i < m_ToDo.GetSize(); i++)
		delete m_ToDo[i];

	m_ToDo.RemoveAll();
}

void CSubmountsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSubmountsDlg)
	DDX_Control(pDX, IDC_DELETE, m_Delete);
	DDX_Control(pDX, IDC_CHANGE, m_Change);
	DDX_Control(pDX, IDC_LIST, m_SubmtList);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSubmountsDlg, CDialog)
	//{{AFX_MSG_MAP(CSubmountsDlg)
	ON_BN_CLICKED(IDC_ADD, OnAdd)
	ON_BN_CLICKED(IDC_CHANGE, OnChange)
	ON_BN_CLICKED(IDC_DELETE, OnDelete)
	ON_LBN_SELCHANGE(IDC_LIST, OnSelChangeList)
	ON_BN_CLICKED(IDOK, OnOk)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSubmountsDlg message handlers

BOOL CSubmountsDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	if (m_bAddOnlyMode) {
		TCHAR szRemoteName[MAX_PATH];
		ULONG nBufSize = sizeof(szRemoteName);

		if (WNetGetConnection(m_strAddOnlyPath.Left(2), szRemoteName, &nBufSize) == ERROR_SUCCESS) {
			CString strAfsShare(szRemoteName);
			int nSlashPos = strAfsShare.ReverseFind('\\');
			if (nSlashPos > -1) {
				strAfsShare = strAfsShare.Mid(nSlashPos + 1);

				// Get the submount info for this share name
				CSubmountInfo *pInfo = ReadSubmtInfo(strAfsShare);
				CString strSharePath;
				if (pInfo != 0) {
					ASSERT_VALID(pInfo);
					strSharePath = pInfo->GetPathName();
				}

				m_strAddOnlyPath = strSharePath + m_strAddOnlyPath.Mid(2);
			}
		}

		OnAdd();
		OnOk();

		return TRUE;
	}

	if (!FillSubmtList()) {
//		ShowMessageBox(IDS_GET_CELL_LIST_ERROR);
//		EndDialog(0);
//		return TRUE;
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CSubmountsDlg::FillSubmtList()
{
	HOURGLASS hourglass;

    HKEY hkSubmounts;
    DWORD dwType;
    DWORD dwIndex;
    DWORD dwSubmounts;

    RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                    AFSREG_CLT_OPENAFS_SUBKEY "\\Submounts",
                    0, 
                    "AFS", 
                    REG_OPTION_NON_VOLATILE,
                    KEY_READ|KEY_WRITE|KEY_QUERY_VALUE,
                    NULL, 
                    &hkSubmounts,
                    NULL );

    RegQueryInfoKey( hkSubmounts,
                 NULL,  /* lpClass */
                 NULL,  /* lpcClass */
                 NULL,  /* lpReserved */
                 NULL,  /* lpcSubKeys */
                 NULL,  /* lpcMaxSubKeyLen */
                 NULL,  /* lpcMaxClassLen */
                 &dwSubmounts, /* lpcValues */
                 NULL,  /* lpcMaxValueNameLen */
                 NULL,  /* lpcMaxValueLen */
                 NULL,  /* lpcbSecurityDescriptor */
                 NULL   /* lpftLastWriteTime */
                 );


    for ( dwIndex = 0; dwIndex < dwSubmounts; dwIndex ++ ) {
        char submountName[256];
        DWORD submountNameLen = sizeof(submountName);

        RegEnumValue( hkSubmounts, dwIndex, submountName, &submountNameLen, NULL,
              &dwType, NULL, NULL);

        m_SubmtList.AddString(submountName);
    }

    RegCloseKey( hkSubmounts );

	return TRUE;
}

void CSubmountsDlg::OnDelete() 
{
	HOURGLASS hourglass;
	
	int nIndex = m_SubmtList.GetCurSel();
	ASSERT(nIndex >= 0);

	CString strSubmt;
	CString strShareName;
	m_SubmtList.GetText(nIndex, strSubmt);

	ASSERT(!strSubmt.IsEmpty());

	strShareName = strSubmt.SpanExcluding("=");

	if (ShowMessageBox(IDS_REALLY_DELETE_SUBMT, MB_YESNO | MB_ICONQUESTION, IDS_REALLY_DELETE_SUBMT, strShareName) != IDYES)
		return;

	m_SubmtList.DeleteString(nIndex);

	if (m_SubmtList.GetCount() == 0) {
		m_Delete.EnableWindow(FALSE);
		m_Change.EnableWindow(FALSE);
	}

	CSubmountInfo *pInfo = new CSubmountInfo();
	pInfo->SetShareName(strShareName);
	pInfo->SetStatus(SIS_DELETED);
	AddWork(pInfo);
}

void CSubmountsDlg::OnSelChangeList() 
{
	m_Delete.EnableWindow(TRUE);
	m_Change.EnableWindow(TRUE);
}

static BOOL AddSubmt(CSubmountInfo *pInfo)
{
	HOURGLASS hourglass;

    HKEY hkSubmounts;
    RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                    AFSREG_CLT_OPENAFS_SUBKEY "\\Submounts",
                    0, 
                    "AFS", 
                    REG_OPTION_NON_VOLATILE,
                    KEY_WRITE,
                    NULL, 
                    &hkSubmounts,
                    NULL );

    DWORD status = RegSetValueEx( hkSubmounts, PCCHAR(pInfo->GetShareName()), 0, REG_SZ,
                   (const BYTE *)PCCHAR(pInfo->GetPathName()), strlen(PCCHAR(pInfo->GetPathName())) + 1);

    RegCloseKey(hkSubmounts);
	return (status == ERROR_SUCCESS);
}

static BOOL DeleteSubmt(CSubmountInfo *pInfo)
{
	HOURGLASS hourglass;

    HKEY hkSubmounts;
    RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                    AFSREG_CLT_OPENAFS_SUBKEY "\\Submounts",
                    0, 
                    "AFS", 
                    REG_OPTION_NON_VOLATILE,
                    KEY_WRITE,
                    NULL, 
                    &hkSubmounts,
                    NULL );

    DWORD status = RegDeleteValue( hkSubmounts, PCCHAR(pInfo->GetShareName()));

    RegCloseKey(hkSubmounts);
	return (status == ERROR_SUCCESS);
}

void CSubmountsDlg::OnAdd() 
{
	CAddSubmtDlg dlg;

	if (m_bAddOnlyMode) {
		CSubmountInfo info("", m_strAddOnlyPath, SIS_ADDED);
		dlg.SetSubmtInfo(&info);
	}

	dlg.DoModal();

	CSubmountInfo *pInfo = dlg.GetSubmtInfo();
	if (!pInfo)
		return;

	m_SubmtList.AddString(pInfo->GetShareName() + "=" + pInfo->GetPathName());
	AddWork(pInfo);
}

void CSubmountsDlg::OnChange() 
{
	CAddSubmtDlg dlg;

	HOURGLASS hourglass;

	dlg.SetAddMode(FALSE);

	int nIndex = m_SubmtList.GetCurSel();
	ASSERT(nIndex >= 0);

	CString strSubmt;
	CString strShareName;
	m_SubmtList.GetText(nIndex, strSubmt);

	ASSERT(!strSubmt.IsEmpty());

	strShareName = strSubmt.SpanExcluding("=");

	CSubmountInfo *pInfo = FindWork(strShareName);
	if (pInfo != 0)
		// Make a copy we can free below
		pInfo = new CSubmountInfo(*pInfo);
	else
		pInfo = ReadSubmtInfo(strShareName);

	if (!pInfo) {
		ShowMessageBox(IDS_GET_SUBMT_INFO_ERROR, MB_ICONEXCLAMATION, IDS_GET_SUBMT_INFO_ERROR, strShareName);
		return;
	}

	dlg.SetSubmtInfo(pInfo);
	
	delete pInfo;

	if (dlg.DoModal() != IDOK)
		return;
		
	pInfo = dlg.GetSubmtInfo();

	m_SubmtList.DeleteString(nIndex);
	m_SubmtList.InsertString(nIndex,
		pInfo->GetShareName() + "=" + pInfo->GetPathName());

	AddWork(pInfo);
}

void CSubmountsDlg::AddWork(CSubmountInfo *pInfo)
{
	ASSERT_VALID(pInfo);

	HOURGLASS hourglass;

	BOOL bAdd = TRUE;

	for (int i = 0; i < m_ToDo.GetSize(); i++) {
		if (m_ToDo[i]->GetShareName() == pInfo->GetShareName()) {
			if ((pInfo->GetStatus() == SIS_DELETED) && (m_ToDo[i]->GetStatus() == SIS_ADDED))
				bAdd = FALSE;
			delete m_ToDo[i];
			m_ToDo.RemoveAt(i);
			break;	
		}
	}

	if (bAdd)
		m_ToDo.Add(pInfo);
}

BOOL CSubmountsDlg::FixSubmts()
{
	for (int i = 0; i < m_ToDo.GetSize(); i++) {
		SUBMT_INFO_STATUS status = m_ToDo[i]->GetStatus();
		if ((status == SIS_ADDED) || (status == SIS_CHANGED))
			if (!AddSubmt(m_ToDo[i]))
				return FALSE;
		if (status == SIS_DELETED)
			if (!DeleteSubmt(m_ToDo[i]))
				return FALSE;
	}

	return TRUE;
}

CSubmountInfo *CSubmountsDlg::FindWork(const CString& strShareName)
{
	for (int i = 0; i < m_ToDo.GetSize(); i++)
		if (m_ToDo[i]->GetShareName() == strShareName)
			return m_ToDo[i];

	return 0;
}

void CSubmountsDlg::WinHelp(DWORD dwData, UINT nCmd) 
{
	CDialog::WinHelp(dwData, nCmd);
}

void CSubmountsDlg::OnOk() 
{
	if (!FixSubmts())
		ShowMessageBox(IDS_SUBMT_SAVE_FAILED);

	CDialog::OnOK();
}

void CSubmountsDlg::SetAddOnlyMode(const CString& strAddOnlyPath)
{
	m_bAddOnlyMode = TRUE;
	m_strAddOnlyPath = strAddOnlyPath;
}

