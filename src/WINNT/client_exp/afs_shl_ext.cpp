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

	if (!nCMRefCount && !nSERefCount)
		return S_OK;

	return S_FALSE;
}

// by exporting DllRegisterServer, you can use regsvr.exe
STDAPI DllRegisterServer(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	COleObjectFactory::UpdateRegistryAll();
	return S_OK;
}
