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
#include <winsock2.h>
#include "help.h"
#include "shell_ext.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CAfsShlExt

BEGIN_MESSAGE_MAP(CAfsShlExt, CWinApp)
	//{{AFX_MSG_MAP(CAfsShlExt)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAfsShlExt construction

CAfsShlExt::CAfsShlExt()
{
	/* Start up sockets */
	WSADATA WSAjunk;
	WSAStartup(0x0101, &WSAjunk);
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CAfsShlExt object

CAfsShlExt theApp;

/////////////////////////////////////////////////////////////////////////////
// CAfsShlExt initialization

BOOL CAfsShlExt::InitInstance()
{
	// Load our translated resources
	TaLocale_LoadCorrespondingModule (m_hInstance);

	// Register all OLE server (factories) as running.  This enables the
	//  OLE libraries to create objects from other applications.
	COleObjectFactory::RegisterAll();

	SetHelpPath(m_pszHelpFilePath);

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// Special entry points required for inproc servers

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return AfxDllGetClassObject(rclsid, riid, ppv);
}

STDAPI DllCanUnloadNow(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (!nCMRefCount && !nSERefCount && !nICRefCount && !nTPRefCount && !nXPRefCount)
		return S_OK;

	return S_FALSE;
}

// by exporting DllRegisterServer, you can use regsvr.exe
STDAPI DllRegisterServer(void)
{
	int      i;
	HKEY     hKey;
	LRESULT  lResult;
	DWORD    dwDisp;
	TCHAR    szSubKey[MAX_PATH];
	TCHAR    szCLSID[MAX_PATH];
	TCHAR    szModule[MAX_PATH];
	LPWSTR   pwsz;
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	COleObjectFactory::UpdateRegistryAll();
	wsprintf(szSubKey, TEXT("%s\\%s"), STR_REG_PATH, STR_EXT_TITLE);
	lResult = RegCreateKeyEx(  HKEY_LOCAL_MACHINE,
							szSubKey,
							0,
							NULL,
							REG_OPTION_NON_VOLATILE,
							KEY_WRITE,
							NULL,
							&hKey,
							&dwDisp);

	if(NOERROR == lResult)
	{
	//Create the value string.
		lResult = RegSetValueEx(   hKey,
								NULL,
								0,
								REG_SZ,
								(LPBYTE)szCLSID,
								(lstrlen(szCLSID) + 1) * sizeof(TCHAR));
		RegCloseKey(hKey);
	}
	else
		return SELFREG_E_CLASS;

	//If running on NT, register the extension as approved.
	OSVERSIONINFO  osvi;
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	GetVersionEx(&osvi);
	if(VER_PLATFORM_WIN32_NT == osvi.dwPlatformId)
	{
		lstrcpy( szSubKey, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved"));

		lResult = RegCreateKeyEx(  HKEY_LOCAL_MACHINE,
								szSubKey,
								0,
								NULL,
								REG_OPTION_NON_VOLATILE,
								KEY_WRITE,
								NULL,
								&hKey,
								&dwDisp);

		if(NOERROR == lResult)
		{
			TCHAR szData[MAX_PATH];

		//Create the value string.
			lstrcpy(szData, STR_EXT_TITLE);

			lResult = RegSetValueEx(   hKey,
									szCLSID,
									0,
									REG_SZ,
									(LPBYTE)szData,
									(lstrlen(szData) + 1) * sizeof(TCHAR));
	      
			RegCloseKey(hKey);
		} else
			return SELFREG_E_CLASS;
	}
	return S_OK;
}
