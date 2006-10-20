/*

Copyright 2004 by the Massachusetts Institute of Technology

All rights reserved.

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the Massachusetts
Institute of Technology (M.I.T.) not be used in advertising or publicity
pertaining to distribution of the software without specific, written
prior permission.

M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
M.I.T. BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

/* afscustom.h
 *
 * Declarations for OpenAFS MSI setup tools
 *
 * rcsid : $Id: afscustom.h,v 1.2.2.1 2004/08/23 15:55:09 jaltman Exp $
 */

#ifndef __afsMsiTools_H__
#define __afsMsiTools_H__

#include<windows.h>
#include<setupapi.h>
#include<msiquery.h>
#include<stdio.h>
#include<string.h>
#include<lm.h>

#define MSIDLLEXPORT UINT __stdcall

#define CHECK(x)	if((x)) goto _cleanup

#define CHECKX(x,y) if(!(x)) { msiErr = (y); goto _cleanup; }

#define CHECK2(x,y)  if((x)) { msiErr = (y); goto _cleanup; }

#define STR_KEY_ORDER _T("SYSTEM\\CurrentControlSet\\Control\\NetworkProvider\\Order")
#define STR_VAL_ORDER _T("ProviderOrder")

#define STR_SERVICE _T("TransarcAFSDaemon")
#define STR_SERVICE_LEN 18

#define INP_ERR_PRESENT 1
#define INP_ERR_ADDED   2
#define INP_ERR_ABSENT  3
#define INP_ERR_REMOVED 4

#define ERR_NPI_FAILED 4001
#define ERR_SCC_FAILED 4002
#define ERR_SCS_FAILED 4003
#define ERR_ABORT 4004
#define ERR_NSS_FAILED 4005
#define ERR_GROUP_CREATE_FAILED 4006
#define ERR_GROUP_MEMBER_FAILED 4007

/* non-exported */
int npi_CheckAndAddRemove( LPTSTR, LPTSTR, int );
DWORD InstNetProvider(MSIHANDLE, int);
void ShowMsiError(MSIHANDLE, DWORD, DWORD);
DWORD ConfigService(int);
UINT createAfsAdminGroup(void);
UINT initializeAfsAdminGroup(void);
UINT removeAfsAdminGroup(void);

/* exported */
MSIDLLEXPORT InstallNetProvider( MSIHANDLE );
MSIDLLEXPORT UninstallNetProvider ( MSIHANDLE );
MSIDLLEXPORT ConfigureClientService( MSIHANDLE );
MSIDLLEXPORT ConfigureServerService( MSIHANDLE );
MSIDLLEXPORT AbortMsiImmediate( MSIHANDLE );
MSIDLLEXPORT UninstallNsisInstallation( MSIHANDLE hInstall );
MSIDLLEXPORT CreateAFSClientAdminGroup( MSIHANDLE hInstall );
MSIDLLEXPORT RemoveAFSClientAdminGroup( MSIHANDLE hInstall );

#endif /*__afsMsiTools_H__*/
