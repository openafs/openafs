// WinAfsLoad.cpp : Defines the class behaviors for the application.
//
/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/

#include "stdafx.h"
#include "WinAfsLoad.h"
#include "WinAfsLoadDlg.h"
#include "cregkey.h"
#include "api95.h"
#include "termwarn.h"
#include "myframe.h"
#include "afsmsg95.h"
#include "Tlhelp32.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CWinAfsLoadApp

BEGIN_MESSAGE_MAP(CWinAfsLoadApp, CWinApp)
	//{{AFX_MSG_MAP(CWinAfsLoadApp)
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
	ON_THREAD_MESSAGE(WM_UIONPARM,OnNotifyReturn)
	ON_THREAD_MESSAGE(WSA_EVENT, OnAfsEvent)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWinAfsLoadApp construction

CWinAfsLoadApp::CWinAfsLoadApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CWinAfsLoadApp object

CWinAfsLoadApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CWinAfsLoadApp initialization

HWND CWinAfsLoadApp::m_hAfsLoadFinish=0;
HWND CWinAfsLoadApp::m_hAfsLoad=0;
CString CWinAfsLoadApp::m_sDosAppName;
/*
		DWORD dwProcessID;
		::GetWindowThreadProcessId (hWnd, &dwProcessID);
*/
BOOL CALLBACK CWinAfsLoadApp::EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
	CWnd *wnd=CWnd::FromHandle(hWnd);
	CString msg;
	CString sFinish;
	wnd->GetWindowText(msg);
	if ( (!m_sDosAppName.IsEmpty()) 
		&& (m_sDosAppName!=""))
	{
		if (msg==m_sDosAppName)
			m_hAfsLoad=hWnd;
		sFinish.Format("Finished - %s",m_sDosAppName);
		if (msg==sFinish)
			m_hAfsLoadFinish=hWnd;
	}
	if (msg==DOSTITLE)
		m_hAfsLoad=hWnd;
	if ((msg==DOSTITLEFINISH)||(msg==APPTITLEFINISH))
		m_hAfsLoadFinish=hWnd;
	return ((m_hAfsLoad==0) && (m_hAfsLoadFinish==0));
}

int CWinAfsLoadApp::ExitInstance() 
{
	// TODO: Add your specialized code here and/or call the base class
	if (m_pCMyUIThread)
	{
		SetNotifyEventSuspend();
		m_pCMyUIThread->PostThreadMessage(WM_UIONPARM,ONPARMCLOSE,0);
		WaitForSingleObject(CMyUIThread::m_hEventThreadKilled, INFINITE);
		CloseHandle(CMyUIThread::m_hEventThreadKilled);
		m_pCMyUIThread=NULL;
	}
	return CWinApp::ExitInstance();
}

BOOL CWinAfsLoadApp::InitInstance()
{
	HWND hwnd=FindWindow(NULL,"AFS Activity Log");
	CString msg;
	msg.LoadString(IDD_ACTIVITYLOG);
	if (hwnd)
	{
		::PostMessage(hwnd,WM_SYSCOMMAND,IDM_VISIBLE,0);
		return FALSE;
	}
	EnumWindows (CWinAfsLoadApp::EnumWindowsProc, (LPARAM) 0);	//lets grab other open windows and close them down
	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	PCHAR pPath=getenv("AFSCONF");
	m_sTargetDir.Empty();
	if (!pPath)
		AfxMessageBox("AFS Error - Improper installation, Enviornemnt variable AFSCONF missing",MB_ICONWARNING | MB_OK);
	else 
		m_sTargetDir.Format("%s\\",pPath);

//	CString t(m_sTargetDir);
//	t+="afs-light.hlp";

	m_pszHelpFilePath=_tcsdup(_T("afswin9x.hlp"));
// initialize variables
	CRegkey regkey("AFS\\Window");
	if (regkey.Getkey()!=0)
	{
		DWORD len=64;
		regkey.GetString("Title",m_sDosAppName,len);
	}
	RegOptions(TRUE);

	if ((m_pCMyUIThread=(CMyUIThread *)AfxBeginThread(RUNTIME_CLASS(CMyUIThread)),THREAD_PRIORITY_IDLE)==NULL)
	{// need to create a very low priority else it will interfere with other windows
		AfxMessageBox("Error creating background thread for progress bar! /nApplicaton still operational.");
		m_pCMyUIThread=NULL;
	} else
		m_pCMyUIThread->m_hEventThreadKilled	//used to test when thread is done
			= CreateEvent(NULL, FALSE, FALSE, NULL); // auto reset, initially reset
	// scan for command line options
	// show userid: password: connect
	CharLower(m_lpCmdLine);
	m_bShowAfs=(strstr(m_lpCmdLine,"show")!=NULL);
	m_bConnect=(strstr(m_lpCmdLine,"connect")!=NULL);
	if (m_bLogWindow)
		ShowLog(TRUE);
	if (m_bLog)
		ShowPrint(TRUE);
	char *str=NULL;
	CString cUserName;
	strlwr(m_lpCmdLine);	//convert to lower case
	if ((str=strstr(m_lpCmdLine,"userid:"))!=NULL)	// 
	{
		cUserName=(strstr(str,":"));
	}
	CString cPassWord;
	if ((str=strstr(m_lpCmdLine,"password:"))!=NULL)
	{
		cPassWord=(strstr(str,":"));
	}
	m_bNoID=(strstr(m_lpCmdLine,"noid")!=NULL);

	if (m_hAfsLoadFinish)	// SOME REASON THE WINDOW WAS NOT SHUT DOWN, SO LETES KILL IT
	{
		::SendMessage(m_hAfsLoadFinish,WM_CLOSE,0,0);
		LOG("WM_CLOSE");
	}
	if (m_hAfsLoad)	// Something wrong, BACKGROUND is running without dialog
	{
		CTermWarn *pModeless = new CTermWarn(NULL);
		pModeless->Create();
		pModeless->SetActiveWindow();
		ShutdownAfs();
		CTime startTime = CTime::GetCurrentTime();
		CTimeSpan elapsedTime;
		do 
		{
			Sleep(500);
			m_hAfsLoad=0;
			EnumWindows (EnumWindowsProc, (LPARAM) 0);	//lets prevent multiple instances!
			elapsedTime= CTime::GetCurrentTime() - startTime;
			if (elapsedTime.GetTotalSeconds()>10) 
			{
				if (pModeless)
					pModeless->OnCancel();
				::ShowWindow(m_hAfsLoad,SW_SHOWNORMAL);
				::AfxMessageBox("AFS Error - Warning AFS Client Console still running.\n Locate the console and use Control C to manually terminate it!");
				return FALSE;
			}
		} while (m_hAfsLoad);
		if (m_hAfsLoadFinish)	// SOME REASON THE WINDOW WAS NOT SHUT DOWN, SO LETES KILL IT
		{
			::SendMessage(m_hAfsLoadFinish,WM_CLOSE,0,0);
			LOG("WM_CLOSE");
		}
		if (pModeless)
			pModeless->OnCancel();
	}
	CWinAfsLoadDlg dlg(cUserName,cPassWord);
	m_pMainWnd = &dlg;
	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return TRUE;
}

BOOL CWinAfsLoadApp::RegOptions(BOOL fetch)
{
	CRegkey regkey("AFS\\Window");
	if (regkey.Getkey()==0) return FALSE;
	DWORD w=0;
	DWORD len=sizeof(w);
	if (fetch)
	{
		regkey.GetBinary("CommandSettings",(LPBYTE)(&w),len);
		m_bLogWindow=((w & 2)!=0);
		m_bLog=((w & 4)!=0);
		m_bConnect=((w & 1)!=0);
	} else
	{
		w=((m_bLogWindow)?2:0) | ((m_bLog)?4:0) | ((m_bConnect)?1:0);
		regkey.PutBinary("CommandSettings",(LPBYTE)(&w),sizeof(DWORD));
	}
	return TRUE;
}

void CWinAfsLoadApp::Log(const char *s)
{
	TRACE("%s\n",s);
	if ((!m_bLogWindow) && (!m_bLog)) return;
	if (s)
	{
		SYSTEMTIME timeDest;
		GetSystemTime(&timeDest);
		CString *mt=new CString;	//release is handled by CMyUIThread
		mt->Format("(%02d:%02d:%2d-%03d)%s\r\n",
			timeDest.wHour,
			timeDest.wMinute,
			timeDest.wSecond,
			timeDest.wMilliseconds,s);
		m_pCMyUIThread->PostThreadMessage(WM_LOG,0,(LPARAM)mt);
	}
	else
		m_pCMyUIThread->PostThreadMessage(WM_LOG,0,0);
}

void CWinAfsLoadApp::Log(const char *f,const DWORD d)
{
	CString msg;
	msg.Format(f,d);
	Log(msg);
}

void CWinAfsLoadApp::Log(const char *f,const DWORD d,const DWORD d2)
{
	CString msg;
	msg.Format(f,d,d2);
	Log(msg);
}

void CWinAfsLoadApp::Log(const char *f,const char*s)
{
	CString msg;
	msg.Format(f,s);
	Log(msg);
}

void CWinAfsLoadApp::Log(const char *f,const char*s,const char*s2)
{
	CString msg;
	msg.Format(f,s,s2);
	Log(msg);
}

void CWinAfsLoadApp::Log(const char *f,const char*s,const char*s2,const char*s3)
{
	CString msg;
	msg.Format(f,s,s2,s3);
	Log(msg);
}

void CWinAfsLoadApp::ShowLog(BOOL yes)
{
	m_pCMyUIThread->PostThreadMessage(WM_LOG,LOGSHOWWINDOW,yes);
}

void CWinAfsLoadApp::ShowPrint(BOOL yes)
{
	CString *mp=new CString();
	mp->Format("%s\\afscli.log",getenv("AFSCONF"));
	m_pCMyUIThread->PostThreadMessage(WM_LOG,LOGSHOWPRINT,(yes)?(LPARAM)mp:NULL);
}

void CWinAfsLoadApp::Progress(VOID *ptr, UINT mode)
{
	// you cannot obtain the handle to the window int these routines so you have enough time to create the window
	// It is ok to postmessage to a window in a different thread!
	if (m_pCMyUIThread==NULL) return;
	switch (mode)
	{
	case ProgFSetTitle:
		::PostMessage(((CDialog *)m_pCMyUIThread->GetMainWnd())->GetSafeHwnd()
			,WM_PROGRESSPARM,ProgFSetTitle,(LPARAM)ptr);
		break;
	case ProgFHide:
		::PostMessage(((CDialog *)m_pCMyUIThread->GetMainWnd())->GetSafeHwnd()
			,WM_PROGRESSPARM,ProgFHide,0);
		break;
	case ProgFNext:
		::PostMessage(((CDialog *)m_pCMyUIThread->GetMainWnd())->GetSafeHwnd(),WM_PROGRESSPARM,ProgFNext,0);
		BringWindowToTop(((CDialog *)m_pCMyUIThread->GetMainWnd())->GetSafeHwnd());
		break;
	default:
		::PostMessage(((CDialog *)m_pCMyUIThread->GetMainWnd())->GetSafeHwnd(),WM_PROGRESSPARM,ProgFOpt,mode);
		BringWindowToTop(((CDialog *)m_pCMyUIThread->GetMainWnd())->GetSafeHwnd());
		((CDialog *)m_pCMyUIThread->GetMainWnd())->CenterWindow((CWnd *)ptr);
	case 0:
		break;
	}
}

void CWinAfsLoadApp::RequestUISuspendEvent(UINT mode)
{
	SetNotifyEventSuspend();
	m_pCMyUIThread->PostThreadMessage(WM_UIONPARM,mode,0);
}

void CWinAfsLoadApp::RequestUIPostEvent(UINT mode,WPARAM wp,LPARAM lp)
{
	SetNotifyEventPostMessage();
	m_pCMyUIThread->PostThreadMessage(mode,wp,lp);
}


/*
There is no queue for messages (only one event) therefore messages passed to us
are not queued and stored in m_sMsg & m_wParam;
*/

void CWinAfsLoadApp::SetNotifyEventSuspend()
{
	ResetEvent(CMyUIThread::m_hEventThreadKilled);
	m_uNotifyMessage=1;
}

void CWinAfsLoadApp::WaitForEvent(UINT logintime,WPARAM *wp,CString *msg)
{
	switch (WaitForSingleObject(CMyUIThread::m_hEventThreadKilled, logintime))
	{
	case WAIT_OBJECT_0:
		LOG("OnAfsEvent - Receive Good Status ");
		*msg=m_sMsg;
		break;
	default:
		*wp=AFS_EXITCODE_GENERAL_FAILURE;
		*msg="AFS Client timed out, failure to respond\r\nCheck Network Connection.";
		LOG("OnAfsEvent %s",(const char *)*msg);
		return;
	}
	*wp=m_wParam;
}

/*
Tthis funciton is called from UIThread (different thread) 
	and should not contain and Windows stuff assocaited with CWinAFSLoad.
It is used to display messages from a backgound process or during PowerSuspend denial
NOTICE: PostThreadMessage leaves deletion of message up to caller
        WaitEvent does not have to delete message because Wait is not queued like PostMessage
*/
void CWinAfsLoadApp::WSANotifyFromUI(WPARAM wp,const char *msg)
{	
	TRACE("CWinAfsLoadApp::Notify wp=%d,msg=[%s]\n",wp,msg);
	switch (m_uNotifyMessage)
	{
	case 2:
		{// for post message use allocated memory to hold results and pass back to main thread
		CString *mp=new CString;		//memory is released by destination routine!
		*mp=msg;
		PostThreadMessage(WSA_EVENT,wp,(LPARAM)mp);	//this goes to WSA_EVENT
		}
		break;
	case 1:
		// For setting the Event completion, pass information back to calling thread
		m_wParam=wp;
		m_sMsg=msg;	
		SetEvent(CMyUIThread::m_hEventThreadKilled);
		m_uNotifyMessage=0;
		break;
	default:
		break;
	}
}

void CWinAfsLoadApp::NotifyFromUI(WPARAM wp,const char *msg)
{// called from UIThread to notify return status
	CString *mp=NULL;
	if (msg)
	{
		mp=new CString;		//memory is released by destination routine!
		*mp=msg;
		TRACE("CWinAfsLoadApp::Notify wp=%d,msg=[%s]\n",wp,msg);
	} else
		TRACE("CWinAfsLoadApp::Notify wp=%d,msg=NULL\n",wp);
	if (wp)
		PostThreadMessage(WM_UIONPARM,wp,(LPARAM)mp);
}

LRESULT CWinAfsLoadApp::OnNotifyReturn(WPARAM wParam, LPARAM lParam)
{
	if (m_pMainWnd)
		return ((CWinAfsLoadDlg *)m_pMainWnd)->OnNotifyReturn(wParam,lParam);
	return TRUE;
}

LRESULT CWinAfsLoadApp::OnAfsEvent(WPARAM wParam, LPARAM lParam)
{
	if (m_pMainWnd)
		return ((CWinAfsLoadDlg *)m_pMainWnd)->OnAfsEvent(wParam,lParam);
	return TRUE;
}

