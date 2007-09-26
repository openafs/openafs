/*
 * Copyright (c) 2003 SkyRope, LLC
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 * 
 * - Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution.
 * - Neither the name of Skyrope, LLC nor the names of its contributors may be 
 *   used to endorse or promote products derived from this software without 
 *   specific prior written permission from Skyrope, LLC.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this code are derived from portions of the MIT
 * Leash Ticket Manager and LoadFuncs utilities.  For these portions the
 * following copyright applies.
 *
 * Copyright (c) 2003,2004 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 */

// IP Change Monitoring Functions

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>

#define USE_MS2MIT 1

#include <afs/stds.h>
#include <krb5.h>
#include <rxkad.h>
#include <afskfw.h>
#include "ipaddrchg.h"
#include "creds.h"
#include <iphlpapi.h>
#include <afs/auth.h>

#define MAXCELLCHARS   64

#ifdef USE_FSPROBE
// Cell Accessibility Functions
// based on work originally submitted to the CMU Computer Club
// by Jeffrey Hutzelman
//
// These would work great if the fsProbe interface had been 
// ported to Windows

static 
void probeComplete()
{
    fsprobe_Cleanup(1);
    rx_Finalize();
}

struct ping_params {
    unsigned short port;            // in
    int            retry_delay;     // in seconds
    int            verbose;         // in
    struct {
        int        wait;            // in seconds
        int        retry;           // in attempts
    }   host;
    int            max_hosts;       // in
    int            hosts_attempted; // out
}

// the fsHandler is where we receive the answer to the probe
static 
int fsHandler(void)
{
    ping_count = fsprobe_Results.probeNum;
    if (!*fsprobe_Results.probeOK)
    {
        ok_count++;
        if (waiting) complete();
    }
    if (ping_count == retry) 
        complete();
    return 0;
}

// ping_fs is a callback routine meant to be called from within
// cm_SearchCellFile() or cm_SearchCellDNS()
static long 
pingFS(void *ping_params, struct sockaddr_in *addrp, char *namep)
{
    int rc;
    struct ping_params * pp = (struct ping_params *) ping_params;

    if ( pp->max_hosts && pp->hosts_attempted >= pp->max_hosts )
        return 0;

    pp->hosts_attempted++;

    if (pp->port && addrp->sin_port != htons(pp->port))
        addrp->sin_port = htons(pp->port);

    rc = fsprobe_Init(1, addrp, pp->retry_delay, fsHandler, pp->verbose);
    if (rc)
    {
        fprintf(stderr, "fsprobe_Init failed (%d)\n", rc);
        fsprobe_Cleanup(1);
        return 0;
    }

    for (;;)
    {
        tv.tv_sec = pp->host.wait;
        tv.tv_usec = 0;
        if (IOMGR_Select(0, 0, 0, 0, &tv)) 
            break;
    }
    probeComplete();
    return(0);
}


static BOOL
pingCell(char *cell)
{
    int	rc;
    char rootcell[MAXCELLCHARS+1];
    char newcell[MAXCELLCHARS+1];
    struct ping_params pp;

    memset(&pp, 0, sizeof(struct ping_params));

    if (!cell || strlen(cell) == 0) {
        /* WIN32 NOTE: no way to get max chars */
        if (rc = pcm_GetRootCellName(rootcell))
            return(FALSE);
        cell = rootcell;
    }

    pp.port = 7000; // AFS FileServer
    pp.retry_delay = 10;
    pp.max_hosts = 3;
    pp.host.wait = 30;
    pp.host.retry = 0;
    pp.verbose = 1;

    /* WIN32: cm_SearchCellFile(cell, pcallback, pdata) */
    rc = pcm_SearchCellFile(cell, newcell, pingFS, (void *)&pp);
}
#endif /* USE_FSPROBE */
 
// These two items are imported from afscreds.h 
// but it cannot be included without causing conflicts
#define c100ns1SECOND        (LONGLONG)10000000
static void 
TimeToSystemTime (SYSTEMTIME *pst, time_t TimeT)
{
    struct tm *pTime;
    memset (pst, 0x00, sizeof(SYSTEMTIME));

    if ((pTime = localtime (&TimeT)) != NULL)
    {
        pst->wYear = pTime->tm_year + 1900;
        pst->wMonth = pTime->tm_mon + 1;
        pst->wDayOfWeek = pTime->tm_wday;
        pst->wDay = pTime->tm_mday;
        pst->wHour = pTime->tm_hour;
        pst->wMinute = pTime->tm_min;
        pst->wSecond = pTime->tm_sec;
        pst->wMilliseconds = 0;
    }
}

static DWORD 
GetServiceStatus(
    LPSTR lpszMachineName, 
    LPSTR lpszServiceName,
    DWORD *lpdwCurrentState) 
{ 
    DWORD           hr               = NOERROR; 
    SC_HANDLE       schSCManager     = NULL; 
    SC_HANDLE       schService       = NULL; 
    DWORD           fdwDesiredAccess = 0; 
    SERVICE_STATUS  ssServiceStatus  = {0}; 
    BOOL            fRet             = FALSE; 

    *lpdwCurrentState = 0; 
 
    fdwDesiredAccess = GENERIC_READ; 
 
    schSCManager = OpenSCManager(lpszMachineName,  
                                 NULL,
                                 fdwDesiredAccess); 
 
    if(schSCManager == NULL) 
    { 
        hr = GetLastError();
        goto cleanup; 
    } 
 
    schService = OpenService(schSCManager,
                             lpszServiceName,
                             fdwDesiredAccess); 
 
    if(schService == NULL) 
    { 
        hr = GetLastError();
        goto cleanup; 
    } 
 
    fRet = QueryServiceStatus(schService,
                              &ssServiceStatus); 
 
    if(fRet == FALSE) 
    { 
        hr = GetLastError(); 
        goto cleanup; 
    } 
 
    *lpdwCurrentState = ssServiceStatus.dwCurrentState; 
 
cleanup: 
 
    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager); 
 
    return(hr); 
} 

void
ObtainTokensFromUserIfNeeded(HWND hWnd)
{
    char * rootcell = NULL;
    char   cell[MAXCELLCHARS+1] = "";
    char   password[PROBE_PASSWORD_LEN+1];
    struct afsconf_cell cellconfig;
    struct ktc_principal    aserver;
    struct ktc_principal    aclient;
    struct ktc_token	atoken;
    krb5_timestamp now = 0;
    BOOL serverReachable = 0;
    int rc;
    DWORD       CurrentState, code;
    char        HostName[64];
    int         use_kfw = KFW_is_available();

    SYSTEMTIME stNow;
    FILETIME ftNow;
    LONGLONG llNow;
    FILETIME ftExpires;
    LONGLONG llExpires;
    SYSTEMTIME stExpires;

    CurrentState = 0;
    memset(HostName, '\0', sizeof(HostName));
    gethostname(HostName, sizeof(HostName));
    if (GetServiceStatus(HostName, TRANSARCAFSDAEMON, &CurrentState) != NOERROR)
        return;
    if (CurrentState != SERVICE_RUNNING) {
        SendMessage(hWnd, WM_START_SERVICE, FALSE, 0L);
        return;
    }

    rootcell = (char *)GlobalAlloc(GPTR,MAXCELLCHARS+1);
    if (!rootcell) 
        goto cleanup;

    code = KFW_AFS_get_cellconfig(cell, (void*)&cellconfig, rootcell);
    if (code) 
        goto cleanup;

    memset(&aserver, '\0', sizeof(aserver));
    strcpy(aserver.name, "afs");
    strcpy(aserver.cell, rootcell);

    GetLocalTime (&stNow);
    SystemTimeToFileTime (&stNow, &ftNow);
    llNow = (((LONGLONG)ftNow.dwHighDateTime) << 32) + (LONGLONG)(ftNow.dwLowDateTime);
    llNow /= c100ns1SECOND;

    rc = ktc_GetToken(&aserver, &atoken, sizeof(atoken), &aclient);
    if ( rc == 0 ) {
        TimeToSystemTime (&stExpires, atoken.endTime);
        SystemTimeToFileTime (&stExpires, &ftExpires);
        llExpires = (((LONGLONG)ftExpires.dwHighDateTime) << 32) + (LONGLONG)(ftExpires.dwLowDateTime);
        llExpires /= c100ns1SECOND;

        if (llNow < llExpires)
            goto cleanup;

        if ( IsDebuggerPresent() ) {
            char message[256];
            sprintf(message,"ObtainTokensFromUserIfNeeded: %d  now = %ul  endTime = %ul\n",
                     rc, llNow, llExpires);
            OutputDebugString(message);
        }
    }

#ifdef USE_FSPROBE
    serverReachable = cellPing(NULL);
#else
    if (use_kfw) {
        // If we can't use the FSProbe interface we can attempt to forge
        // a kinit and if we can back an invalid user error we know the
        // kdc is at least reachable
        serverReachable = KFW_probe_kdc(&cellconfig);
    } else {
        int i;

        for ( i=0 ; i<PROBE_PASSWORD_LEN ; i++ )
            password[i] = 'x';

        code = ObtainNewCredentials(rootcell, PROBE_USERNAME, password, TRUE);
        switch ( code ) {
        case INTK_BADPW:
        case KERB_ERR_PRINCIPAL_UNKNOWN:
        case KERB_ERR_SERVICE_EXP:
        case RD_AP_TIME:
            serverReachable = TRUE;
            break;
        default:
            serverReachable = FALSE;
        }
    }
#endif
    if ( !serverReachable ) {
        if ( IsDebuggerPresent() )
            OutputDebugString("Server Unreachable\n");
        goto cleanup;
    }

    if ( IsDebuggerPresent() )
        OutputDebugString("Server Reachable\n");

    if ( use_kfw ) {
#ifdef USE_MS2MIT
        KFW_import_windows_lsa();
#endif /* USE_MS2MIT */
        KFW_AFS_renew_expiring_tokens();
        KFW_AFS_renew_token_for_cell(rootcell);

        rc = ktc_GetToken(&aserver, &atoken, sizeof(atoken), &aclient);
        if ( rc == 0 ) {
            TimeToSystemTime (&stExpires, atoken.endTime);
            SystemTimeToFileTime (&stExpires, &ftExpires);
            llExpires = (((LONGLONG)ftExpires.dwHighDateTime) << 32) + (LONGLONG)(ftExpires.dwLowDateTime);
            llExpires /= c100ns1SECOND;
        
            if (llNow < llExpires)
                goto cleanup;
        }
    }

    SendMessage(hWnd, WM_OBTAIN_TOKENS, FALSE, (long)rootcell);
    rootcell = NULL;    // rootcell freed by message receiver

  cleanup:
    if (rootcell)
        GlobalFree(rootcell);

    return;
}

DWORD
GetNumOfIpAddrs(void)
{
    PMIB_IPADDRTABLE pIpAddrTable = NULL;
    ULONG            dwSize;
    DWORD            code;
    DWORD            index;
    DWORD            validAddrs = 0;

    dwSize = 0;
    code = GetIpAddrTable(NULL, &dwSize, 0);
    if (code == ERROR_INSUFFICIENT_BUFFER) {
        pIpAddrTable = malloc(dwSize);
        code = GetIpAddrTable(pIpAddrTable, &dwSize, 0);
        if ( code == NO_ERROR ) {
            for ( index=0; index < pIpAddrTable->dwNumEntries; index++ ) {
                if (pIpAddrTable->table[index].dwAddr != 0)
                    validAddrs++;
            }
        }
        free(pIpAddrTable);
    }
    return validAddrs;
}

void
IpAddrChangeMonitor(void * hWnd)
{
#ifdef USE_OVERLAPPED
    HANDLE Handle = INVALID_HANDLE_VALUE;   /* Do Not Close This Handle */
    OVERLAPPED Ovlap;
#endif /* USE_OVERLAPPED */
    DWORD Result;
    DWORD prevNumOfAddrs = GetNumOfIpAddrs();
    DWORD NumOfAddrs;
    char message[256];

    if ( !hWnd )
        return;

    while ( TRUE ) {
#ifdef USE_OVERLAPPED
        ZeroMemory(&Ovlap, sizeof(OVERLAPPED));

        Result = NotifyAddrChange(&Handle,&Ovlap);
        if (Result != ERROR_IO_PENDING)
        {        
            if ( IsDebuggerPresent() ) {
                sprintf(message, "NotifyAddrChange() failed with error %d \n", Result);
                OutputDebugString(message);
            }
            break;
        }

        if ((Result = WaitForSingleObject(Handle,INFINITE)) != WAIT_OBJECT_0)
        {
            if ( IsDebuggerPresent() ) {
                sprintf(message, "WaitForSingleObject() failed with error %d\n",
                        GetLastError());
                OutputDebugString(message);
            }
            continue;
        }

        if (GetOverlappedResult(Handle, &Ovlap,
                                 &DataTransfered, TRUE) == 0)
        {
            if ( IsDebuggerPresent() ) {
                sprintf(message, "GetOverlapped result failed %d \n",
                        GetLastError());
                OutputDebugString(message);
            }
            break;
        }
#else
        Result = NotifyAddrChange(NULL,NULL);
        if (Result != NO_ERROR)
        {        
            if ( IsDebuggerPresent() ) {
                sprintf(message, "NotifyAddrChange() failed with error %d \n", Result);
                OutputDebugString(message);
            }
            break;
        }
#endif
        
        NumOfAddrs = GetNumOfIpAddrs();

        if ( IsDebuggerPresent() ) {
            sprintf(message,"IPAddrChangeMonitor() NumOfAddrs: now %d was %d\n",
                    NumOfAddrs, prevNumOfAddrs);
            OutputDebugString(message);
        }

        if ( NumOfAddrs != prevNumOfAddrs ) {
            // Give AFS Client Service a chance to notice and die
            // Or for network services to startup
            Sleep(2000);
            // this call should probably be mutex protected
            ObtainTokensFromUserIfNeeded(hWnd);
        }
        prevNumOfAddrs = NumOfAddrs;
    }
}


DWORD 
IpAddrChangeMonitorInit(HWND hWnd)
{
    DWORD status = ERROR_SUCCESS;
    HANDLE thread;
    ULONG  threadID = 0;

    thread = CreateThread(NULL, 0, (PTHREAD_START_ROUTINE)IpAddrChangeMonitor,
                                    hWnd, 0, &threadID);

    if (thread == NULL) {
        status = GetLastError();
    }
    CloseHandle(thread);
    return status;
}

