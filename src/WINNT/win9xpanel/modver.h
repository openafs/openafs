/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#ifndef __MODVER_H
#define __MODVER_H

#include <shlwapi.h>			//win95/98/2000 sdk
#define PACKVERSION(major,minor) MAKELONG(minor,major)

// tell linker to link with version.lib for VerQueryValue, etc.
#pragma comment(linker, "/defaultlib:version.lib")

// CModuleVersion version info about a module.
//

class CModuleVersion {
public:
   CModuleVersion();
   virtual ~CModuleVersion();
   BOOL     GetFileVersionInfo(LPCTSTR modulename,DWORD &vernum);
   BOOL DllGetVersion(LPCTSTR modulename, DLLVERSIONINFO& dvi,DWORD &vernum);
};

#endif

