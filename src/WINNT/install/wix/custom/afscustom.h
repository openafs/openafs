/* afsMsiTools.h
 *
 * Declarations for OpenAFS MSI setup tools
 *
 * rcsid : $Id$
 */

#ifndef __afsMsiTools_H__
#define __afsMsiTools_H__

#include<windows.h>
#include<setupapi.h>
#include<msiquery.h>
#include<stdio.h>
#include<string.h>

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

/* non-exported */
int npi_CheckAndAddRemove( LPTSTR, LPTSTR, int );
DWORD InstNetProvider(MSIHANDLE, int);
void ShowMsiError(MSIHANDLE, DWORD, DWORD);
DWORD ConfigService(int);

/* exported */
MSIDLLEXPORT InstallNetProvider( MSIHANDLE );
MSIDLLEXPORT UninstallNetProvider ( MSIHANDLE );
MSIDLLEXPORT ConfigureClientService( MSIHANDLE );
MSIDLLEXPORT ConfigureServerService( MSIHANDLE );
MSIDLLEXPORT AbortMsiImmediate( MSIHANDLE );
MSIDLLEXPORT UninstallNsisInstallation( MSIHANDLE hInstall );

#endif /*__afsMsiTools_H__*/
