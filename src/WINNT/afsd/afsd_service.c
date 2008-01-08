
#include <afs/stds.h>

#include <windows.h>
#include <softpub.h>
#include <psapi.h>
#include <winerror.h>
#include <string.h>
#include <setjmp.h>
#include "afsd.h"
#include "afsd_init.h"
#include "lanahelper.h"
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <WINNT\afsreg.h>

#include <osi.h>

#ifdef DEBUG
//#define NOTSERVICE
#endif
#ifdef _DEBUG
#include <crtdbg.h>
#endif
#include "afsdifs.h"

//#define REGISTER_POWER_NOTIFICATIONS 1
#include "afsd_flushvol.h"

extern void afsi_log(char *pattern, ...);

static SERVICE_STATUS		ServiceStatus;
static SERVICE_STATUS_HANDLE	StatusHandle;

HANDLE hAFSDMainThread = NULL;
#ifdef AFSIFS
HANDLE hAFSDWorkerThread[WORKER_THREADS];
#endif

HANDLE WaitToTerminate;

static int GlobalStatus;

#ifdef JUMP
unsigned int MainThreadId;
jmp_buf notifier_jmp;
#endif /* JUMP */

extern int traceOnPanic;
extern HANDLE afsi_file;

static int powerEventsRegistered = 0;
extern int powerStateSuspended = 0;

/*
 * Notifier function for use by osi_panic
 */
static void afsd_notifier(char *msgp, char *filep, long line)
{
#ifdef AFSIFS
    int i;
#endif
    if (!msgp)
        msgp = "unspecified assert";

    if (filep)
    	LogEvent(EVENTLOG_ERROR_TYPE, MSG_SERVICE_ERROR_STOP_WITH_MSG_AND_LOCATION, 
                 filep, line, msgp);
    else
	LogEvent(EVENTLOG_ERROR_TYPE, MSG_SERVICE_ERROR_STOP_WITH_MSG, msgp);

    GlobalStatus = line;

    osi_LogEnable(afsd_logp);

    afsd_ForceTrace(TRUE);
    buf_ForceTrace(TRUE);

    afsi_log("--- begin dump ---");
    cm_MemDumpDirStats(afsi_file, "a", 0);
    cm_MemDumpBPlusStats(afsi_file, "a", 0);
    cm_DumpCells(afsi_file, "a", 0);
    cm_DumpVolumes(afsi_file, "a", 0);
    cm_DumpSCache(afsi_file, "a", 0);
#ifdef keisa
    cm_dnlcDump(afsi_file, "a");
#endif
    cm_DumpBufHashTable(afsi_file, "a", 0);
    smb_DumpVCP(afsi_file, "a", 0);			
    afsi_log("--- end   dump ---");
    
#ifdef DEBUG
    if (IsDebuggerPresent())
        DebugBreak();	
#endif

    SetEvent(WaitToTerminate);
#ifdef AFSIFS
    WaitForMultipleObjects(WORKER_THREADS, hAFSDWorkerThread, TRUE, INFINITE);
    for (i = 0; i < WORKER_THREADS; i++)
        CloseHandle(hAFSDWorkerThread[i]);
#endif

#ifdef JUMP
    if (GetCurrentThreadId() == MainThreadId)
        longjmp(notifier_jmp, 1);
#endif /* JUMP */

    ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    ServiceStatus.dwWin32ExitCode = NO_ERROR;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint = 0;
    ServiceStatus.dwControlsAccepted = 0;
    SetServiceStatus(StatusHandle, &ServiceStatus);

    exit(1);
}

/*
 * For use miscellaneously in smb.c; need to do better
 */
static int _stdcall DummyMessageBox(HWND h, LPCTSTR l1, LPCTSTR l2, UINT ui)
{
    return 0;
}

DWORD
afsd_ServiceFlushVolume(DWORD dwlpEventData)
{
    DWORD   dwRet = ERROR_NETWORK_BUSY; /* or NO_ERROR */

    /*
    **  If UI bit is not set, user interaction is not possible
    **      BUT, since we are a NON-interactive service, and therefore
    **  have NO user I/O, it doesn't much matter.
    **  This benign code left here as example of how to find this out
    */
    BOOL bUI = (dwlpEventData & 1);

    /* flush volume */
    if ( PowerNotificationThreadNotify() )
    {
        dwRet = NO_ERROR;
    }
    else
    {
        /* flush was unsuccessful, or timeout - deny shutdown */
        dwRet = ERROR_NETWORK_BUSY;
    }

    /*      to deny hibernate, simply return
    //      any value besides NO_ERROR.
    //      For example:
    //      dwRet = ERROR_NETWORK_BUSY;
    */

    return dwRet;
}


/* service control handler used in nt4 only for backward compat. */
VOID WINAPI 
afsd_ServiceControlHandler(DWORD ctrlCode)
{
    HKEY parmKey;
    DWORD dummyLen, doTrace;
    long code;

    switch (ctrlCode) {
    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:
        ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        ServiceStatus.dwWin32ExitCode = NO_ERROR;
        ServiceStatus.dwCheckPoint = 1;
        ServiceStatus.dwWaitHint = 30000;
        ServiceStatus.dwControlsAccepted = 0;
        SetServiceStatus(StatusHandle, &ServiceStatus);

        if (ctrlCode == SERVICE_CONTROL_STOP)
            afsi_log("SERVICE_CONTROL_STOP");
        else
            afsi_log("SERVICE_CONTROL_SHUTDOWN");

        /* Write all dirty buffers back to server */
	if ( !lana_OnlyLoopback() )
	    buf_CleanAndReset();

        /* Force trace if requested */
        code = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                             AFSREG_CLT_SVC_PARAM_SUBKEY,
                             0, KEY_QUERY_VALUE, &parmKey);
        if (code != ERROR_SUCCESS)
            goto doneTrace;

        dummyLen = sizeof(doTrace);
        code = RegQueryValueEx(parmKey, "TraceOnShutdown",
                                NULL, NULL,
                                (BYTE *) &doTrace, &dummyLen);
        RegCloseKey (parmKey);
        if (code != ERROR_SUCCESS)
            doTrace = 0;
        if (doTrace) {
            afsd_ForceTrace(FALSE);
            buf_ForceTrace(FALSE);
        }

      doneTrace:
        SetEvent(WaitToTerminate);
        break;

    case SERVICE_CONTROL_INTERROGATE:
        ServiceStatus.dwCurrentState = SERVICE_RUNNING;
        ServiceStatus.dwWin32ExitCode = NO_ERROR;
        ServiceStatus.dwCheckPoint = 0;
        ServiceStatus.dwWaitHint = 0;
        ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
        SetServiceStatus(StatusHandle, &ServiceStatus);
        break;
        /* XXX handle system shutdown */
        /* XXX handle pause & continue */
    }
}       


/*
**    Extended ServiceControlHandler that provides Event types
**    for monitoring Power events, for example.
*/
DWORD WINAPI
afsd_ServiceControlHandlerEx(
              DWORD  ctrlCode,
              DWORD  dwEventType,
              LPVOID lpEventData,
              LPVOID lpContext
              )
{
    HKEY parmKey;
    DWORD dummyLen, doTrace;
    long code;
    DWORD dwRet = ERROR_CALL_NOT_IMPLEMENTED;
    OSVERSIONINFO osVersion;

    /* Get the version of Windows */
    memset(&osVersion, 0x00, sizeof(osVersion));
    osVersion.dwOSVersionInfoSize = sizeof(osVersion);
    GetVersionEx(&osVersion);

    switch (ctrlCode) 
    {
    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:
	if (ctrlCode == SERVICE_CONTROL_SHUTDOWN)
	    afsi_log("SERVICE_CONTROL_SHUTDOWN");
	else
            afsi_log("SERVICE_CONTROL_STOP");

        ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        ServiceStatus.dwWin32ExitCode = NO_ERROR;
        ServiceStatus.dwCheckPoint = 1;
        ServiceStatus.dwWaitHint = 30000;
        ServiceStatus.dwControlsAccepted = 0;
        SetServiceStatus(StatusHandle, &ServiceStatus);

        /* Write all dirty buffers back to server */
	if ( !lana_OnlyLoopback() )
	    buf_CleanAndReset();

        /* Force trace if requested */
        code = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                            AFSREG_CLT_SVC_PARAM_SUBKEY,
                            0, KEY_QUERY_VALUE, &parmKey);
        if (code != ERROR_SUCCESS)
            goto doneTrace;

        dummyLen = sizeof(doTrace);
        code = RegQueryValueEx(parmKey, "TraceOnShutdown",
                               NULL, NULL,
                               (BYTE *) &doTrace, &dummyLen);
        RegCloseKey (parmKey);
        if (code != ERROR_SUCCESS)
            doTrace = 0;
        if (doTrace) {
            afsd_ForceTrace(FALSE);
            buf_ForceTrace(FALSE);
        }

      doneTrace:
        SetEvent(WaitToTerminate);
        dwRet = NO_ERROR;
        break;

    case SERVICE_CONTROL_INTERROGATE:
        ServiceStatus.dwCurrentState = SERVICE_RUNNING;
        ServiceStatus.dwWin32ExitCode = NO_ERROR;
        ServiceStatus.dwCheckPoint = 0;
        ServiceStatus.dwWaitHint = 0;
        ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_POWEREVENT;
        SetServiceStatus(StatusHandle, &ServiceStatus);
        afsi_log("SERVICE_CONTROL_INTERROGATE");
        dwRet = NO_ERROR;
        break;

        /* XXX handle system shutdown */
        /* XXX handle pause & continue */
    case SERVICE_CONTROL_POWEREVENT:                                              
        { 
#ifdef DEBUG
	    afsi_log("SERVICE_CONTROL_POWEREVENT");
#endif
            /*                                                                                
            **	dwEventType of this notification == WPARAM of WM_POWERBROADCAST               
            **	Return NO_ERROR == return TRUE for that message, i.e. accept request          
            **	Return any error code to deny request,                                        
            **	i.e. as if returning BROADCAST_QUERY_DENY                                     
            */                                                                                
            if (powerEventsRegistered) {
                switch((int) dwEventType)                                                         
                {                                                                               
                case PBT_APMQUERYSUSPEND:       
                    afsi_log("SERVICE_CONTROL_APMQUERYSUSPEND"); 
                    /* Write all dirty buffers back to server */
		    if ( !lana_OnlyLoopback() ) {
			buf_CleanAndReset();
                        cm_SuspendSCache();
                    }
                    afsi_log("SERVICE_CONTROL_APMQUERYSUSPEND buf_CleanAndReset complete"); 
                    dwRet = NO_ERROR;                       
                    break;                                  
                case PBT_APMQUERYSTANDBY:                                                         
                    afsi_log("SERVICE_CONTROL_APMQUERYSTANDBY"); 
                    /* Write all dirty buffers back to server */
		    if ( !lana_OnlyLoopback() ) {
			buf_CleanAndReset();
                        cm_SuspendSCache();
                    }
                    afsi_log("SERVICE_CONTROL_APMQUERYSTANDBY buf_CleanAndReset complete"); 
                    dwRet = NO_ERROR;                                                             
                    break;                                                                        
							                                                                  
                    /* allow remaining case PBT_WhatEver */                                           
                case PBT_APMSUSPEND:                         
                    afsi_log("SERVICE_CONTROL_APMSUSPEND");
		    powerStateSuspended = 1;
		    if (osVersion.dwMajorVersion >= 6) {
                        cm_SuspendSCache();
			smb_StopListeners(0);
                    }
                    dwRet = NO_ERROR;                       
                    break;                                  
                case PBT_APMSTANDBY:                  
                    afsi_log("SERVICE_CONTROL_APMSTANDBY"); 
		    powerStateSuspended = 1;
		    if (osVersion.dwMajorVersion >= 6) {
                        cm_SuspendSCache();
			smb_StopListeners(0);
                    }
                    dwRet = NO_ERROR;                       
                    break;                                  
                case PBT_APMRESUMECRITICAL:             
                    afsi_log("SERVICE_CONTROL_APMRESUMECRITICAL"); 
		    if (osVersion.dwMajorVersion >= 6)
			smb_RestartListeners(0);
                    dwRet = NO_ERROR;                       
                    break;                                  
                case PBT_APMRESUMESUSPEND:                                                        
		    /* User logged in after suspend */
                    afsi_log("SERVICE_CONTROL_APMRESUMESUSPEND"); 
                    dwRet = NO_ERROR;                       
                    break;                                  
                case PBT_APMRESUMESTANDBY:            
		    /* User logged in after standby */
                    afsi_log("SERVICE_CONTROL_APMRESUMESTANDBY"); 
                    dwRet = NO_ERROR;                       
                    break;                                  
                case PBT_APMBATTERYLOW:                                                           
                    afsi_log("SERVICE_CONTROL_APMBATTERYLOW"); 
                    dwRet = NO_ERROR;                       
                    break;                                  
                case PBT_APMPOWERSTATUSCHANGE:                                                    
#ifdef DEBUG
		    afsi_log("SERVICE_CONTROL_APMPOWERSTATUSCHANGE");
#endif
                    dwRet = NO_ERROR;                       
                    break;                                  
                case PBT_APMOEMEVENT:                                                             
#ifdef DEBUG
                    afsi_log("SERVICE_CONTROL_APMOEMEVENT"); 
#endif
                    dwRet = NO_ERROR;                       
                    break;                                  
                case PBT_APMRESUMEAUTOMATIC:          
		    /* This is the message delivered once all devices are up */
                    afsi_log("SERVICE_CONTROL_APMRESUMEAUTOMATIC"); 
		    powerStateSuspended = 0;
		    if (osVersion.dwMajorVersion >= 6)
			smb_RestartListeners(0);
                    dwRet = NO_ERROR;                       
                    break;                                  
                default:                                                                          
                    afsi_log("SERVICE_CONTROL_unknown"); 
                    dwRet = NO_ERROR;                       
                }   
            }
        }
        break;
    case SERVICE_CONTROL_CUSTOM_DUMP: 
        {
            afsi_log("SERVICE_CONTROL_CUSTOM_DUMP"); 
            GenerateMiniDump(NULL);
	    dwRet = NO_ERROR;
        }
        break;
    }		/* end switch(ctrlCode) */                                                        
    return dwRet;   
}

/* There is similar code in client_config\drivemap.cpp GlobalMountDrive()
 * 
 * Mount a drive into AFS if there global mapping
 */
/* DEE Could check first if we are run as SYSTEM */
#define MAX_RETRIES 10
#define MAX_DRIVES  23
static DWORD __stdcall MountGlobalDrivesThread(void * notUsed)
{
#ifndef AFSIFS
    char szAfsPath[_MAX_PATH];
#endif
    char szDriveToMapTo[5];
    DWORD dwResult;
    char szKeyName[256];
    HKEY hKey;
    DWORD dwIndex = 0, dwRetry = 0;
    DWORD dwDriveSize;
    DWORD dwSubMountSize;
    char szSubMount[256];
    DWORD dwType;

    sprintf(szKeyName, "%s\\GlobalAutoMapper", AFSREG_CLT_SVC_PARAM_SUBKEY);

    dwResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKeyName, 0, KEY_QUERY_VALUE, &hKey);
    if (dwResult != ERROR_SUCCESS)
        return 0;

    while (dwIndex < MAX_DRIVES) {
        dwDriveSize = sizeof(szDriveToMapTo);
        dwSubMountSize = sizeof(szSubMount);
        dwResult = RegEnumValue(hKey, dwIndex++, szDriveToMapTo, &dwDriveSize, 0, &dwType, szSubMount, &dwSubMountSize);
        if (dwResult != ERROR_MORE_DATA) {
            if (dwResult != ERROR_SUCCESS) {
                if (dwResult != ERROR_NO_MORE_ITEMS)
                    afsi_log("Failed to read GlobalAutoMapper values: %d\n", dwResult);
                break;
            }
        }

#ifndef AFSIFS
        for (dwRetry = 0 ; dwRetry < MAX_RETRIES; dwRetry++)
        {
            NETRESOURCE nr;
            memset (&nr, 0x00, sizeof(NETRESOURCE));

            sprintf(szAfsPath,"\\\\%s\\%s",cm_NetbiosName,szSubMount);

            nr.dwScope = RESOURCE_GLOBALNET;              /* ignored parameter */
            nr.dwType=RESOURCETYPE_DISK;
            nr.lpLocalName=szDriveToMapTo;
            nr.lpRemoteName=szAfsPath;
            nr.dwDisplayType = RESOURCEDISPLAYTYPE_SHARE; /* ignored parameter */
            nr.dwUsage = RESOURCEUSAGE_CONNECTABLE;       /* ignored parameter */

            dwResult = WNetAddConnection2(&nr,NULL,NULL,0);
            afsi_log("GlobalAutoMap of %s to %s %s (%d)", szDriveToMapTo, szSubMount, 
                     (dwResult == NO_ERROR) ? "succeeded" : "failed", dwResult);
            if (dwResult == NO_ERROR) {
                break;
            }
            /* wait for smb server to come up */
            Sleep((DWORD)1000 /* miliseconds */);		

            /* Disconnect any previous mappings */
            dwResult = WNetCancelConnection2(szDriveToMapTo, 0, TRUE);
        }
#else
	/* FIXFIX: implement */
	afsi_log("GlobalAutoMap of %s to %s not implemented", szDriveToMapTo, szSubMount);
#endif
    }        

    RegCloseKey(hKey);
    return 0;
}

static HANDLE hThreadMountGlobalDrives = NULL;

static void MountGlobalDrives()
{
    DWORD tid;

    hThreadMountGlobalDrives = CreateThread(NULL, 0, MountGlobalDrivesThread, 0, 0, &tid);

    if ( hThreadMountGlobalDrives ) {
        DWORD rc = WaitForSingleObject( hThreadMountGlobalDrives, 15000 );
	if (rc == WAIT_TIMEOUT) {
	    afsi_log("GlobalAutoMap thread failed to complete after 15 seconds");
	} else if (rc == WAIT_OBJECT_0) {
	    afsi_log("GlobalAutoMap thread completed");
	    CloseHandle( hThreadMountGlobalDrives );
	    hThreadMountGlobalDrives = NULL;
	}
    }
}

static void DismountGlobalDrives()
{
#ifndef AFSIFS
    char szAfsPath[_MAX_PATH];
    char szDriveToMapTo[5];
    DWORD dwDriveSize;
    DWORD dwSubMountSize;
    char szSubMount[256];
    DWORD dwType;
#endif
    DWORD dwResult;
    char szKeyName[256];
    HKEY hKey;
    DWORD dwIndex = 0;

    if ( hThreadMountGlobalDrives ) {
        DWORD rc = WaitForSingleObject(hThreadMountGlobalDrives, 0);

	if (rc == WAIT_TIMEOUT) {
	    afsi_log("GlobalAutoMap thread failed to complete before service shutdown");
	}
	else if (rc == WAIT_OBJECT_0) {
	    afsi_log("GlobalAutoMap thread completed");
	    CloseHandle( hThreadMountGlobalDrives );
	    hThreadMountGlobalDrives = NULL;
	}
    }

    sprintf(szKeyName, "%s\\GlobalAutoMapper", AFSREG_CLT_SVC_PARAM_SUBKEY);

    dwResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKeyName, 0, KEY_QUERY_VALUE, &hKey);
    if (dwResult != ERROR_SUCCESS)
        return;

#ifdef AFSIFS    
    /* FIXFIX: implement */
#else
    while (dwIndex < MAX_DRIVES) {
        dwDriveSize = sizeof(szDriveToMapTo);
        dwSubMountSize = sizeof(szSubMount);
        dwResult = RegEnumValue(hKey, dwIndex++, szDriveToMapTo, &dwDriveSize, 0, &dwType, szSubMount, &dwSubMountSize);
        if (dwResult != ERROR_MORE_DATA) {
            if (dwResult != ERROR_SUCCESS) {
                if (dwResult != ERROR_NO_MORE_ITEMS)
                    afsi_log("Failed to read GlobalAutoMapper values: %d\n", dwResult);
                break;
            }
        }

        sprintf(szAfsPath,"\\\\%s\\%s",cm_NetbiosName,szSubMount);
		    
        dwResult = WNetCancelConnection2(szDriveToMapTo, 0, TRUE);
        dwResult = WNetCancelConnection(szAfsPath, TRUE);
        
        afsi_log("Disconnect from GlobalAutoMap of %s to %s %s", szDriveToMapTo, szSubMount, dwResult ? "succeeded" : "failed");
    }        
#endif

    RegCloseKey(hKey);
}

DWORD
GetVersionInfo( CHAR * filename, CHAR * szOutput, DWORD dwOutput )
{
    DWORD dwVersionHandle;
    LPVOID pVersionInfo = 0;
    DWORD retval = 0;
    LPDWORD pLangInfo = 0;
    LPTSTR szVersion = 0;
    UINT len = 0;
    TCHAR szVerQ[] = TEXT("\\StringFileInfo\\12345678\\FileVersion");
    DWORD size = GetFileVersionInfoSize(filename, &dwVersionHandle);

    if (!size) {
        afsi_log("GetFileVersionInfoSize failed");
        return GetLastError();
    }

    pVersionInfo = malloc(size);
    if (!pVersionInfo) {
        afsi_log("out of memory 1");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    GetFileVersionInfo(filename, dwVersionHandle, size, pVersionInfo);
    if (retval = GetLastError())
    {
        afsi_log("GetFileVersionInfo failed: %d", retval);
        goto cleanup;
    }

    VerQueryValue(pVersionInfo, TEXT("\\VarFileInfo\\Translation"),
                       (LPVOID*)&pLangInfo, &len);
    if (retval = GetLastError())
    {
        afsi_log("VerQueryValue 1 failed: %d", retval);
        goto cleanup;
    }

    wsprintf(szVerQ,
             TEXT("\\StringFileInfo\\%04x%04x\\FileVersion"),
             LOWORD(*pLangInfo), HIWORD(*pLangInfo));

    VerQueryValue(pVersionInfo, szVerQ, (LPVOID*)&szVersion, &len);
    if (retval = GetLastError())
    {
        /* try again with language 409 since the old binaries were tagged wrong */
        wsprintf(szVerQ,
                  TEXT("\\StringFileInfo\\0409%04x\\FileVersion"),
                  HIWORD(*pLangInfo));

        VerQueryValue(pVersionInfo, szVerQ, (LPVOID*)&szVersion, &len);
        if (retval = GetLastError()) {
            afsi_log("VerQueryValue 2 failed: [%s] %d", szVerQ, retval);
            goto cleanup;
        }
    }
    snprintf(szOutput, dwOutput, TEXT("%s"), szVersion);
    szOutput[dwOutput - 1] = 0;

 cleanup:
    if (pVersionInfo)
        free(pVersionInfo);

    return retval;
}

static HINSTANCE hCrypt32;
static DWORD (WINAPI *pCertGetNameString)(PCCERT_CONTEXT pCertContext,  DWORD dwType,  DWORD dwFlags,
                                          void* pvTypePara, LPTSTR pszNameString, DWORD cchNameString);
static BOOL (WINAPI *pCryptQueryObject)(DWORD dwObjectType, const void* pvObject, DWORD dwExpectedContentTypeFlags,
                                        DWORD dwExpectedFormatTypeFlags, DWORD dwFlags,
                                        DWORD* pdwMsgAndCertEncodingType, DWORD* pdwContentType,
                                        DWORD* pdwFormatType, HCERTSTORE* phCertStore,
                                        HCRYPTMSG* phMsg, const void** ppvContext);
static BOOL (WINAPI *pCryptMsgGetParam)(HCRYPTMSG hCryptMsg, DWORD dwParamType, DWORD dwIndex,
                                        void* pvData, DWORD* pcbData);
static PCCERT_CONTEXT (WINAPI *pCertFindCertificateInStore)(HCERTSTORE hCertStore, DWORD dwCertEncodingType,
                                                            DWORD dwFindFlags, DWORD dwFindType,
                                                            const void* pvFindPara,
                                                            PCCERT_CONTEXT pPrevCertContext);
static BOOL (WINAPI *pCertCloseStore)(HCERTSTORE hCertStore, DWORD dwFlags);
static BOOL (WINAPI *pCryptMsgClose)(HCRYPTMSG hCryptMsg);
static BOOL (WINAPI *pCertCompareCertificate)(DWORD dwCertEncodingType, PCERT_INFO pCertId1,
                                              PCERT_INFO pCertId2);
static BOOL (WINAPI *pCertFreeCertificateContext)(PCCERT_CONTEXT pCertContext);

void LoadCrypt32(void)
{
    hCrypt32 = LoadLibrary("crypt32");
    if ( !hCrypt32 )
        return;

    (FARPROC) pCertGetNameString = GetProcAddress( hCrypt32, "CertGetNameString" );
    (FARPROC) pCryptQueryObject = GetProcAddress( hCrypt32, "CryptQueryObject" );
    (FARPROC) pCryptMsgGetParam = GetProcAddress( hCrypt32, "CryptMsgGetParam" );
    (FARPROC) pCertFindCertificateInStore = GetProcAddress( hCrypt32, "CertFindCertificateInStore" );
    (FARPROC) pCertCloseStore = GetProcAddress( hCrypt32, "CertCloseStore" );
    (FARPROC) pCryptMsgClose = GetProcAddress( hCrypt32, "CryptMsgClose" );
    (FARPROC) pCertCompareCertificate = GetProcAddress( hCrypt32, "CertCompareCertificate" );
    (FARPROC) pCertFreeCertificateContext = GetProcAddress( hCrypt32, "CertFreeCertificateContext" );
    
    if ( !pCertGetNameString ||
         !pCryptQueryObject ||
         !pCryptMsgGetParam ||
         !pCertFindCertificateInStore ||
         !pCertCloseStore ||
         !pCryptMsgClose ||
         !pCertCompareCertificate ||
         !pCertFreeCertificateContext)
    {
        FreeLibrary(hCrypt32);
        hCrypt32 = NULL;
    }
}

void UnloadCrypt32(void)
{
    FreeLibrary(hCrypt32);
}

#define ENCODING (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING)

PCCERT_CONTEXT GetCertCtx(CHAR * filename)
{
    wchar_t wfilename[260];
    BOOL fResult;
    DWORD dwEncoding;
    DWORD dwContentType;
    DWORD dwFormatType;
    DWORD dwSignerInfo;
    HCERTSTORE hStore = NULL;
    HCRYPTMSG hMsg = NULL;
    PCMSG_SIGNER_INFO pSignerInfo = NULL;
    PCCERT_CONTEXT pCertContext = NULL;
    CERT_INFO CertInfo;

    if ( hCrypt32 == NULL )
        return NULL;

    ZeroMemory(&CertInfo, sizeof(CertInfo));
    mbstowcs(wfilename, filename, 260);

    fResult = pCryptQueryObject(CERT_QUERY_OBJECT_FILE,
			        wfilename,
			        CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
			        CERT_QUERY_FORMAT_FLAG_BINARY,
			        0,
			        &dwEncoding,
			        &dwContentType,
			        &dwFormatType,
			        &hStore,
			        &hMsg,
			        NULL);

    if (!fResult) {
        afsi_log("CryptQueryObject failed for [%s] with error 0x%x",
		 filename,
		 GetLastError());
	goto __exit;
    }

    fResult = pCryptMsgGetParam(hMsg,
			        CMSG_SIGNER_INFO_PARAM,
			        0,
			        NULL,
			        &dwSignerInfo);

    if (!fResult) {
        afsi_log("CryptMsgGetParam failed for [%s] with error 0x%x",
		 filename,
		 GetLastError());
	goto __exit;
    }

    pSignerInfo = (PCMSG_SIGNER_INFO)LocalAlloc(LPTR, dwSignerInfo);

    fResult = pCryptMsgGetParam(hMsg,
			        CMSG_SIGNER_INFO_PARAM,
			        0,
			        (PVOID)pSignerInfo,
			        &dwSignerInfo);
    
    if (!fResult) {
        afsi_log("CryptMsgGetParam failed for [%s] with error 0x%x",
		 filename,
		 GetLastError());
	goto __exit;
    }

    CertInfo.Issuer = pSignerInfo->Issuer;
    CertInfo.SerialNumber = pSignerInfo->SerialNumber;

    pCertContext = pCertFindCertificateInStore(hStore,
					      ENCODING,
					      0,
					      CERT_FIND_SUBJECT_CERT,
					      (PVOID) &CertInfo,
					      NULL);

    if (!pCertContext) {
      afsi_log("CertFindCertificateInStore for file [%s] failed with 0x%x",
	       filename,
	       GetLastError());
      goto __exit;
    }

  __exit:
    if (pSignerInfo)
      LocalFree(pSignerInfo);

    /*    if (pCertContext)
	  CertFreeCertificateContext(pCertContext);*/

    if (hStore)
      pCertCloseStore(hStore,0);

    if (hMsg)
      pCryptMsgClose(hMsg);

    return pCertContext;
}

BOOL VerifyTrust(CHAR * filename)
{
    WIN_TRUST_ACTDATA_CONTEXT_WITH_SUBJECT fContextWSubject;
    WIN_TRUST_SUBJECT_FILE fSubjectFile;
    GUID trustAction = WIN_SPUB_ACTION_PUBLISHED_SOFTWARE;
    GUID subject = WIN_TRUST_SUBJTYPE_PE_IMAGE;
    wchar_t wfilename[260];
    LONG ret;
    BOOL success = FALSE;

    LONG (WINAPI *pWinVerifyTrust)(HWND hWnd, GUID* pgActionID, WINTRUST_DATA* pWinTrustData) = NULL;
    HINSTANCE hWinTrust;

    if (filename == NULL ) 
        return FALSE;

    hWinTrust = LoadLibrary("wintrust");
    if ( !hWinTrust )
        return FALSE;

    if (((FARPROC) pWinVerifyTrust =
          GetProcAddress( hWinTrust, "WinVerifyTrust" )) == NULL )
    {
        FreeLibrary(hWinTrust);
        return FALSE;
    }

    mbstowcs(wfilename, filename, 260);

    fSubjectFile.hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                    0, NULL);
    fSubjectFile.lpPath = wfilename;
    fContextWSubject.hClientToken = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                                FALSE, GetCurrentProcessId());
    fContextWSubject.SubjectType = &subject;
    fContextWSubject.Subject = &fSubjectFile;

    ret = pWinVerifyTrust(INVALID_HANDLE_VALUE, &trustAction, (WINTRUST_DATA *)&fContextWSubject);

    if ( fSubjectFile.hFile != INVALID_HANDLE_VALUE )
        CloseHandle( fSubjectFile.hFile );
    if ( fContextWSubject.hClientToken != INVALID_HANDLE_VALUE )
        CloseHandle( fContextWSubject.hClientToken );

    if (ret == ERROR_SUCCESS) {
        success = TRUE;
    } else {
        DWORD gle = GetLastError();
        switch (gle) {
        case TRUST_E_PROVIDER_UNKNOWN:
            afsi_log("VerifyTrust failed: \"Generic Verify V2\" Provider Unknown");
            break;  
        case TRUST_E_NOSIGNATURE:
            afsi_log("VerifyTrust failed: Unsigned executable");
            break;
        case TRUST_E_EXPLICIT_DISTRUST:
            afsi_log("VerifyTrust failed: Certificate Marked as Untrusted by the user");
            break;
        case TRUST_E_SUBJECT_NOT_TRUSTED:
            afsi_log("VerifyTrust failed: File is not trusted");
            break;
        case TRUST_E_BAD_DIGEST:
            afsi_log("VerifyTrust failed: Executable has been modified");
            break;
        case CRYPT_E_SECURITY_SETTINGS:
            afsi_log("VerifyTrust failed: local security options prevent verification");
            break;
        default:
            afsi_log("VerifyTrust failed: 0x%X", GetLastError());
        }
        success = FALSE;
    }
    FreeLibrary(hWinTrust);
    return success;
}

void LogCertCtx(PCCERT_CONTEXT pCtx) {
    DWORD dwData;
    LPTSTR szName = NULL;

    if ( hCrypt32 == NULL )
        return;

    // Get Issuer name size.
    if (!(dwData = pCertGetNameString(pCtx,
		     		      CERT_NAME_SIMPLE_DISPLAY_TYPE,
				      CERT_NAME_ISSUER_FLAG,
				      NULL,
				      NULL,
				      0))) {
        afsi_log("CertGetNameString failed: 0x%x", GetLastError());
	goto __exit;
    }

    // Allocate memory for Issuer name.
    szName = (LPTSTR)LocalAlloc(LPTR, dwData * sizeof(TCHAR));

    // Get Issuer name.
    if (!(pCertGetNameString(pCtx,
			     CERT_NAME_SIMPLE_DISPLAY_TYPE,
			     CERT_NAME_ISSUER_FLAG,
			     NULL,
			     szName,
			     dwData))) {
        afsi_log("CertGetNameString failed: 0x%x", GetLastError());
	goto __exit;
    }

    // print Issuer name.
    afsi_log("Issuer Name: %s", szName);
    LocalFree(szName);
    szName = NULL;

    // Get Subject name size.
    if (!(dwData = pCertGetNameString(pCtx,
				      CERT_NAME_SIMPLE_DISPLAY_TYPE,
				      0,
				      NULL,
				      NULL,
				      0))) {
        afsi_log("CertGetNameString failed: 0x%x", GetLastError());
	goto __exit;
    }

    // Allocate memory for subject name.
    szName = (LPTSTR)LocalAlloc(LPTR, dwData * sizeof(TCHAR));

    // Get subject name.
    if (!(pCertGetNameString(pCtx,
			     CERT_NAME_SIMPLE_DISPLAY_TYPE,
			     0,
			     NULL,
			     szName,
			     dwData))) {
        afsi_log("CertGetNameString failed: 0x%x", GetLastError());
	goto __exit;
    }

    // Print Subject Name.
    afsi_log("Subject Name: %s", szName);

  __exit:

    if (szName)
        LocalFree(szName);
}

BOOL AFSModulesVerify(void)
{
    CHAR filename[1024];
    CHAR afsdVersion[128];
    CHAR modVersion[128];
    CHAR checkName[1024];
    BOOL trustVerified = FALSE;
    HMODULE hMods[1024];
    HANDLE hProcess;
    DWORD cbNeeded;
    unsigned int i;
    BOOL success = TRUE;
    PCCERT_CONTEXT pCtxService = NULL;
    HINSTANCE hPSAPI;
    DWORD (WINAPI *pGetModuleFileNameExA)(HANDLE hProcess, HMODULE hModule, LPTSTR lpFilename, DWORD nSize);
    BOOL (WINAPI *pEnumProcessModules)(HANDLE hProcess, HMODULE* lphModule, DWORD cb, LPDWORD lpcbNeeded);
    DWORD dummyLen, code;
    DWORD cacheSize = CM_CONFIGDEFAULT_CACHESIZE;
    DWORD verifyServiceSig = TRUE;
    HKEY parmKey;

    hPSAPI = LoadLibrary("psapi");

    if ( hPSAPI == NULL )
        return FALSE;

    if (!GetModuleFileName(NULL, filename, sizeof(filename)))
        return FALSE;

    if (GetVersionInfo(filename, afsdVersion, sizeof(afsdVersion)))
        return FALSE;

    afsi_log("%s version %s", filename, afsdVersion);

    if (((FARPROC) pGetModuleFileNameExA =
          GetProcAddress( hPSAPI, "GetModuleFileNameExA" )) == NULL ||
         ((FARPROC) pEnumProcessModules =
           GetProcAddress( hPSAPI, "EnumProcessModules" )) == NULL)
    {
        FreeLibrary(hPSAPI);
        return FALSE;
    }


    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                        AFSREG_CLT_SVC_PARAM_SUBKEY,
                        0, KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        dummyLen = sizeof(cacheSize);
        code = RegQueryValueEx(parmKey, "CacheSize", NULL, NULL,
                               (BYTE *) &cacheSize, &dummyLen);
        RegCloseKey (parmKey);
    }

    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_OPENAFS_SUBKEY,
                         0, KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        dummyLen = sizeof(verifyServiceSig);
        code = RegQueryValueEx(parmKey, "VerifyServiceSignature", NULL, NULL,
                                (BYTE *) &verifyServiceSig, &dummyLen);
        RegCloseKey (parmKey);
    }

    if (verifyServiceSig 
#ifndef _WIN64
         && cacheSize < 716800
#endif
         ) {
        trustVerified = VerifyTrust(filename);
    } else {
        afsi_log("Signature Verification disabled");
    }

    if (trustVerified) {
        LoadCrypt32();

        // get a certificate context for the signer of afsd_service.
        pCtxService = GetCertCtx(filename);
        if (pCtxService)
            LogCertCtx(pCtxService);
    }

    // Get a list of all the modules in this process.
    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                           FALSE, GetCurrentProcessId());

    if (pEnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
    {
        afsi_log("Num of Process Modules: %d", (cbNeeded / sizeof(HMODULE)));

        for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
        {
            char szModName[2048];

            // Get the full path to the module's file.
            if (pGetModuleFileNameExA(hProcess, hMods[i], szModName, sizeof(szModName)))
            {
                lstrcpy(checkName, szModName);
                strlwr(checkName);

                if ( strstr(checkName, "afspthread.dll") ||
                     strstr(checkName, "afsauthent.dll") ||
                     strstr(checkName, "afsrpc.dll") ||
                     strstr(checkName, "libafsconf.dll") ||
                     strstr(checkName, "libosi.dll") )
                {
                    if (GetVersionInfo(szModName, modVersion, sizeof(modVersion))) {
                        success = FALSE;
                        continue;
                    }

                    afsi_log("%s version %s", szModName, modVersion);
                    if (strcmp(afsdVersion,modVersion)) {
                        afsi_log("Version mismatch: %s", szModName);
                        success = FALSE;
                    }
                    if ( trustVerified ) {
                        if ( !VerifyTrust(szModName) ) {
                            afsi_log("Signature Verification failed: %s", szModName);
                            success = FALSE;
                        } 
                        else if (pCtxService) {
                            PCCERT_CONTEXT pCtx = GetCertCtx(szModName);

                            if (!pCtx || !pCertCompareCertificate(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                                                  pCtxService->pCertInfo,
                                                                  pCtx->pCertInfo)) {
                                afsi_log("Certificate mismatch: %s", szModName);
                                if (pCtx)
                                    LogCertCtx(pCtx);
                                
                                success = FALSE;
                            }
                            
                            if (pCtx)
                                pCertFreeCertificateContext(pCtx);
                        }
                    }
                }
            }
        }
    }

    if (pCtxService) {
        pCertFreeCertificateContext(pCtxService);
        UnloadCrypt32();
    }

    FreeLibrary(hPSAPI);

    CloseHandle(hProcess);
    return success;
}

/*
control serviceex exists only on 2000/xp. These functions will be loaded dynamically.
*/

typedef SERVICE_STATUS_HANDLE ( * RegisterServiceCtrlHandlerExFunc )(  LPCTSTR , LPHANDLER_FUNCTION_EX , LPVOID );
typedef SERVICE_STATUS_HANDLE ( * RegisterServiceCtrlHandlerFunc   )(  LPCTSTR ,  LPHANDLER_FUNCTION );

RegisterServiceCtrlHandlerExFunc pRegisterServiceCtrlHandlerEx = NULL;
RegisterServiceCtrlHandlerFunc   pRegisterServiceCtrlHandler   = NULL; 

VOID WINAPI
afsd_Main(DWORD argc, LPTSTR *argv)
{
    long code;
    char *reason;
#ifdef JUMP
    int jmpret;
#endif /* JUMP */
    HMODULE hHookDll;
    HMODULE hAdvApi32;
#ifdef AFSIFS
    int cnt;
#endif

#ifdef _DEBUG
    afsd_DbgBreakAllocInit();
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF /*| _CRTDBG_CHECK_ALWAYS_DF*/ | 
                   _CRTDBG_CHECK_CRT_DF /* | _CRTDBG_DELAY_FREE_MEM_DF */ );
#endif 

    afsd_SetUnhandledExceptionFilter();
       
    osi_InitPanic(afsd_notifier);
    osi_InitTraceOption();

    GlobalStatus = 0;

    afsi_start();

    WaitToTerminate = CreateEvent(NULL, TRUE, FALSE, TEXT("afsd_service_WaitToTerminate"));
    if ( GetLastError() == ERROR_ALREADY_EXISTS )
        afsi_log("Event Object Already Exists: %s", TEXT("afsd_service_WaitToTerminate"));

#ifndef NOTSERVICE
    hAdvApi32 = LoadLibrary("advapi32.dll");
    if (hAdvApi32 == NULL)
    {
        afsi_log("Fatal: cannot load advapi32.dll");
        return;
    }

    pRegisterServiceCtrlHandlerEx = (RegisterServiceCtrlHandlerExFunc)GetProcAddress(hAdvApi32, "RegisterServiceCtrlHandlerExA");
    if (pRegisterServiceCtrlHandlerEx)
    {
        afsi_log("running on 2000+ - using RegisterServiceCtrlHandlerEx");
        StatusHandle = RegisterServiceCtrlHandlerEx(AFS_DAEMON_SERVICE_NAME, afsd_ServiceControlHandlerEx, NULL );
    }
    else
    {
        StatusHandle = RegisterServiceCtrlHandler(AFS_DAEMON_SERVICE_NAME, afsd_ServiceControlHandler);
    }

    ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    ServiceStatus.dwServiceSpecificExitCode = 0;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    ServiceStatus.dwWin32ExitCode = NO_ERROR;
    ServiceStatus.dwCheckPoint = 1;
    ServiceStatus.dwWaitHint = 120000;
    /* accept Power Events */
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_POWEREVENT | SERVICE_ACCEPT_PARAMCHANGE;
    SetServiceStatus(StatusHandle, &ServiceStatus);
#endif

    LogEvent(EVENTLOG_INFORMATION_TYPE, MSG_SERVICE_START_PENDING);

#ifdef REGISTER_POWER_NOTIFICATIONS
    {
        HKEY hkParm;
        DWORD code;
        DWORD dummyLen;
        int bpower = TRUE;

        /* see if we should handle power notifications */
        code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY, 
                            0, KEY_QUERY_VALUE, &hkParm);
        if (code == ERROR_SUCCESS) {
            dummyLen = sizeof(bpower);
            code = RegQueryValueEx(hkParm, "FlushOnHibernate", NULL, NULL,
                (BYTE *) &bpower, &dummyLen);      

            if(code != ERROR_SUCCESS)
                bpower = TRUE;

	    RegCloseKey(hkParm);
        }
        /* create thread used to flush cache */
        if (bpower) {
            PowerNotificationThreadCreate();
            powerEventsRegistered = 1;
        }
    }
#endif

    /* Verify the versions of the DLLs which were loaded */
    if (!AFSModulesVerify()) {
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        ServiceStatus.dwWin32ExitCode = NO_ERROR;
        ServiceStatus.dwCheckPoint = 0;
        ServiceStatus.dwWaitHint = 0;
        ServiceStatus.dwControlsAccepted = 0;
        SetServiceStatus(StatusHandle, &ServiceStatus);

	LogEvent(EVENTLOG_ERROR_TYPE, MSG_SERVICE_INCORRECT_VERSIONS);

        /* exit if initialization failed */
        return;
    }

    /* allow an exit to be called prior to any initialization */
    hHookDll = LoadLibrary(AFSD_HOOK_DLL);
    if (hHookDll)
    {
        BOOL hookRc = TRUE;
        AfsdInitHook initHook = ( AfsdInitHook ) GetProcAddress(hHookDll, AFSD_INIT_HOOK);
        if (initHook)
        {
            hookRc = initHook();
        }
        FreeLibrary(hHookDll);
        hHookDll = NULL;

        if (hookRc == FALSE)
        {
            ServiceStatus.dwCurrentState = SERVICE_STOPPED;
            ServiceStatus.dwWin32ExitCode = NO_ERROR;
            ServiceStatus.dwCheckPoint = 0;
            ServiceStatus.dwWaitHint = 0;
            ServiceStatus.dwControlsAccepted = 0;
            SetServiceStatus(StatusHandle, &ServiceStatus);
                       
            /* exit if initialization failed */
            return;
        }
        else
        {
            /* allow another 120 seconds to start */
            ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
            ServiceStatus.dwServiceSpecificExitCode = 0;
            ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
            ServiceStatus.dwWin32ExitCode = NO_ERROR;
            ServiceStatus.dwCheckPoint = 2;
            ServiceStatus.dwWaitHint = 120000;
            /* accept Power Events */
            ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_POWEREVENT | SERVICE_ACCEPT_PARAMCHANGE;
            SetServiceStatus(StatusHandle, &ServiceStatus);
        }
    }

    /* Perform Volume Status Notification Initialization */
    cm_VolStatus_Initialization();

#ifdef JUMP
    MainThreadId = GetCurrentThreadId();
    jmpret = setjmp(notifier_jmp);

    if (jmpret == 0) 
#endif /* JUMP */
    {
        code = afsd_InitCM(&reason);
        if (code != 0) {
            afsi_log("afsd_InitCM failed: %s (code = %d)", reason, code);
            osi_panic(reason, __FILE__, __LINE__);
        }

#ifndef NOTSERVICE
        ServiceStatus.dwCheckPoint = 3;
        ServiceStatus.dwWaitHint = 30000;
        SetServiceStatus(StatusHandle, &ServiceStatus);
#endif
        code = afsd_InitDaemons(&reason);
        if (code != 0) {
            afsi_log("afsd_InitDaemons failed: %s (code = %d)", reason, code);
			osi_panic(reason, __FILE__, __LINE__);
        }

        /* allow an exit to be called post rx initialization */
        hHookDll = LoadLibrary(AFSD_HOOK_DLL);
        if (hHookDll)
        {
            BOOL hookRc = TRUE;
            AfsdRxStartedHook rxStartedHook = ( AfsdRxStartedHook ) GetProcAddress(hHookDll, AFSD_RX_STARTED_HOOK);
            if (rxStartedHook)
            {
                hookRc = rxStartedHook();
            }
            FreeLibrary(hHookDll);
            hHookDll = NULL;

            if (hookRc == FALSE)
            {
                ServiceStatus.dwCurrentState = SERVICE_STOPPED;
                ServiceStatus.dwWin32ExitCode = NO_ERROR;
                ServiceStatus.dwCheckPoint = 0;
                ServiceStatus.dwWaitHint = 0;
                ServiceStatus.dwControlsAccepted = 0;
                SetServiceStatus(StatusHandle, &ServiceStatus);
                       
                /* exit if initialization failed */
                return;
            }
        }

#ifndef NOTSERVICE
        ServiceStatus.dwCheckPoint = 4;
        ServiceStatus.dwWaitHint = 15000;
        SetServiceStatus(StatusHandle, &ServiceStatus);
#endif

        /* Notify any volume status handlers that the cache manager has started */
        cm_VolStatus_Service_Started();

/* the following ifdef chooses the mode of operation for the service.  to enable
 * a runtime flag (instead of compile-time), pioctl() would need to dynamically
 * determine the mode, in order to use the correct ioctl special-file path. */
#ifndef AFSIFS
        code = afsd_InitSMB(&reason, MessageBox);
        if (code != 0) {
            afsi_log("afsd_InitSMB failed: %s (code = %d)", reason, code);
            osi_panic(reason, __FILE__, __LINE__);
        }
#else
        code = ifs_Init(&reason);
        if (code != 0) {
            afsi_log("ifs_Init failed: %s (code = %d)", reason, code);
            osi_panic(reason, __FILE__, __LINE__);
        }     
        for (cnt = 0; cnt < WORKER_THREADS; cnt++)
            hAFSDWorkerThread[cnt] = CreateThread(NULL, 0, ifs_MainLoop, 0, 0, NULL);
#endif  

        /* allow an exit to be called post smb initialization */
        hHookDll = LoadLibrary(AFSD_HOOK_DLL);
        if (hHookDll)
        {
            BOOL hookRc = TRUE;
            AfsdSmbStartedHook smbStartedHook = ( AfsdSmbStartedHook ) GetProcAddress(hHookDll, AFSD_SMB_STARTED_HOOK);
            if (smbStartedHook)
            {
                hookRc = smbStartedHook();
            }
            FreeLibrary(hHookDll);
            hHookDll = NULL;

            if (hookRc == FALSE)
            {
                ServiceStatus.dwCurrentState = SERVICE_STOPPED;
                ServiceStatus.dwWin32ExitCode = NO_ERROR;
                ServiceStatus.dwCheckPoint = 0;
                ServiceStatus.dwWaitHint = 0;
                ServiceStatus.dwControlsAccepted = 0;
                SetServiceStatus(StatusHandle, &ServiceStatus);
                       
                /* exit if initialization failed */
                return;
            }
        }

        MountGlobalDrives();

#ifndef NOTSERVICE
        ServiceStatus.dwCurrentState = SERVICE_RUNNING;
        ServiceStatus.dwWin32ExitCode = NO_ERROR;
        ServiceStatus.dwCheckPoint = 5;
        ServiceStatus.dwWaitHint = 0;

        /* accept Power events */
        ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_POWEREVENT | SERVICE_ACCEPT_PARAMCHANGE;
        SetServiceStatus(StatusHandle, &ServiceStatus);
#endif  

	LogEvent(EVENTLOG_INFORMATION_TYPE, MSG_SERVICE_RUNNING);
    }

    /* allow an exit to be called when started */
    hHookDll = LoadLibrary(AFSD_HOOK_DLL);
    if (hHookDll)
    {
        BOOL hookRc = TRUE;
        AfsdStartedHook startedHook = ( AfsdStartedHook ) GetProcAddress(hHookDll, AFSD_STARTED_HOOK);
        if (startedHook)
        {
            hookRc = startedHook();
        }
        FreeLibrary(hHookDll);
        hHookDll = NULL;

        if (hookRc == FALSE)
        {
            ServiceStatus.dwCurrentState = SERVICE_STOPPED;
            ServiceStatus.dwWin32ExitCode = NO_ERROR;
            ServiceStatus.dwCheckPoint = 0;
            ServiceStatus.dwWaitHint = 0;
            ServiceStatus.dwControlsAccepted = 0;
            SetServiceStatus(StatusHandle, &ServiceStatus);
                       
            /* exit if initialization failed */
            return;
        }
    }

#ifndef AFSIFS
    WaitForSingleObject(WaitToTerminate, INFINITE);
#else
    WaitForMultipleObjects(WORKER_THREADS, hAFSDWorkerThread, TRUE, INFINITE);
    for (cnt = 0; cnt < WORKER_THREADS; cnt++)
        CloseHandle(hAFSDWorkerThread[cnt]);
#endif

    ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
    ServiceStatus.dwWin32ExitCode = NO_ERROR;
    ServiceStatus.dwCheckPoint = 6;
    ServiceStatus.dwWaitHint = 120000;
    ServiceStatus.dwControlsAccepted = 0;
    SetServiceStatus(StatusHandle, &ServiceStatus);

    afsi_log("Received Termination Signal, Stopping Service");

    if ( GlobalStatus )
	LogEvent(EVENTLOG_ERROR_TYPE, MSG_SERVICE_ERROR_STOP);
    else
	LogEvent(EVENTLOG_INFORMATION_TYPE, MSG_SERVICE_STOPPING);

    /* allow an exit to be called prior to stopping the service */
    hHookDll = LoadLibrary(AFSD_HOOK_DLL);
    if (hHookDll)
    {
        BOOL hookRc = TRUE;
        AfsdStoppingHook stoppingHook = ( AfsdStoppingHook ) GetProcAddress(hHookDll, AFSD_STOPPING_HOOK);
        if (stoppingHook)
        {
            hookRc = stoppingHook();
        }
        FreeLibrary(hHookDll);
        hHookDll = NULL;
    }


#ifdef AFS_FREELANCE_CLIENT
    cm_FreelanceShutdown();
    afsi_log("Freelance Shutdown complete");
#endif

    DismountGlobalDrives();
    afsi_log("Global Drives dismounted");
                                         
    cm_DaemonShutdown();                 
    afsi_log("Daemon shutdown complete");
    
    afsd_ShutdownCM();

    buf_Shutdown();                      
    afsi_log("Buffer shutdown complete");
                                         
    rx_Finalize();                       
    afsi_log("rx finalization complete");
                                         
#ifndef AFSIFS
    smb_Shutdown();                      
    afsi_log("smb shutdown complete");   
#endif
                                         
    RpcShutdown();                       

    cm_ReleaseAllLocks();

    rx_Finalize();
    afsi_log("rx finalization complete");

    cm_ShutdownMappedMemory();           

#ifdef	REGISTER_POWER_NOTIFICATIONS
    /* terminate thread used to flush cache */
    if (powerEventsRegistered)
        PowerNotificationThreadExit();
#endif

    cm_DirDumpStats();
#ifdef USE_BPLUS
    cm_BPlusDumpStats();
#endif

    /* Notify any Volume Status Handlers that we are stopped */
    cm_VolStatus_Service_Stopped();

    /* Cleanup any Volume Status Notification Handler */
    cm_VolStatus_Finalize();

    /* allow an exit to be called after stopping the service */
    hHookDll = LoadLibrary(AFSD_HOOK_DLL);
    if (hHookDll)
    {
        BOOL hookRc = TRUE;
        AfsdStoppedHook stoppedHook = ( AfsdStoppedHook ) GetProcAddress(hHookDll, AFSD_STOPPED_HOOK);
        if (stoppedHook)
        {
            hookRc = stoppedHook();
        }
        FreeLibrary(hHookDll);
        hHookDll = NULL;
    }

    /* Remove the ExceptionFilter */
    SetUnhandledExceptionFilter(NULL);

    ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    ServiceStatus.dwWin32ExitCode = GlobalStatus ? ERROR_EXCEPTION_IN_SERVICE : NO_ERROR;
    ServiceStatus.dwCheckPoint = 7;
    ServiceStatus.dwWaitHint = 0;
    ServiceStatus.dwControlsAccepted = 0;
    SetServiceStatus(StatusHandle, &ServiceStatus);
}       

DWORD __stdcall afsdMain_thread(void* notUsed)
{
    char * argv[2] = {AFS_DAEMON_SERVICE_NAME, NULL};
    afsd_Main(1, (LPTSTR*)argv);
    return(0);
}

void usage(void)
{
    fprintf(stderr, "afsd_service.exe [--validate-cache <cache-path>]");
}

int
main(int argc, char * argv[])
{
    static SERVICE_TABLE_ENTRY dispatchTable[] = {
        {AFS_DAEMON_SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION) afsd_Main},
        {NULL, NULL}
    };
    int i;

    for (i = 1; i < argc; i++) {
        if (!stricmp(argv[i],"--validate-cache")) {
            if (++i != argc - 1) {
                usage();
                return(1);
            }

            return cm_ValidateMappedMemory(argv[i]);
        } else {
            usage();
            return(1);
        }
    }

    if (!StartServiceCtrlDispatcher(dispatchTable))
    {
        LONG status = GetLastError();
        if (status == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
        {
            DWORD tid;
            hAFSDMainThread = CreateThread(NULL, 0, afsdMain_thread, 0, 0, &tid);
		
            printf("Hit <Enter> to terminate OpenAFS Client Service\n");
            getchar();  
            SetEvent(WaitToTerminate);
#ifdef AFSIFS
	    dc_release_hooks();
#endif
        }
    }

    if ( hAFSDMainThread ) {
        WaitForSingleObject( hAFSDMainThread, INFINITE );
        CloseHandle( hAFSDMainThread );
    }
    return(0);
}
