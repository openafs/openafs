//cafs.h
/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/

//#define DOSTITLE "_AFS File System_"
//#define DOSTITLEFINISH "Finished - _AFS File System_"
#define DOSTITLE "AFS Client Console"
#define DOSTITLEFINISH "Finished - AFS Client Console"
#define APPTITLE "AFS"
#define APPTITLEFINISH "Finished - AFSD"
#ifdef DEV_IDE
#ifdef _DEBUG
#define CMDLINE "AFSD.EXE -startup"
#else
#define CMDLINE ".//release//AFSD.EXE -startup"
#endif
#else
#define CMDLINE "AFSD.PIF -startup"
#endif

#ifndef __CAFS__
#define __CAFS__
#include "share.h"

class CProgress
{
public:
	CProgress(CWnd *wnd, UINT mode);
	~CProgress();
	void Next();
	void Finish();
	void SetTitle(const char *,const char *,const char *);
private:
	CWnd *m_pWnd;
};

class CAfs
{
	friend CProgress;
public:
	const char * MountName(){return m_sMountName;}
	CAfs(){m_sDosAppName=DOSTITLE;m_sMountName="";};
	~CAfs();
	BOOL Mount(CString &msg,const char *drvLetter,const char *path);
	BOOL Create(CString &msg,CString sCompName,PROCESS_INFORMATION &procInfo);
	BOOL Authencate(CString &msg,const char * username,const char * password);
	BOOL Dismount(CString &msg,const char * drive,BOOL force=FALSE);
	BOOL StartExployer(CString &msg, const char *drive);
	BOOL Shutdown(CString &msg);
	BOOL Init(CWnd *,CString &);
	UINT TestTokenTime(CString&);
	BOOL ScanTokens(CString&);
	HWND & GetLoadWindowHandle(){return m_hAfsLoad;}
	void FinishProgress();
	BOOL CheckNet(CString &);
private:
	CString m_sMountName;
	static CString m_sDosAppName;
	STARTUPINFO m_startUpInfo;
	PROCESS_INFORMATION m_procProgBar;
	static HWND m_hAfsLoad;
	static HWND m_hAfsLoadFinish;
	static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam);
	static BOOL CALLBACK EnumWindowsProcShutdown(HWND hWnd, LPARAM lParam);
	char m_cCell[256];			//cell name

	DWORD m_dTokenEndTime;
	CTimeSpan m_tTotalSpanTime;
};

#endif

