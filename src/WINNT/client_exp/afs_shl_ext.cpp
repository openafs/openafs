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
#include "winreg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#ifndef _WIN64

// 32-bit
static const IID IID_IShellExt =
    { 0xdc515c27, 0x6cac, 0x11d1, { 0xba, 0xe7, 0x0, 0xc0, 0x4f, 0xd1, 0x40, 0xd2 } };

#else

// 64-bit
static const IID IID_IShellExt =
    { 0x5f820ca1, 0x3dde, 0x11db, {0xb2, 0xce, 0x00, 0x15, 0x58, 0x09, 0x2d, 0xb5} };

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
HINSTANCE g_hInstance;

BOOL CAfsShlExt::InitInstance()
{
	// Load our translated resources
	TaLocale_LoadCorrespondingModuleByName (m_hInstance, TEXT("afs_shl_ext.dll"));

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

#ifdef COMMENT
    // This test is correct and we really do want to allow the extension to be loaded and 
    // unloaded as needed.   Unfortunately, the extension is being unloaded and never loaded
    // again which is disconcerting for many folks.  For now we will prevent the unloading 
    // until someone has time to figure out how to debug this.   
    // Jeffrey Altman - 2 Oct 2005

	if (!nCMRefCount && !nSERefCount && !nICRefCount && !nTPRefCount && !nXPRefCount)
		return S_OK;
#endif
	return S_FALSE;
}

int WideCharToLocal(LPTSTR pLocal, LPCWSTR pWide, DWORD dwChars)
{
	*pLocal = 0;
	WideCharToMultiByte( CP_ACP, 0, pWide, -1, pLocal, dwChars, NULL, NULL);
	return lstrlen(pLocal);
}

LRESULT DoRegCLSID(HKEY hKey,PTCHAR szSubKey,PTCHAR szData,PTCHAR szValue=NULL)
{
	DWORD    dwDisp;
	LRESULT  lResult;
	HKEY     thKey;
	lResult = RegCreateKeyEx(hKey, szSubKey, 0, NULL,
				 REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
				 &thKey, &dwDisp);
	if(NOERROR == lResult)
	{
		lResult = RegSetValueEx(thKey, szValue, 0, REG_SZ,
					(LPBYTE)szData, (lstrlen(szData) + 1) 
					* sizeof(TCHAR));
		RegCloseKey(thKey);
	}
	RegCloseKey(hKey);
	return lResult;
}

// by exporting DllRegisterServer, you can use regsvr.exe
STDAPI DllRegisterServer(void)
{
	HKEY     hKey;
	LRESULT  lResult;
	DWORD    dwDisp;
	TCHAR    szSubKey[MAX_PATH];
	TCHAR    szCLSID[MAX_PATH];
    TCHAR    szModule[MAX_PATH];
    LPWSTR   pwsz;
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	COleObjectFactory::UpdateRegistryAll();

	StringFromIID(IID_IShellExt, &pwsz);
	if(pwsz)
	{
		WideCharToMultiByte( CP_ACP, 0,pwsz, -1, szCLSID, sizeof(szCLSID), NULL, NULL);
		LPMALLOC pMalloc;
		CoGetMalloc(1, &pMalloc);
		if(pMalloc)
		{
			(pMalloc->Free)(pwsz);
			(pMalloc->Release)();
		}
	} else {
		return E_FAIL;
	}
    
    /*
    [HKEY_CLASSES_ROOT\CLSID\{$CLSID}\InprocServer32]
    @="Y:\\DEST\\root.client\\usr\\vice\\etc\\afs_shl_ext.dll"
    "ThreadingModel"="Apartment"
    */
    HMODULE hModule=GetModuleHandle("afs_shl_ext.dll");
	DWORD z=GetModuleFileName(hModule,szModule,sizeof(szModule));
	wsprintf(szSubKey, TEXT("CLSID\\%s\\InprocServer32"),szCLSID);
	if ((lResult=DoRegCLSID(HKEY_CLASSES_ROOT,szSubKey,szModule))!=NOERROR)
		return lResult;

	wsprintf(szSubKey, TEXT("CLSID\\%s\\InprocServer32"),szCLSID);
	if ((lResult=DoRegCLSID(HKEY_CLASSES_ROOT,szSubKey,"Apartment","ThreadingModel"))!=NOERROR)
		return lResult;

    /*
    [HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers\AFS Client Shell Extension]
    @="{EA3775F2-28BE-11D3-9C8D-00105A24ED29}"
    */

	wsprintf(szSubKey, TEXT("%s\\%s"), STR_REG_PATH, STR_EXT_TITLE);
	if ((lResult=DoRegCLSID(HKEY_LOCAL_MACHINE,szSubKey,szCLSID))!=NOERROR)
		return lResult;
	
	//If running on NT, register the extension as approved.
    /*
    [HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved]
    "{$(CLSID)}"="AFS Client Shell Extension"

    [HKEY_CLASSES_ROOT\Folder\shellex\ContextMenuHandlers\AFS Client Shell Extension]
    @="{$(CLSID)}"
    */

    OSVERSIONINFO  osvi;
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    GetVersionEx(&osvi);
    if(VER_PLATFORM_WIN32_NT == osvi.dwPlatformId)
    {
        wsprintf(szSubKey, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved"));
        if ((lResult=DoRegCLSID(HKEY_LOCAL_MACHINE,szSubKey,STR_EXT_TITLE,szCLSID))!=NOERROR)
            return lResult;
    }
    wsprintf(szSubKey, TEXT("*\\shellex\\ContextMenuHandlers\\%s"),STR_EXT_TITLE);
    if ((lResult=DoRegCLSID(HKEY_CLASSES_ROOT,szSubKey,szCLSID))!=NOERROR)
        return lResult;
    wsprintf(szSubKey, TEXT("Folder\\shellex\\ContextMenuHandlers\\%s"),STR_EXT_TITLE);
    if ((lResult=DoRegCLSID(HKEY_CLASSES_ROOT,szSubKey,szCLSID))!=NOERROR)
        return lResult;

    /*
    Register InfoTip

    [HKEY_CLASSES_ROOT\Folder\shellex\{00021500-0000-0000-C000-000000000046}]
    @="{$(CLSID)}"
    */

	wsprintf(szSubKey, TEXT("Folder\\shellex\\{00021500-0000-0000-C000-000000000046}"));
	if ((lResult=DoRegCLSID(HKEY_CLASSES_ROOT,szSubKey,szCLSID))!=NOERROR)
		return lResult;

	
	/* Below needs to be merged with above */

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

//returnValue = RegOpenKeyEx (HKEY_CLASSES_ROOT, keyName, 0, KEY_ALL_ACCESS, &registryKey);

LRESULT DoValueDelete(HKEY hKey,PTCHAR pszSubKey,PTCHAR szValue=NULL)
{
	LRESULT  lResult;
	HKEY     thKey;
	if (szValue==NULL) {
		lResult=RegDeleteKey(hKey, pszSubKey);
		return lResult;
	}
	lResult = RegOpenKeyEx( hKey,
				pszSubKey,
				0,
				KEY_ALL_ACCESS,
				&thKey);
	if(NOERROR == lResult)
	{
		lResult=RegDeleteValue(hKey, szValue);
		RegCloseKey(thKey);
	}
	return lResult;
}

STDAPI DllUnregisterServer(void)
{
	TCHAR    szSubKey[MAX_PATH];
	TCHAR    szCLSID[MAX_PATH];
	LPWSTR   pwsz;
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	COleObjectFactory::UpdateRegistryAll(FALSE);
	StringFromIID(IID_IShellExt, &pwsz);
	if(pwsz)
	{
		WideCharToMultiByte( CP_ACP, 0,pwsz, -1, szCLSID, sizeof(szCLSID), NULL, NULL);
		LPMALLOC pMalloc;
		CoGetMalloc(1, &pMalloc);
		if(pMalloc)
		{
			(pMalloc->Free)(pwsz);
			(pMalloc->Release)();
		}
	} else {
		return E_FAIL;
	}
	wsprintf(szSubKey, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved"));
	DoValueDelete(HKEY_LOCAL_MACHINE,szSubKey,szCLSID);
	wsprintf(szSubKey, TEXT("*\\shellex\\ContextMenuHandlers\\%s"),STR_EXT_TITLE);
	DoValueDelete(HKEY_CLASSES_ROOT, szSubKey);
	wsprintf(szSubKey, TEXT("Folder\\shellex\\{00021500-0000-0000-C000-000000000046}"));
	DoValueDelete(HKEY_CLASSES_ROOT, szSubKey);
	wsprintf(szSubKey, TEXT("Folder\\shellex\\ContextMenuHandlers\\%s"),STR_EXT_TITLE);
	DoValueDelete(HKEY_CLASSES_ROOT, szSubKey);
	wsprintf(szSubKey, TEXT("%s\\%s"), STR_REG_PATH, STR_EXT_TITLE);
	DoValueDelete(HKEY_LOCAL_MACHINE, szSubKey);
	return S_OK;
}

