// MyFrame.cpp : implementation file
// frame class used to demonstrate UI-threads
/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/

#include "stdafx.h"
#include "MyFrame.h"
#include "ProgBarDlg.h"
#include "share.h"
#include "datalog.h"
#include "WinAfsLoad.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//  CMyThread Stuff  //

HANDLE CMyUIThread::m_hEventThreadKilled=NULL;

IMPLEMENT_DYNCREATE(CMyUIThread, CWinThread)

CMyUIThread::CMyUIThread()
{
	m_pLog=NULL;
}

CMyUIThread::~CMyUIThread()
{
}

void CMyUIThread::operator delete(void* p)
{
	// The exiting main application thread waits for this event before completely
	// terminating in order to avoid a false memory leak detection.  See also
	// CMyUIThread::OnNcDestroy in bounce.cpp.

	SetEvent(m_hEventThreadKilled);
	CWinThread::operator delete(p);
}

BOOL CMyUIThread::InitInstance()
{
#ifdef _AFXDLL
//	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
//	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif
	// we use the modeless dialog box so we can create it invisable

	m_pMainWnd = new CProgBarDlg;	//we use the window pointer so MFC will delete the thread when Main window is destroyed

	if (m_pMainWnd==NULL)
	{
		AfxMessageBox("AFS - Memory allocation error",MB_OK);
		return FALSE;
	}
	if (!((CProgBarDlg *)m_pMainWnd)->Create())
	{
		AfxMessageBox("AFS - Thread Memory allocation error",MB_OK);
		return FALSE;
	}
	// Initialize sockets 
	CString msg;
	if (!((CProgBarDlg *)m_pMainWnd)->Connect(msg))
	{
		AfxMessageBox(msg,MB_OK);
		return FALSE;
	}
	m_pLog = new CDatalog();
	if (m_pLog->Create() != TRUE)
	{
		delete m_pLog;
		m_pLog=NULL;
	}
	return TRUE;
}

int CMyUIThread::ExitInstance()
{
	// TODO:  perform any per-thread cleanup here
	if (m_cPrint.m_hFile!=CFile::hFileNull)
		m_cPrint.Close();
	return CWinThread::ExitInstance();
}

BEGIN_MESSAGE_MAP(CMyUIThread, CWinThread)
	//{{AFX_MSG_MAP(CMyUIThread)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
	ON_THREAD_MESSAGE(WM_UIONPARM,OnParm)
	ON_THREAD_MESSAGE(WM_UICONNECT,OnConnect)
	ON_THREAD_MESSAGE(WM_LOG,OnLog)
END_MESSAGE_MAP()


void CMyUIThread::OnParm( UINT wp, LONG lp)
{
	switch (wp)
	{
	case ONPARMCLOSE:
		m_pMainWnd->DestroyWindow();	// this will also destroy the datalog window
		m_pLog=NULL;
		if (m_cPrint.m_hFile!=CFile::hFileNull)
			m_cPrint.Close();
		break;
	case ONPARMDISCONNECT:
		((CProgBarDlg *)m_pMainWnd)->DisConnect();
		CWINAFSLOADAPP->WSANotifyFromUI(AFS_EXITCODE_NORMAL,NULL);
		break;
	default:
		ASSERT(0);
		break;
	}

}

// these routines must either return null or point to a message
void CMyUIThread::OnConnect( UINT wParam, LONG lp)
{
	CString sStatus;
	if (((CProgBarDlg *)m_pMainWnd)->Connect(sStatus))
	{
		CWINAFSLOADAPP->NotifyFromUI(wParam,NULL);
		return;
	}
	CWINAFSLOADAPP->NotifyFromUI(wParam,sStatus);
}

void CMyUIThread::OnLog( UINT wp, LONG lp)
{
TRY
{
	switch (wp)
	{
	case 0:
		if ((lp) && (m_cPrint.m_hFile!=CFile::hFileNull))
		{
			CString *pMsg=(CString *)lp;
			m_cPrint.Write((const char *)*pMsg, lstrlen((const char *)*pMsg));
			m_cPrint.Flush();
		}
		if (m_pLog==NULL) break;
		if (lp)
		{
			CString *pMsg=(CString *)lp;
			m_pLog->m_sEdit+=*pMsg;
			delete pMsg;	//FREE up date
		} else
			m_pLog->m_sEdit="";
		m_pLog->UpdateData(FALSE);
		m_pLog->m_cEdit.LineScroll(10000);
		if ( (m_pLog->GetStyle() & (WS_VISIBLE|WS_MINIMIZE))==WS_VISIBLE)
			m_pLog->SendMessage(WM_PAINT);
		break;
	case LOGSHOWWINDOW:
		if (m_pLog==NULL) break;
		m_pLog->ShowWindow((lp)?SW_SHOWNORMAL:SW_HIDE);
		break;
	case LOGSHOWPRINT:
		if (lp==NULL)
		{
			if (m_cPrint.m_hFile!=CFile::hFileNull)
				m_cPrint.Close();
		} else {
			CString *mp=(CString *)lp;
			if (m_cPrint.Open((const char *)*mp
				,CFile::modeCreate|CFile::modeNoTruncate|CFile::modeWrite,&m_cPrintException)==NULL)
			{
				TCHAR szError[1024];
				m_cPrintException.GetErrorMessage(szError, 1024);
				LOG("Log file(%s) open error %s",(const char *)lp,szError);
				break;
			}
			m_cPrint.SeekToEnd();
			delete mp;	//this was passed as a pointer to a created CString;
			CString mt;
			SYSTEMTIME timeDest;
			GetSystemTime(&timeDest);
			mt.Format("\nNew Session:(%02d:%02d:%2d-%03d)\n",
				timeDest.wHour,
				timeDest.wMinute,
				timeDest.wSecond,
				timeDest.wMilliseconds);
			m_cPrint.Write((const char *)mt, lstrlen(mt));
		}
		break;
	default:
		break;
	}
} 
CATCH(CFileException, e)
{	
	if (m_cPrint.m_hFile!=CFile::hFileNull)
		m_cPrint.Close();
	TCHAR szError[1024];
	e->GetErrorMessage(szError, 1024);
	LOG("Open Log file error %s",szError);
}
END_CATCH
}
