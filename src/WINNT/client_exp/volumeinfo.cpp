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
#include "volume_info.h"
#include "partition_info_dlg.h"
#include "volume_inf.h"
#include "gui2fs.h"
#include <limits.h>
#include "msgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CVolumeInfo dialog


CVolumeInfo::CVolumeInfo(CWnd* pParent /*=NULL*/)
	: CDialog()
{
	InitModalIndirect (TaLocale_GetDialogResource (CVolumeInfo::IDD), pParent);

	//{{AFX_DATA_INIT(CVolumeInfo)
	m_nNewQuota = 0;
	//}}AFX_DATA_INIT
	
	m_pVolInfo = 0;
	m_nCurIndex = -1;
}

CVolumeInfo::~CVolumeInfo()
{
	if (m_pVolInfo != 0)
		delete [] m_pVolInfo;
}

void CVolumeInfo::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVolumeInfo)
	DDX_Control(pDX, IDC_QUOTA_SPIN, m_QuotaSpin);
	DDX_Control(pDX, IDOK, m_Ok);
	DDX_Control(pDX, IDC_PARTITION_INFO, m_ShowPartInfo);
	DDX_Control(pDX, IDC_LIST, m_List);
	DDX_Text(pDX, IDC_NEW_QUOTA, m_nNewQuota);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CVolumeInfo, CDialog)
	//{{AFX_MSG_MAP(CVolumeInfo)
	ON_LBN_SELCHANGE(IDC_LIST, OnSelChangeList)
	ON_BN_CLICKED(IDC_PARTITION_INFO, OnPartitionInfo)
	ON_EN_CHANGE(IDC_NEW_QUOTA, OnChangeNewQuota)
	ON_NOTIFY(UDN_DELTAPOS, IDC_QUOTA_SPIN, OnDeltaPosQuotaSpin)
	ON_BN_CLICKED(IDHELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVolumeInfo message handlers

BOOL CVolumeInfo::OnInitDialog() 
{
	ASSERT(m_Files.GetSize() > 0);

	CDialog::OnInitDialog();

	int tabs[] = { 79, 164, 214, 260, 301 };
	
	m_List.SetTabStops(sizeof(tabs) / sizeof(int), tabs);

	m_pVolInfo = new CVolInfo[m_Files.GetSize()];

	for (int i = 0; i < m_Files.GetSize(); i++) {
		GetVolumeInfo(m_Files[i], m_pVolInfo[i]);
		// Check if this is a duplicate entry (same volume as earlier entry)
		for (int j = 0; j < i; j++) {
			if (m_pVolInfo[j].m_nID == m_pVolInfo[i].m_nID)
				m_pVolInfo[i].m_nDup = j;
				break;
		}
	}

	ShowInfo();

	m_QuotaSpin.SetRange(UD_MINVAL, UD_MAXVAL);
	m_QuotaSpin.SetPos(0);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

int CVolumeInfo::GetCurVolInfoIndex()
{
	int nListIndex = m_List.GetCurSel();
	ASSERT(nListIndex >= 0);

	int nIndex = m_List.GetItemData(nListIndex);
	ASSERT(nIndex >= 0);

	if (m_pVolInfo[nIndex].m_nDup != -1)
		return m_pVolInfo[nIndex].m_nDup;

	return nIndex;
}

void CVolumeInfo::OnSelChangeList() 
{
	m_nCurIndex = GetCurVolInfoIndex();
	if (m_nCurIndex < 0)
		return;

	m_nNewQuota = m_pVolInfo[m_nCurIndex].m_nNewQuota;

	m_ShowPartInfo.EnableWindow(TRUE);
	CWnd *pNewQuota = GetDlgItem(IDC_NEW_QUOTA);
	pNewQuota->EnableWindow(TRUE);

	m_QuotaSpin.EnableWindow(TRUE);
	m_QuotaSpin.SetPos(0);

	UpdateData(FALSE);
}

void CVolumeInfo::OnPartitionInfo() 
{
	CPartitionInfoDlg dlg;

	ASSERT(m_nCurIndex >= 0);

	dlg.SetValues(m_pVolInfo[m_nCurIndex].m_nPartSize, m_pVolInfo[m_nCurIndex].m_nPartFree);

	dlg.DoModal();
}

void CVolumeInfo::SetFiles(const CStringArray& files)
{
	m_Files.RemoveAll();

	m_Files.Copy(files);
}

void CVolumeInfo::OnChangeNewQuota() 
{
	if (m_List.m_hWnd == 0)
		return;

	CString strQuota;
	GetDlgItem(IDC_NEW_QUOTA)->GetWindowText(strQuota);

	if (strQuota.IsEmpty())
		return;

	if (!UpdateData(TRUE))
		return;

	ASSERT(m_nCurIndex >= 0);

	m_pVolInfo[m_nCurIndex].m_nNewQuota = m_nNewQuota;

	if (m_pVolInfo[m_nCurIndex].m_nNewQuota != m_pVolInfo[m_nCurIndex].m_nQuota)
		m_Ok.EnableWindow(TRUE);

	ShowInfo();
}

void CVolumeInfo::OnOK() 
{
	for (int i = 0; i < m_Files.GetSize(); i++) {
		if ((m_pVolInfo[i].m_nDup == -1) && (m_pVolInfo[i].m_nQuota != m_pVolInfo[i].m_nNewQuota)) {
//			CString strQuota;
//			strQuota.Format("= %ld", m_pVolInfo[i].m_nNewQuota);
//			AfxMessageBox("Setting quota for volume: " + m_pVolInfo[i].m_strName + strQuota);
			SetVolInfo(m_pVolInfo[i]);
		}
	}
	
	CDialog::OnOK();
}

void CVolumeInfo::OnDeltaPosQuotaSpin(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
	
	LONG nNewQuota = m_nNewQuota + pNMUpDown->iDelta * 1024;
	if (nNewQuota < 0)
		return;

	m_nNewQuota = nNewQuota;
	
	UpdateData(FALSE);

	OnChangeNewQuota();

	*pResult = 0;
}

void CVolumeInfo::ShowInfo()
{
	m_List.ResetContent();
	
	for (int i = 0; i < m_Files.GetSize(); i++) {

		CString strEntry;
		if (m_pVolInfo[i].m_strErrorMsg.GetLength() > 0)
			strEntry = m_pVolInfo[i].m_strFileName + "\t(Error:  " + m_pVolInfo[i].m_strErrorMsg + ")";
		else {

			LONG nQuota;
			if (m_pVolInfo[i].m_nDup == -1)
				nQuota = m_pVolInfo[i].m_nNewQuota;
			else
				nQuota = m_pVolInfo[m_pVolInfo[i].m_nDup].m_nNewQuota;

 			if (nQuota != 0) {
				LONG nPercentUsed = (m_pVolInfo[i].m_nUsed * 100) / nQuota;
				strEntry.Format("%s\t%s\t%ld\t%ldK\t%ldK\t%ld%%", m_pVolInfo[i].m_strFileName, m_pVolInfo[i].m_strName,
					m_pVolInfo[i].m_nID, nQuota, m_pVolInfo[i].m_nUsed, nPercentUsed);
			} else {
				strEntry.Format("%s\t%s\t%ld\tUnlimited\t%ldK", m_pVolInfo[i].m_strFileName, m_pVolInfo[i].m_strName,
					m_pVolInfo[i].m_nID, m_pVolInfo[i].m_nUsed);
			}
		}

		int nListIndex = m_List.AddString(strEntry);
		ASSERT(nListIndex >= 0);
		VERIFY(m_List.SetItemData(nListIndex, i) != LB_ERR);
	}
}

void CVolumeInfo::OnHelp() 
{
	ShowHelp(m_hWnd, VOLUME_INFO_HELP_ID);
}

