/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#include "StdAfx.h"
#include "ModVer.h"

CModuleVersion::CModuleVersion()
{
}

//////////////////
// Destroy: delete version info
//
CModuleVersion::~CModuleVersion()
{
}

BOOL CModuleVersion::GetFileVersionInfo(LPCTSTR modulename,DWORD &vernum)
{
	vernum=0;
   TCHAR filename[_MAX_PATH];
   HMODULE hModule = ::GetModuleHandle(modulename);
   DWORD dwHandle; 
   if (hModule==NULL && modulename!=NULL)
      return FALSE;
   DWORD len = GetModuleFileName(hModule, filename,
      sizeof(filename)/sizeof(filename[0]));
   if (len <= 0)
      return FALSE;
   len = GetFileVersionInfoSize(filename, &dwHandle);
   if (len <= 0)
      return FALSE;

   BYTE * pVersionInfo = new BYTE[len]; // allocate version info
   if (!::GetFileVersionInfo(filename, 0, len, pVersionInfo))
      return FALSE;
   VS_FIXEDFILEINFO *pfixed;
   UINT iLen;
   if (!VerQueryValue(pVersionInfo, _T("\\"), (LPVOID *)&pfixed, &iLen))
      return FALSE;
	vernum=pfixed->dwFileVersionMS;
	delete pVersionInfo;
   return pfixed->dwSignature == VS_FFI_SIGNATURE;
}

typedef HRESULT (CALLBACK* DLLGETVERSIONPROC)(DLLVERSIONINFO *);

BOOL CModuleVersion::DllGetVersion(LPCTSTR modulename, DLLVERSIONINFO& dvi,DWORD &vernum)
{
   HINSTANCE hinst = LoadLibrary(modulename);
	vernum=0;
   if (!hinst)
      return FALSE;

   DLLGETVERSIONPROC pDllGetVersion =
      (DLLGETVERSIONPROC)GetProcAddress(hinst, _T("DllGetVersion"));

   if (!pDllGetVersion)
      return FALSE;

   memset(&dvi, 0, sizeof(dvi));        // clear
   dvi.cbSize = sizeof(dvi);            // set size for Windows
	BOOL ret=SUCCEEDED((*pDllGetVersion)(&dvi));
	vernum=MAKELONG(dvi.dwMinorVersion,dvi.dwMajorVersion);
	return ret;
}
