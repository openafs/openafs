/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/

#include "stdafx.h"
#ifdef _MFC_VER
extern "C" {
#endif
#include <afs\param.h>
#include <afs\stds.h>
#include <afs\kautils.h>
#include "cm_config.h"
#include "cmd.h"
#ifdef _MFC_VER
	}
#endif
#include "api95.h"
#include "cregkey.h"
#include "cafs.h"
#include <Svrapi.h>
#include "afsmsg95.h"

#include "WinAfsLoad.h"

#define AFS_KERBEROS_ENV

#ifndef KABADARGUMENT
#undef KABADARGUMENT
#define KABADARGUMENT 1
#endif
#define KLOGEXIT(code) exit(code)

#define LENDIR 128

//#define _DEBUG	 

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define HOURSLEFT 24

CProgress::CProgress(CWnd *wnd, UINT mode)
{
	m_pWnd=wnd;
	// you cannot obtain the handle to the window int these routines so you have enough time to create the window
	PROGRESS(wnd,mode);
}

CProgress::~CProgress()
{
	PROGRESS(NULL,ProgFHide);
}

void CProgress::SetTitle(const char *t1,const char *t2,const char *t3)
{
	char *msg=new char[strlen(t1)+strlen(t2)+strlen(t3)+3];
	strcpy(msg,t1);
	strcpy(msg+strlen(t1)+1,t2);
	strcpy(msg+strlen(t1)+strlen(t2)+2,t3);
	PROGRESS(msg,ProgFSetTitle);
}

void CProgress::Next()
{
	PROGRESS(m_pWnd,ProgFNext);
}

void CProgress::Finish()
{
	PROGRESS(NULL,ProgFHide);
}


CAfs::~CAfs()
{
	// let the OS kill the progress bar thread
}

void CAfs::FinishProgress()
{
	PROGRESS(NULL,ProgFHide);
}

BOOL CALLBACK CAfs::EnumWindowsProcShutdown(HWND hWnd, LPARAM lParam)
{
	CWnd *wnd=CWnd::FromHandle(hWnd);
	CString msg;
	CString sFinish;
	wnd->GetWindowText(msg);
	if (msg==m_sDosAppName)
		m_hAfsLoad=hWnd;
	sFinish.Format("Finished - %s",m_sDosAppName);
	if (msg==sFinish)
		m_hAfsLoadFinish=hWnd;
	return ((m_hAfsLoad==0) && (m_hAfsLoadFinish==0));
}

BOOL CAfs::Init(CWnd *wnd,CString &msg)
{
	ka_Init(0);
	return TRUE;
}

HWND CAfs::m_hAfsLoad;
HWND CAfs::m_hAfsLoadFinish;
CString CAfs::m_sDosAppName;

BOOL CALLBACK CAfs::EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
	CWnd *wnd=CWnd::FromHandle(hWnd);
	CString msg;
	wnd->GetWindowText(msg);
	DWORD dwProcessID;
	::GetWindowThreadProcessId (hWnd, &dwProcessID);
	if (dwProcessID == (DWORD) lParam)
	{
		if (msg.IsEmpty()) return TRUE;
		m_hAfsLoad=hWnd;
		return FALSE;
	}
	return TRUE;
}

BOOL CAfs::Create(CString &msg,CString sCompName,PROCESS_INFORMATION &procInfo)
{
	DWORD rc;
// lets make sure there are no other instances of the client	
	m_hAfsLoadFinish=m_hAfsLoad=0;
	EnumWindows (EnumWindowsProcShutdown, (LPARAM) 0);	//lets prevent multiple instances!
	if (m_hAfsLoad)
		Shutdown(msg);
	else if (m_hAfsLoadFinish)	// SOME REASON THE WINDOW WAS NOT SHUT DOWN, SO LETES KILL IT
	{
		::SendMessage(m_hAfsLoadFinish,WM_CLOSE,0,0);
		LOG("WM_CLOSE");
	}
	CRegkey regkey("AFS\\Window");
	UINT logintime=0;
	DWORD size=sizeof(logintime);
	regkey.GetBinary("LoginTime",(LPBYTE)&logintime,size);
	if (logintime<=0) {
#ifdef _DEBUG
		logintime=10;
#else
		logintime=40;
#endif
		regkey.PutBinary("LoginTime",(LPBYTE)&logintime,size);
	}
	int Pipe = 0;
	GetStartupInfo(&m_startUpInfo);
	m_startUpInfo.lpTitle = DOSTITLE;
	m_startUpInfo.wShowWindow = (CWINAFSLOADAPP->m_bShowAfs) ?SW_SHOW :SW_HIDE;//SW_SHOWMINIMIZED;//SW_HIDE;
	m_startUpInfo.dwFlags|=STARTF_USESHOWWINDOW;
	CWINAFSLOADAPP->SetNotifyEventSuspend();	//this needs to be done before createprocess, incase notification occurs before WaitSuspend
	LOG("Start AFSD Creation");
	rc = CreateProcess(NULL /*appName 16bit apps should be null*/,CMDLINE, NULL, NULL, FALSE,
                     /*DETACHED_PROCESS |*/ CREATE_NEW_CONSOLE | HIGH_PRIORITY_CLASS /*NORMAL_PRIORITY_CLASS*/,
                     NULL, NULL, &m_startUpInfo, &procInfo);

	if (!rc) {
		LPVOID lpMsgBuf;
		FormatMessage( 
			FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM | 
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			(LPTSTR) &lpMsgBuf,
			0,
			NULL 
		);
		msg.Format("Error creating AFS Client Console process, Status=%s.",lpMsgBuf);
		return FALSE;
	}
	LOG("AFSD Creation done - wait for notification");
	WPARAM wp;
	CWINAFSLOADAPP->WaitForEvent(logintime*1000,&wp,&msg);
	switch (wp)

	{
	case AFS_EXITCODE_NORMAL:
		// extract machine name for logon
		msg=msg.Right(msg.GetLength()-msg.Find('#')-1);
		m_sMountName.Format("\\\\%s",msg.Left(msg.ReverseFind('#')));
		if (m_sMountName=="\\\\")
		{
			m_sMountName.Format("\\\\%s-AFS",sCompName.Left(11));
		}
		break;
	default:
		return FALSE;
	}
	EnumWindows (EnumWindowsProc, (LPARAM) procInfo.dwProcessId);
	CString name;
	int len=0;
	if (m_hAfsLoad)
	{
		len=GetWindowText(m_hAfsLoad,name.GetBuffer(64),63);
		m_sDosAppName=name;
	}
	if (name==DOSTITLE) return TRUE;
	if ((len==0)||(name==DOSTITLEFINISH) || (name==APPTITLEFINISH))
	{
		msg="AFS Client Console did not start properly!";
		CString temp;
		Shutdown(temp);
		return FALSE;
	}
	regkey.PutString("Title",name);
	return TRUE;
}

BOOL CAfs::CheckNet(CString &msg)
{
	TCHAR szDrive[] = TEXT("*:");
	TCHAR szMapping[ MAX_PATH ] = TEXT("");
 	LPTSTR pszSubmount = szMapping;
//	TCHAR scan[MAX_PATH];
    DWORD dwSize = MAX_PATH;
//	TCHAR pBuffer[MAX_PATH];
//	wsprintf(scan, "\\\\%s", compname);
	for (TCHAR chDrive = 'D'; chDrive <= 'Z'; ++chDrive)
	{
		szDrive[0]=chDrive;
		if (WNetGetConnection (szDrive, szMapping, &dwSize) == NO_ERROR)
		{
			CHAR *p=strstr(szMapping,m_sMountName);
			if (p)
			{
				p=strrchr(szMapping,'\\')+1;
				if (!Dismount(msg,szDrive,FALSE))
				{
					msg.Format("Drive %s has Open Files",szDrive);
					return FALSE;
				}
				Mount(msg,szDrive,p);
			}
		}
	}
	return TRUE;
}


BOOL CAfs::Mount(CString &msg,const char *drvLetter,const char *path)
{
	char resource[256];
	NETRESOURCE nr;
	int oundHome=0;
	int Pipe = 0;
	CWait wait;		//simple way to set cursor busy
	memset(&nr, 0, sizeof(NETRESOURCE));
	ASSERT(strlen(m_sMountName)!=0);
	wsprintf(resource, "%s\\%s", m_sMountName,path);
	nr.dwType = RESOURCETYPE_DISK;
	nr.lpProvider = NULL;
	nr.lpLocalName = (char *)drvLetter;
	nr.lpRemoteName = resource;
	switch (WNetAddConnection2(&nr, 0, 0, 0))
	{
	case ERROR_ACCESS_DENIED:
		msg.Format("Access to the network resource was denied, cannot connect '%s%s'",drvLetter,path);
		return FALSE;
	case ERROR_ALREADY_ASSIGNED: 
		msg.Format("Drive %s specified by the path, '%s', is already connected to a network resource",drvLetter,path);
		return FALSE;
	case ERROR_BAD_DEVICE: 
		msg.Format("Path '%s' is invalid",path);
		return FALSE;
	case ERROR_BAD_NET_NAME: 
		msg.Format("Path '%s' is not acceptable to any network resource provider, either because the resource name is invalid, or because the named resource cannot be located",path);
		return FALSE;
	case ERROR_BAD_PROFILE: 
		msg.Format("The user profile is in an incorrect format, cannot add '%s%s'",drvLetter,path);
		return FALSE;
	case ERROR_BUSY: 
		msg.Format("The router or provider is busy, possibly initializing. Please retry, cannot add %s%s",drvLetter,path);
		return FALSE;
	case ERROR_INVALID_PASSWORD: 
		msg.Format("The specified password is invalid, cannot add '%s%s'",drvLetter,path);
		return FALSE;
	case NO_ERROR:
		return TRUE;
	default:
		msg.Format("Network error adding '%s%s'",drvLetter,path);
		return FALSE;
	}
	return TRUE;
}

BOOL CAfs::Dismount(CString &msg,const char *drvLetter,BOOL force)
{
	int Pipe = 0;
	DWORD ret=-1;
	CWait wait;		//simple way to set cursor busy
	ret=WNetCancelConnection2(drvLetter,CONNECT_UPDATE_PROFILE,force);
	switch (ret)
	{
	case ERROR_DEVICE_IN_USE:
		msg.Format("A device is in use and '%s' cannot be disconnected",drvLetter);
		return FALSE;
	case ERROR_OPEN_FILES:
		msg.Format("There are open files on '%s' and cannot be disconnected",drvLetter);
		return FALSE;
	case ERROR_NOT_CONNECTED:
	case NO_ERROR:
	default:
		break;
	}
	return TRUE;
}

BOOL CAfs::Authencate(CString &msg,const char * name,const char * password)
{
	DWORD rc;
	char instance[8];
	char realm[256];
	int lifetime;
	char *reason;
	int oundHome=0;
	long password_expires;
	int Pipe = 0;
	cm_GetRootCellName(m_cCell);
	strcpy(realm, m_cCell);
	lifetime = 0;
	instance[0] = 0;
    rc = ka_UserAuthenticateGeneral (KA_USERAUTH_VERSION,
                                    (PCHAR)name, instance, realm, (PCHAR)password, lifetime,
                                    &password_expires, 0, &reason);
	if (rc) {
		if (strstr(reason,"skewed"))
		{
			char buf[32];
			if (GetEnvironmentVariable("TZ",buf,32))
			{
				msg.Format("Unable to authenticate to AFS because:\n '%s'.\nCheck Enviornment Variable 'TZ=%s', either remove it (See Autoexec.bat) or set it correctly.",reason,buf);
				return FALSE;
			}
		}
		msg.Format("Unable to authenticate to AFS because:\n '%s'.", reason);
		return FALSE;
	}

#ifndef AFS_KERBEROS_ENV
	if (writeTicketFile) {
		code = krb_write_ticket_file (realm);
		if (!Silent) {
			if (code) 
				com_err (rn, code, "writing Kerberos ticket file");
			else {
				m_pParent->Log("Wrote ticket file to /tmp");
			}
		}
	}
#endif
	m_tTotalSpanTime=0;			//reset total time to 0 so ScanTokens will reset it
	return ScanTokens(msg);
}

BOOL CAfs::StartExployer(CString &msg,const char *udrive)
{
	DWORD rc;
	PROCESS_INFORMATION procInfo;
	STARTUPINFO startUpInfo;
	int Pipe = 0;
	int code;
	WIN32_FIND_DATA FileData;
	CString dir;
	if ((udrive==NULL) || (strlen(udrive)==0)) return TRUE;
	dir.Format("%s\\*",udrive);
	HANDLE hFile= FindFirstFile(
		dir,               // file name
		&FileData);
	if (hFile==INVALID_HANDLE_VALUE)
	{
		msg.Format("Unable to open Explorer, Drive '%s' is connected but not accessable.",udrive);
		return FALSE;
	}
	FindClose(hFile);
	char wdir[LENDIR+1];
	code = GetWindowsDirectory(wdir, sizeof(wdir));
    
	if (code == 0 || code > sizeof(wdir))
      strcpy(wdir, "c:\\windows");
	CString cmdLine;
	cmdLine.Format("%s\\explorer.exe /n,/e,%s\\",wdir,udrive);
	GetStartupInfo(&startUpInfo);
    startUpInfo.lpTitle = NULL;
    startUpInfo.wShowWindow = SW_SHOWNORMAL;
    rc = CreateProcess(NULL /*appName*/
		,cmdLine.GetBuffer(cmdLine.GetLength())
		, NULL, NULL, FALSE
		,DETACHED_PROCESS | NORMAL_PRIORITY_CLASS
		,NULL, NULL, &startUpInfo, &procInfo);
	if (rc!=0) return TRUE;
	LPVOID lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
	    FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
	    NULL,
		GetLastError(),
	    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
	    0,
		NULL 
	);
	msg.Format("Unable to open Explorer, %s.",lpMsgBuf);
	LocalFree( lpMsgBuf );
	return FALSE;
}

BOOL CAfs::Shutdown(CString &msg)
{
	CWINAFSLOADAPP->ClearNotify();
	ShutdownAfs();
	m_hAfsLoadFinish=0;
	CTime startTime = CTime::GetCurrentTime();
	CTimeSpan elapsedTime;
	do 
	{
		m_hAfsLoad=0;
		EnumWindows (EnumWindowsProcShutdown, (LPARAM) 0);	//lets prevent multiple instances!
		Sleep(500);
		elapsedTime= CTime::GetCurrentTime() - startTime;
		if (elapsedTime.GetTotalSeconds()>10) 
		{
			msg="AFS Client Console was not terminated properly!";
			::ShowWindow(m_hAfsLoad,SW_SHOWNORMAL);
			return FALSE;
		}
	} while (m_hAfsLoad);
	if (m_hAfsLoadFinish==0)
	{
		Sleep(250);	//maybe it takes a little time between process kill and window display??
		EnumWindows (EnumWindowsProcShutdown, (LPARAM) 0);	//lets prevent multiple instances!
	}
	if (m_hAfsLoadFinish)	// SOME REASON THE WINDOW WAS NOT SHUT DOWN, SO LETES KILL IT
	{
		::SendMessage(m_hAfsLoadFinish,WM_CLOSE,0,0);
		LOG("WM_CLOSE2");
	}
	return TRUE;
}


/*
	struct afsconf_cell {
		char name[MAXCELLCHARS];            Cell name
		short numServers;                   Num active servers for the cell
		short flags;						useful flags
		struct sockaddr_in hostAddr[MAXHOSTSPERCELL]; IP addresses for cell's servers
		char hostName[MAXHOSTSPERCELL][MAXHOSTCHARS]; Names for cell's servers
		char *linkedCell;                   Linked cell name, if any
	}
*/

BOOL CAfs::ScanTokens(CString &msg)
{
	int cellNum;
	int rc;
	struct ktc_principal serviceName, clientName;
	struct ktc_token token;
	cellNum = 0;
	msg="";
	while (1) {
		rc = ktc_ListTokens(cellNum, &cellNum, &serviceName);
		if (rc == KTC_NOENT) {
			return TRUE;
			break;
		}
		else if (rc == KTC_NOCM) {
			return TRUE;
		}
		else if (rc) {
			msg.Format("AFS Scan token error - Unexpected error, code 0x%0x\n", rc);
			return FALSE;
		}
		else {
			rc = ktc_GetToken(&serviceName, &token, sizeof(token),
					  &clientName);
			if (rc) {
				msg.Format("Unexpected error, service %s.%s.%s, code 0x%0x\n",
					serviceName.name, serviceName.instance,
					serviceName.cell, rc);
				return FALSE;
			}
			m_dTokenEndTime=token.endTime;
			if (m_tTotalSpanTime.GetTotalSeconds()==0)
				m_tTotalSpanTime=::CTime(m_dTokenEndTime)-CTime::GetCurrentTime();
		}
	}
	return TRUE;
}

UINT CAfs::TestTokenTime(CString &msg)
{
	CTimeSpan timeleft=::CTime(m_dTokenEndTime)-CTime::GetCurrentTime();
#ifdef _DEBUG
	DWORD dt=timeleft.GetTotalMinutes();
	DWORD dl=m_tTotalSpanTime.GetTotalMinutes();
	if (timeleft.GetTotalMinutes()<=m_tTotalSpanTime.GetTotalMinutes()-2)
	{
		msg="Authenication Expired: No Time left.";
		return 2;
	}
	msg.Format("Authenication will expire in Hours:%d Minutes:%d",timeleft.GetTotalHours()
		,timeleft.GetTotalMinutes()-timeleft.GetTotalHours()*60);
	return (timeleft.GetTotalMinutes()>(m_tTotalSpanTime.GetTotalMinutes()-1))?0:1;
#else
	if (timeleft.GetTotalMinutes()<=0)
	{
		msg="Authenication Expired: No Time left.";
		return 2;
	}
	if (
		(timeleft.GetTotalMinutes()>=(m_tTotalSpanTime.GetTotalMinutes()/7)-1)
		&& (timeleft.GetTotalMinutes()<=(m_tTotalSpanTime.GetTotalMinutes()/7)+1)
		)
	{
		LOG("TimeLeft %d Total Span time %d",timeleft.GetTotalMinutes()
			,m_tTotalSpanTime.GetTotalMinutes());
		return (ScanTokens(msg))?0:-1;
	}
	msg.Format("Authenication will expire in Hours:%d Minutes:%d",timeleft.GetTotalHours()
		,timeleft.GetTotalMinutes()-timeleft.GetTotalHours()*60);
	return (timeleft.GetTotalMinutes()>(m_tTotalSpanTime.GetTotalMinutes()/7))?0:1;
#endif
}

