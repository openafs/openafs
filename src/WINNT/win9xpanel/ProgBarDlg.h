/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#if !defined(AFX_PROGBARDLG_H__FD4CD3C1_0EBF_11D4_A374_00105A6BCA62__INCLUDED_)
#define AFX_PROGBARDLG_H__FD4CD3C1_0EBF_11D4_A374_00105A6BCA62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "afsmsg95.h"

#define MAX_PENDING_CONNECTS 4  /* The backlog allowed for listen() */

union SOCKETTYPE {
	afsMsg_hdr_t header;
	afsMsg_statChange_t change;
	afsMsg_print_t print;
};

// ProgBarDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CProgBarDlg dialog

class CProgBarDlg : public CDialog
{
// Construction
public:
	CProgBarDlg(CWnd* pParent = NULL);   // standard constructor
	~CProgBarDlg();
	BOOL Create();
	BOOL Connect(CString &);
	VOID DisConnect();
//	BOOL InitSocket(CString &);
	UINT m_uEvent;
// Dialog Data
	//{{AFX_DATA(CProgBarDlg)
	enum { IDD = IDD_PROGBAR_DIALOG };
	CStatic	m_cAuthenicate;
	CStatic	m_cMount;
	CStatic	m_cBackground;
	CButton	m_cStatusRegion;
	CStatic	m_cCompleteMount;
	CStatic	m_cCompleteEnable;
	CStatic	m_cCompleteAuth;
	CString	m_sAuthenicateTime;
	CString	m_sBackgroundTime;
	CString	m_sMountTime;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CProgBarDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;
	UINT	m_nTimer;
	INT		m_Current;
	UINT	m_Option;
	CTime m_StartTime;
	CWnd * m_pParent;
	CString	m_sDefaultAuthenicate;
	CString	m_sDefaultMount;
	CString	m_sDefaultBackground;
	// Generated message map functions
	//{{AFX_MSG(CProgBarDlg)
	virtual BOOL OnInitDialog();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg LRESULT OnParm(WPARAM, LPARAM);
	afx_msg void OnDestroy();
	afx_msg void OnPaint();
	//}}AFX_MSG
	afx_msg LRESULT OnWSAEvent(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

	SOCKET m_Socket;
	BOOL m_bReadyToSend;
	UINT m_uStatus;
//	CString m_sLog;
	UINT Decode(WPARAM wParam,CString &);
	char m_buffer[AFS_MAX_MSG_LEN+1];
	INT m_iOrientation;	//timer based orientation of timer progress object
	void SetShape(CRect &,int,int);		//set shape for progress bar
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROGBARDLG_H__FD4CD3C1_0EBF_11D4_A374_00105A6BCA62__INCLUDED_)
