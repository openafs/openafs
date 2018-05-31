/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <roken.h>

#include <windows.h>
#include <winioctl.h>
#define SECURITY_WIN32
#include <security.h>
#include <nb30.h>
#include <tchar.h>
#include <strsafe.h>

#include <osi.h>

#include <cm.h>
#include <cm_nls.h>
#include <cm_server.h>
#include <cm_cell.h>
#include <cm_user.h>
#include <cm_conn.h>
#include <cm_scache.h>
#include <cm_buf.h>
#include <cm_dir.h>
#include <cm_utils.h>
#include <cm_ioctl.h>
#include <smb_iocons.h>
#include <smb.h>
#include <pioctl_nt.h>
#include <WINNT/afsreg.h>
#include <lanahelper.h>

#include <krb5.h>
#include <..\WINNT\afsrdr\common\AFSUserDefines.h>
#include <..\WINNT\afsrdr\common\AFSUserIoctl.h>
#include <..\WINNT\afsrdr\common\AFSUserStructs.h>

static char AFSConfigKeyName[] = AFSREG_CLT_SVC_PARAM_SUBKEY;

static const char utf8_prefix[] = UTF8_PREFIX;
static const int  utf8_prefix_size = sizeof(utf8_prefix) -  sizeof(char);

#define FS_IOCTLREQUEST_MAXSIZE	8192
/* big structure for representing and storing an IOCTL request */
typedef struct fs_ioctlRequest {
    char *mp;			/* marshalling/unmarshalling ptr */
    long nbytes;		/* bytes received (when unmarshalling) */
    char data[FS_IOCTLREQUEST_MAXSIZE];	/* data we're marshalling */
} fs_ioctlRequest_t;


static int
CMtoUNIXerror(int cm_code)
{
    switch (cm_code) {
    case CM_ERROR_TIMEDOUT:
	return ETIMEDOUT;
    case CM_ERROR_NOACCESS:
	return EACCES;
    case CM_ERROR_NOSUCHFILE:
    case CM_ERROR_NOSUCHPATH:
    case CM_ERROR_BPLUS_NOMATCH:
	return ENOENT;
    case CM_ERROR_INVAL:
	return EINVAL;
    case CM_ERROR_BADFD:
	return EBADF;
    case CM_ERROR_EXISTS:
    case CM_ERROR_INEXACT_MATCH:
	return EEXIST;
    case CM_ERROR_CROSSDEVLINK:
	return EXDEV;
    case CM_ERROR_NOTDIR:
	return ENOTDIR;
    case CM_ERROR_ISDIR:
	return EISDIR;
    case CM_ERROR_READONLY:
	return EROFS;
    case CM_ERROR_WOULDBLOCK:
	return EWOULDBLOCK;
    case CM_ERROR_NOSUCHCELL:
	return ESRCH;		/* hack */
    case CM_ERROR_NOSUCHVOLUME:
	return EPIPE;		/* hack */
    case CM_ERROR_NOMORETOKENS:
	return EDOM;		/* hack */
    case CM_ERROR_TOOMANYBUFS:
	return EFBIG;		/* hack */
    case CM_ERROR_ALLBUSY:
        return EBUSY;
    case CM_ERROR_ALLDOWN:
        return ENOSYS;          /* hack */
    case CM_ERROR_ALLOFFLINE:
        return ENXIO;           /* hack */
    default:
	if (cm_code > 0 && cm_code < EILSEQ)
	    return cm_code;
	else
	    return ENOTTY;
    }
}

static void
InitFSRequest(fs_ioctlRequest_t * rp)
{
    rp->mp = rp->data;
    rp->nbytes = 0;
}

static BOOL
IoctlDebug(void)
{
    static int init = 0;
    static BOOL debug = 0;

    if ( !init ) {
        HKEY hk;

        if (RegOpenKey (HKEY_LOCAL_MACHINE,
                         TEXT("Software\\OpenAFS\\Client"), &hk) == 0)
        {
            DWORD dwSize = sizeof(BOOL);
            DWORD dwType = REG_DWORD;
            RegQueryValueEx (hk, TEXT("IoctlDebug"), NULL, &dwType, (PBYTE)&debug, &dwSize);
            RegCloseKey (hk);
        }

        init = 1;
    }

    return debug;
}

static BOOL
RDR_Ready(void)
{
    HANDLE hDevHandle = NULL;
    DWORD bytesReturned;
    AFSDriverStatusRespCB *respBuffer = NULL;
    DWORD rc = 0;
    BOOL ready = FALSE;

    hDevHandle = CreateFileW( AFS_SYMLINK_W,
                             GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL,
                             OPEN_EXISTING,
                             0,
                             NULL);
    if( hDevHandle == INVALID_HANDLE_VALUE)
    {
        DWORD gle = GetLastError();

        if (gle && IoctlDebug() ) {
            char buf[4096];
            int saveerrno;

            saveerrno = errno;
            if ( FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                               NULL,
                               gle,
                               MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),
                               buf,
                               4096,
                               NULL
                               ) )
            {
                fprintf(stderr,"RDR_Ready CreateFile(%S) failed: 0x%X\r\n\t[%s]\r\n",
                        AFS_SYMLINK_W,gle,buf);
            }
            errno = saveerrno;
        }
        return FALSE;
    }

    //
    // Allocate a response buffer.
    //
    respBuffer = calloc(1, sizeof( AFSDriverStatusRespCB));
    if( respBuffer)
    {

        if( !DeviceIoControl( hDevHandle,
                              IOCTL_AFS_STATUS_REQUEST,
                              NULL,
                              0,
                              (void *)respBuffer,
                              sizeof( AFSDriverStatusRespCB),
                              &bytesReturned,
                              NULL))
        {
            //
            // Error condition back from driver
            //
            DWORD gle = GetLastError();

            if (gle && IoctlDebug() ) {
                char buf[4096];
                int saveerrno;

                saveerrno = errno;
                if ( FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                                    NULL,
                                    gle,
                                    MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),
                                    buf,
                                    4096,
                                    NULL
                                    ) )
                {
                    fprintf(stderr,"RDR_Ready CreateFile(%s) failed: 0x%X\r\n\t[%s]\r\n",
                             AFS_SYMLINK,gle,buf);
                }
                errno = saveerrno;
            }
            rc = -1;
            goto cleanup;
        }

        if (bytesReturned == sizeof(AFSDriverStatusRespCB))
        {
            ready = ( respBuffer->Status == AFS_DRIVER_STATUS_READY );
        }
    } else
        rc = ENOMEM;

  cleanup:
    if (respBuffer)
        free( respBuffer);

    if (hDevHandle != INVALID_HANDLE_VALUE)
        CloseHandle(hDevHandle);

    return ready;
}

static BOOL
DisableServiceManagerCheck(void)
{
    static int init = 0;
    static BOOL smcheck = 0;

    if ( !init ) {
        HKEY hk;

        if (RegOpenKey (HKEY_LOCAL_MACHINE,
                         TEXT("Software\\OpenAFS\\Client"), &hk) == 0)
        {
            DWORD dwSize = sizeof(BOOL);
            DWORD dwType = REG_DWORD;
            RegQueryValueEx (hk, TEXT("DisableIoctlSMCheck"), NULL, &dwType, (PBYTE)&smcheck, &dwSize);
            RegCloseKey (hk);
        }

        init = 1;
    }

    return smcheck;
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

static BOOL
UnicodeToANSI(LPCWSTR lpInputString, LPSTR lpszOutputString, int nOutStringLen)
{
    CPINFO CodePageInfo;

    GetCPInfo(CP_ACP, &CodePageInfo);

    if (CodePageInfo.MaxCharSize > 1) {
        // Only supporting non-Unicode strings
        int reqLen = WideCharToMultiByte( CP_ACP, 0,
                                          lpInputString, -1,
                                          NULL, 0, NULL, NULL);
        if ( reqLen > nOutStringLen)
        {
            return FALSE;
        } else {
            if (WideCharToMultiByte( CP_ACP,
                                     WC_COMPOSITECHECK,
                                     lpInputString, -1,
                                     lpszOutputString,
                                     nOutStringLen, NULL, NULL) == 0)
                return FALSE;
        }
    }
    else
    {
        // Looks like unicode, better translate it
        if (WideCharToMultiByte( CP_ACP,
                                 WC_COMPOSITECHECK,
                                 lpInputString, -1,
                                 lpszOutputString,
                                 nOutStringLen, NULL, NULL) == 0)
            return FALSE;
    }

    return TRUE;
}

static BOOL
GetLSAPrincipalName(char * pszUser, DWORD dwUserSize)
{
    KERB_QUERY_TKT_CACHE_REQUEST CacheRequest;
    PKERB_RETRIEVE_TKT_RESPONSE pTicketResponse = NULL;
    ULONG ResponseSize;
    PKERB_EXTERNAL_NAME pClientName = NULL;
    PUNICODE_STRING     pDomainName = NULL;
    LSA_STRING Name;
    HANDLE hLogon = INVALID_HANDLE_VALUE;
    ULONG PackageId;
    NTSTATUS ntStatus;
    NTSTATUS ntSubStatus = 0;
    WCHAR * wchUser = NULL;
    DWORD   dwSize;
    SHORT   sCount;
    BOOL bRet = FALSE;

    ntStatus = LsaConnectUntrusted( &hLogon);
    if (FAILED(ntStatus))
        goto cleanup;

    Name.Buffer = MICROSOFT_KERBEROS_NAME_A;
    Name.Length = (USHORT)(sizeof(MICROSOFT_KERBEROS_NAME_A) - sizeof(char));
    Name.MaximumLength = Name.Length;

    ntStatus = LsaLookupAuthenticationPackage( hLogon, &Name, &PackageId);
    if (FAILED(ntStatus))
        goto cleanup;

    memset(&CacheRequest, 0, sizeof(KERB_QUERY_TKT_CACHE_REQUEST));
    CacheRequest.MessageType = KerbRetrieveTicketMessage;
    CacheRequest.LogonId.LowPart = 0;
    CacheRequest.LogonId.HighPart = 0;

    ntStatus = LsaCallAuthenticationPackage( hLogon,
                                             PackageId,
                                             &CacheRequest,
                                             sizeof(CacheRequest),
                                             &pTicketResponse,
                                             &ResponseSize,
                                             &ntSubStatus);
    if (FAILED(ntStatus) || FAILED(ntSubStatus))
        goto cleanup;

    /* We have a ticket in the response */
    pClientName = pTicketResponse->Ticket.ClientName;
    pDomainName = &pTicketResponse->Ticket.DomainName;

    /* We want to return ClientName @ DomainName */

    dwSize = 0;
    for ( sCount = 0; sCount < pClientName->NameCount; sCount++)
    {
        dwSize += pClientName->Names[sCount].Length;
    }
    dwSize += pDomainName->Length + sizeof(WCHAR);

    if ( dwSize / sizeof(WCHAR) > dwUserSize )
        goto cleanup;

    wchUser = malloc(dwSize);
    if (wchUser == NULL)
        goto cleanup;

    for ( sCount = 0, wchUser[0] = L'\0'; sCount < pClientName->NameCount; sCount++)
    {
        StringCbCatNW( wchUser, dwSize,
                       pClientName->Names[sCount].Buffer,
                       pClientName->Names[sCount].Length);
    }
    StringCbCatNW( wchUser, dwSize,
                   pDomainName->Buffer,
                   pDomainName->Length);

    if ( !UnicodeToANSI( wchUser, pszUser, dwUserSize) )
        goto cleanup;

    bRet = TRUE;

  cleanup:

    if (wchUser)
        free(wchUser);

    if ( hLogon != INVALID_HANDLE_VALUE)
        LsaDeregisterLogonProcess(hLogon);

    if ( pTicketResponse ) {
        SecureZeroMemory(pTicketResponse,ResponseSize);
        LsaFreeReturnBuffer(pTicketResponse);
    }

    return bRet;
}

//
// Recursively evaluate drivestr to find the final
// dos drive letter to which the source is mapped.
//
static BOOL
DriveSubstitution(char *drivestr, char *subststr, size_t substlen)
{
    char device[MAX_PATH];

    if ( QueryDosDevice(drivestr, device, MAX_PATH) )
    {
        if ( device[0] == '\\' &&
             device[1] == '?' &&
             device[2] == '?' &&
             device[3] == '\\' &&
             isalpha(device[4]) &&
             device[5] == ':')
        {
            device[0] = device[4];
            device[1] = ':';
            device[2] = '\0';
            if ( DriveSubstitution(device, subststr, substlen) )
            {
                return TRUE;
            } else {
                subststr[0] = device[0];
                subststr[1] = ':';
                subststr[2] = '\0';
                return TRUE;
            }
        } else
        if ( device[0] == '\\' &&
             device[1] == '?' &&
             device[2] == '?' &&
             device[3] == '\\' &&
             device[4] == 'U' &&
             device[5] == 'N' &&
             device[6] == 'C' &&
             device[7] == '\\')
        {
             subststr[0] = '\\';
             strncpy(&subststr[1], &device[7], substlen-1);
             subststr[substlen-1] = '\0';
             return TRUE;
        }
    }

    return FALSE;
}

//
// drivestr - is "<drive-letter>:"
//
static BOOL
DriveIsMappedToAFS(char *drivestr, char *NetbiosName)
{
    DWORD dwResult, dwResultEnum;
    HANDLE hEnum;
    DWORD cbBuffer = 16384;     // 16K is a good size
    DWORD cEntries = -1;        // enumerate all possible entries
    LPNETRESOURCE lpnrLocal;    // pointer to enumerated structures
    DWORD i;
    BOOL  bIsAFS = FALSE;
    char  subststr[MAX_PATH];
    char  device[MAX_PATH];

    //
    // Handle drive letter substitution created with "SUBST <drive> <path>".
    // If a substitution has occurred, use the target drive letter instead
    // of the source.
    //
    if ( DriveSubstitution(drivestr, subststr, MAX_PATH) )
    {
        if (subststr[0] == '\\' &&
            subststr[1] == '\\')
        {
            if (_strnicmp( &subststr[2], NetbiosName, strlen(NetbiosName)) == 0)
                return TRUE;
            else
                return FALSE;
        }
        drivestr = subststr;
    }

    //
    // Check for \Device\AFSRedirector
    //
    if (QueryDosDevice(drivestr, device, MAX_PATH) &&
        _strnicmp( device, "\\Device\\AFSRedirector", strlen("\\Device\\AFSRedirector")) == 0) {
        return TRUE;
    }

    //
    // Call the WNetOpenEnum function to begin the enumeration.
    //
    dwResult = WNetOpenEnum(RESOURCE_CONNECTED,
                            RESOURCETYPE_DISK,
                            RESOURCEUSAGE_ALL,
                            NULL,       // NULL first time the function is called
                            &hEnum);    // handle to the resource

    if (dwResult != NO_ERROR)
        return FALSE;

    //
    // Call the GlobalAlloc function to allocate resources.
    //
    lpnrLocal = (LPNETRESOURCE) GlobalAlloc(GPTR, cbBuffer);
    if (lpnrLocal == NULL)
        return FALSE;

    do {
        //
        // Initialize the buffer.
        //
        ZeroMemory(lpnrLocal, cbBuffer);
        //
        // Call the WNetEnumResource function to continue
        //  the enumeration.
        //
        cEntries = -1;
        dwResultEnum = WNetEnumResource(hEnum,          // resource handle
                                        &cEntries,      // defined locally as -1
                                        lpnrLocal,      // LPNETRESOURCE
                                        &cbBuffer);     // buffer size
        //
        // If the call succeeds, loop through the structures.
        //
        if (dwResultEnum == NO_ERROR) {
            for (i = 0; i < cEntries; i++) {
                if (lpnrLocal[i].lpLocalName &&
                    toupper(lpnrLocal[i].lpLocalName[0]) == toupper(drivestr[0])) {
                    //
                    // Skip the two backslashes at the start of the UNC device name
                    //
                    if ( _strnicmp( &(lpnrLocal[i].lpRemoteName[2]), NetbiosName, strlen(NetbiosName)) == 0 )
                    {
                        bIsAFS = TRUE;
                        break;
                    }
                }
            }
        }
        // Process errors.
        //
        else if (dwResultEnum != ERROR_NO_MORE_ITEMS)
            break;
    }
    while (dwResultEnum != ERROR_NO_MORE_ITEMS);

    //
    // Call the GlobalFree function to free the memory.
    //
    GlobalFree((HGLOBAL) lpnrLocal);
    //
    // Call WNetCloseEnum to end the enumeration.
    //
    dwResult = WNetCloseEnum(hEnum);

    return bIsAFS;
}

static BOOL
DriveIsGlobalAutoMapped(char *drivestr)
{
    DWORD dwResult;
    HKEY hKey;
    DWORD dwSubMountSize;
    char szSubMount[260];
    DWORD dwType;

    dwResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                            AFSREG_CLT_SVC_PARAM_SUBKEY "\\GlobalAutoMapper",
                            0, KEY_QUERY_VALUE, &hKey);
    if (dwResult != ERROR_SUCCESS)
        return FALSE;

    dwSubMountSize = sizeof(szSubMount);
    dwType = REG_SZ;
    dwResult = RegQueryValueEx(hKey, drivestr, 0, &dwType, szSubMount, &dwSubMountSize);
    RegCloseKey(hKey);

    if (dwResult == ERROR_SUCCESS && dwType == REG_SZ)
        return TRUE;
    else
        return FALSE;
}

static long
GetIoctlHandle(char *fileNamep, HANDLE * handlep)
{
    HKEY hk;
    char *drivep = NULL;
    char netbiosName[MAX_NB_NAME_LENGTH]="AFS";
    DWORD CurrentState = 0;
    char  HostName[64] = "";
    char tbuffer[MAX_PATH]="";
    HANDLE fh;
    char szUser[128] = "";
    char szClient[MAX_PATH] = "";
    char szPath[MAX_PATH] = "";
    NETRESOURCE nr;
    DWORD res;
    DWORD ioctlDebug = IoctlDebug();
    DWORD gle;
    DWORD dwAttrib;
    DWORD dwSize = sizeof(szUser);
    BOOL  usingRDR = FALSE;
    int saveerrno;
    UINT driveType;
    int sharingViolation;

    memset(HostName, '\0', sizeof(HostName));
    gethostname(HostName, sizeof(HostName));
    if (!DisableServiceManagerCheck() &&
        GetServiceStatus(HostName, TEXT("TransarcAFSDaemon"), &CurrentState) == NOERROR &&
        CurrentState != SERVICE_RUNNING)
    {
        if ( ioctlDebug ) {
            saveerrno = errno;
            fprintf(stderr, "pioctl GetServiceStatus(%s) == %d\r\n",
                    HostName, CurrentState);
            errno = saveerrno;
        }
	return -1;
    }

    if (RDR_Ready()) {
        usingRDR = TRUE;

        if ( ioctlDebug ) {
            saveerrno = errno;
            fprintf(stderr, "pioctl Redirector is ready\r\n");
            errno = saveerrno;
        }

        if (RegOpenKey (HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY, &hk) == 0)
        {
            DWORD dwSize = sizeof(netbiosName);
            DWORD dwType = REG_SZ;
            RegQueryValueExA (hk, "NetbiosName", NULL, &dwType, (PBYTE)netbiosName, &dwSize);
            RegCloseKey (hk);

            if ( ioctlDebug ) {
                saveerrno = errno;
                fprintf(stderr, "pioctl NetbiosName = \"%s\"\r\n", netbiosName);
                errno = saveerrno;
            }
        } else {
            if ( ioctlDebug ) {
                saveerrno = errno;
                gle = GetLastError();
                fprintf(stderr, "pioctl Unable to open \"HKLM\\%s\" using NetbiosName = \"AFS\" GLE=0x%x\r\n",
                        HostName, CurrentState, gle);
                errno = saveerrno;
            }
        }
    } else {
        if ( ioctlDebug ) {
            saveerrno = errno;
            fprintf(stderr, "pioctl Redirector is not ready\r\n");
            errno = saveerrno;
        }

        if (!GetEnvironmentVariable("AFS_PIOCTL_SERVER", netbiosName, sizeof(netbiosName)))
            lana_GetNetbiosName(netbiosName,LANA_NETBIOS_NAME_FULL);

        if ( ioctlDebug ) {
            saveerrno = errno;
            fprintf(stderr, "pioctl NetbiosName = \"%s\"\r\n", netbiosName);
            errno = saveerrno;
        }
    }

    if (fileNamep) {
        drivep = strchr(fileNamep, ':');
        if (drivep && (drivep - fileNamep) >= 1) {
            tbuffer[0] = *(drivep - 1);
            tbuffer[1] = ':';
            tbuffer[2] = '\0';

            driveType = GetDriveType(tbuffer);
            switch (driveType) {
            case DRIVE_UNKNOWN:
            case DRIVE_REMOTE:
                if (DriveIsMappedToAFS(tbuffer, netbiosName) ||
                    DriveIsGlobalAutoMapped(tbuffer))
                    strcpy(&tbuffer[2], SMB_IOCTL_FILENAME);
                else
                    return -1;
                break;
            default:
                if (DriveIsGlobalAutoMapped(tbuffer))
                    strcpy(&tbuffer[2], SMB_IOCTL_FILENAME);
                else
                    return -1;
            }
        } else if (fileNamep[0] == fileNamep[1] &&
		   (fileNamep[0] == '\\' || fileNamep[0] == '/'))
        {
            int count = 0, i = 0;

            while (count < 4 && fileNamep[i]) {
                tbuffer[i] = fileNamep[i];
                if ( tbuffer[i] == '\\' ||
		     tbuffer[i] == '/')
                    count++;
		i++;
            }
            if (fileNamep[i] == 0 || (fileNamep[i-1] != '\\' && fileNamep[i-1] != '/'))
                tbuffer[i++] = '\\';
            tbuffer[i] = 0;
            strcat(tbuffer, SMB_IOCTL_FILENAME_NOSLASH);
        } else {
            char curdir[MAX_PATH]="";

            GetCurrentDirectory(sizeof(curdir), curdir);
            if ( curdir[1] == ':' ) {
                tbuffer[0] = curdir[0];
                tbuffer[1] = ':';
                tbuffer[2] = '\0';

                driveType = GetDriveType(tbuffer);
                switch (driveType) {
                case DRIVE_UNKNOWN:
                case DRIVE_REMOTE:
                    if (DriveIsMappedToAFS(tbuffer, netbiosName) ||
                        DriveIsGlobalAutoMapped(tbuffer))
                        strcpy(&tbuffer[2], SMB_IOCTL_FILENAME);
                    else
                        return -1;
                    break;
                default:
                    if (DriveIsGlobalAutoMapped(tbuffer))
                        strcpy(&tbuffer[2], SMB_IOCTL_FILENAME);
                    else
                        return -1;
                }
            } else if (curdir[0] == curdir[1] &&
                       (curdir[0] == '\\' || curdir[0] == '/'))
            {
                int count = 0, i = 0;

                while (count < 4 && curdir[i]) {
                    tbuffer[i] = curdir[i];
		    if ( tbuffer[i] == '\\' ||
			 tbuffer[i] == '/')
			count++;
		    i++;
                }
                if (curdir[i] == 0 || (curdir[i-1] != '\\' && curdir[i-1] != '/'))
                    tbuffer[i++] = '\\';
                tbuffer[i] = 0;
                strcat(tbuffer, SMB_IOCTL_FILENAME_NOSLASH);
            }
        }
    }
    if (!tbuffer[0]) {
        /* No file name starting with drive colon specified, use UNC name */
        sprintf(tbuffer,"\\\\%s\\all%s",netbiosName,SMB_IOCTL_FILENAME);
    }

    if ( ioctlDebug ) {
        saveerrno = errno;
        fprintf(stderr, "pioctl filename = \"%s\"\r\n", tbuffer);
        errno = saveerrno;
    }

    fflush(stdout);

    /*
     * Try to find the correct path and authentication
     */
    dwAttrib = GetFileAttributes(tbuffer);
    if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
        int  gonext = 0;

        gle = GetLastError();
        if (gle && ioctlDebug ) {
            char buf[4096];

            saveerrno = errno;
            if ( FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                               NULL,
                               gle,
                               MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),
                               buf,
                               4096,
                               NULL
                               ) )
            {
                fprintf(stderr,"pioctl GetFileAttributes(%s) failed: 0x%X\r\n\t[%s]\r\n",
                        tbuffer,gle,buf);
            }
            errno = saveerrno;
            SetLastError(gle);
        }

        /* with the redirector interface, fail immediately.  there is nothing to retry */
        if (usingRDR)
            return -1;

        if (!GetEnvironmentVariable("AFS_PIOCTL_SERVER", szClient, sizeof(szClient)))
            lana_GetNetbiosName(szClient, LANA_NETBIOS_NAME_FULL);

        if (RegOpenKey (HKEY_CURRENT_USER,
                         TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"), &hk) == 0)
        {
            DWORD dwType = REG_SZ;
            RegQueryValueEx (hk, TEXT("Logon User Name"), NULL, &dwType, (PBYTE)szUser, &dwSize);
            RegCloseKey (hk);
        }

        if ( szUser[0] ) {
            if ( ioctlDebug ) {
                saveerrno = errno;
                fprintf(stderr, "pioctl Explorer logon user: [%s]\r\n",szUser);
                errno = saveerrno;
            }
            sprintf(szPath, "\\\\%s", szClient);
            memset (&nr, 0x00, sizeof(NETRESOURCE));
            nr.dwType=RESOURCETYPE_DISK;
            nr.lpLocalName=0;
            nr.lpRemoteName=szPath;
            res = WNetAddConnection2(&nr,NULL,szUser,0);
            if (res) {
                if ( ioctlDebug ) {
                    saveerrno = errno;
                    fprintf(stderr, "pioctl WNetAddConnection2(%s,%s) failed: 0x%X\r\n",
                             szPath,szUser,res);
                    errno = saveerrno;
                }
                gonext = 1;
            }

            sprintf(szPath, "\\\\%s\\all", szClient);
            res = WNetAddConnection2(&nr,NULL,szUser,0);
            if (res) {
                if ( ioctlDebug ) {
                    saveerrno = errno;
                    fprintf(stderr, "pioctl WNetAddConnection2(%s,%s) failed: 0x%X\r\n",
                             szPath,szUser,res);
                    errno = saveerrno;
                }
                gonext = 1;
            }

            if (gonext)
                goto try_lsa_principal;

            dwAttrib = GetFileAttributes(tbuffer);
            if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
                gle = GetLastError();
                if (gle && ioctlDebug ) {
                    char buf[4096];

                    saveerrno = errno;
                    if ( FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                                        NULL,
                                        gle,
                                        MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),
                                        buf,
                                        4096,
                                        NULL
                                        ) )
                    {
                        fprintf(stderr,"pioctl GetFileAttributes(%s) failed: 0x%X\r\n\t[%s]\r\n",
                                 tbuffer,gle,buf);
                    }
                    errno = saveerrno;
                    SetLastError(gle);
                }
            }
        }
    }

  try_lsa_principal:
    if (!usingRDR &&
        dwAttrib == INVALID_FILE_ATTRIBUTES) {
        int  gonext = 0;

        dwSize = sizeof(szUser);
        if (GetLSAPrincipalName(szUser, dwSize)) {
            if ( ioctlDebug ) {
                saveerrno = errno;
                fprintf(stderr, "pioctl LSA Principal logon user: [%s]\r\n",szUser);
                errno = saveerrno;
            }
            sprintf(szPath, "\\\\%s", szClient);
            memset (&nr, 0x00, sizeof(NETRESOURCE));
            nr.dwType=RESOURCETYPE_DISK;
            nr.lpLocalName=0;
            nr.lpRemoteName=szPath;
            res = WNetAddConnection2(&nr,NULL,szUser,0);
            if (res) {
                if ( ioctlDebug ) {
                    saveerrno = errno;
                    fprintf(stderr, "pioctl WNetAddConnection2(%s,%s) failed: 0x%X\r\n",
                             szPath,szUser,res);
                    errno = saveerrno;
                }
                gonext = 1;
            }

            sprintf(szPath, "\\\\%s\\all", szClient);
            res = WNetAddConnection2(&nr,NULL,szUser,0);
            if (res) {
                if ( ioctlDebug ) {
                    saveerrno = errno;
                    fprintf(stderr, "pioctl WNetAddConnection2(%s,%s) failed: 0x%X\r\n",
                             szPath,szUser,res);
                    errno = saveerrno;
                }
                gonext = 1;
            }

            if (gonext)
                goto try_sam_compat;

            dwAttrib = GetFileAttributes(tbuffer);
            if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
                gle = GetLastError();
                if (gle && ioctlDebug ) {
                    char buf[4096];

                    saveerrno = errno;
                    if ( FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                                        NULL,
                                        gle,
                                        MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),
                                        buf,
                                        4096,
                                        NULL
                                        ) )
                    {
                        fprintf(stderr,"pioctl GetFileAttributes(%s) failed: 0x%X\r\n\t[%s]\r\n",
                                 tbuffer,gle,buf);
                    }
                    errno = saveerrno;
                    SetLastError(gle);
                }
            }
        }
    }

  try_sam_compat:
    if (!usingRDR &&
        dwAttrib == INVALID_FILE_ATTRIBUTES) {
        dwSize = sizeof(szUser);
        if (GetUserNameEx(NameSamCompatible, szUser, &dwSize)) {
            if ( ioctlDebug ) {
                saveerrno = errno;
                fprintf(stderr, "pioctl SamCompatible logon user: [%s]\r\n",szUser);
                errno = saveerrno;
            }
            sprintf(szPath, "\\\\%s", szClient);
            memset (&nr, 0x00, sizeof(NETRESOURCE));
            nr.dwType=RESOURCETYPE_DISK;
            nr.lpLocalName=0;
            nr.lpRemoteName=szPath;
            res = WNetAddConnection2(&nr,NULL,szUser,0);
            if (res) {
                if ( ioctlDebug ) {
                    saveerrno = errno;
                    fprintf(stderr, "pioctl WNetAddConnection2(%s,%s) failed: 0x%X\r\n",
                             szPath,szUser,res);
                    errno = saveerrno;
                }
            }

            sprintf(szPath, "\\\\%s\\all", szClient);
            res = WNetAddConnection2(&nr,NULL,szUser,0);
            if (res) {
                if ( ioctlDebug ) {
                    saveerrno = errno;
                    fprintf(stderr, "pioctl WNetAddConnection2(%s,%s) failed: 0x%X\r\n",
                             szPath,szUser,res);
                    errno = saveerrno;
                }
                return -1;
            }

            dwAttrib = GetFileAttributes(tbuffer);
            if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
                gle = GetLastError();
                if (gle && ioctlDebug ) {
                    char buf[4096];

                    saveerrno = errno;
                    if ( FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                                        NULL,
                                        gle,
                                        MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),
                                        buf,
                                        4096,
                                        NULL
                                        ) )
                    {
                        fprintf(stderr,"pioctl GetFileAttributes(%s) failed: 0x%X\r\n\t[%s]\r\n",
                                 tbuffer,gle,buf);
                    }
                    errno = saveerrno;
                }
                return -1;
            }
        } else {
            fprintf(stderr, "GetUserNameEx(NameSamCompatible) failed: 0x%X\r\n", GetLastError());
            return -1;
        }
    }

    if ( dwAttrib != (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) {
        fprintf(stderr, "GetFileAttributes(%s) returned: 0x%08X\r\n",
                tbuffer, dwAttrib);
        return -1;
    }

    /* tbuffer now contains the correct path; now open the file */
    sharingViolation = 0;
    do {
        if (sharingViolation)
            Sleep(100);
        fh = CreateFile(tbuffer, FILE_READ_DATA | FILE_WRITE_DATA,
                        FILE_SHARE_READ, NULL, OPEN_EXISTING,
                        FILE_FLAG_WRITE_THROUGH, NULL);
        sharingViolation++;
    } while (fh == INVALID_HANDLE_VALUE &&
             GetLastError() == ERROR_SHARING_VIOLATION &&
             sharingViolation < 100);
    fflush(stdout);

    if (fh == INVALID_HANDLE_VALUE)
        return -1;

    /* return fh and success code */
    *handlep = fh;
    return 0;
}

static long
Transceive(HANDLE handle, fs_ioctlRequest_t * reqp)
{
    long rcount;
    long ioCount;
    DWORD gle;
    DWORD ioctlDebug = IoctlDebug();
    int save;

    rcount = (long)(reqp->mp - reqp->data);
    if (rcount <= 0) {
        if ( ioctlDebug ) {
            save = errno;
            fprintf(stderr, "pioctl Transceive rcount <= 0: %ld\r\n",rcount);
            errno = save;
        }
	return EINVAL;		/* not supposed to happen */
    }

    if (!WriteFile(handle, reqp->data, rcount, &ioCount, NULL)) {
	/* failed to write */
	gle = GetLastError();

        if ( ioctlDebug ) {
            save = errno;
            fprintf(stderr, "pioctl Transceive WriteFile failed: 0x%X\r\n",gle);
            errno = save;
        }
        return gle;
    }

    if (!ReadFile(handle, reqp->data, sizeof(reqp->data), &ioCount, NULL)) {
	/* failed to read */
	gle = GetLastError();

        if ( ioctlDebug ) {
            save = errno;
            fprintf(stderr, "pioctl Transceive ReadFile failed: 0x%X\r\n",gle);
            errno = save;
        }
	if (gle == ERROR_NOT_SUPPORTED) {
	    errno = EINVAL;
	    return -1;
	}
        return gle;
    }

    reqp->nbytes = ioCount;	/* set # of bytes available */
    reqp->mp = reqp->data;	/* restart marshalling */

    /* return success */
    return 0;
}

static long
MarshallLong(fs_ioctlRequest_t * reqp, long val)
{
    memcpy(reqp->mp, &val, 4);
    reqp->mp += 4;
    return 0;
}

static long
UnmarshallLong(fs_ioctlRequest_t * reqp, long *valp)
{
    int save;

    /* not enough data left */
    if (reqp->nbytes < 4) {
        if ( IoctlDebug() ) {
            save = errno;
            fprintf(stderr, "pioctl UnmarshallLong reqp->nbytes < 4: %ld\r\n",
                     reqp->nbytes);
            errno = save;
        }
	return -1;
    }

    memcpy(valp, reqp->mp, 4);
    reqp->mp += 4;
    reqp->nbytes -= 4;
    return 0;
}

/* includes marshalling NULL pointer as a null (0 length) string */
static long
MarshallString(fs_ioctlRequest_t * reqp, char *stringp, int is_utf8)
{
    int count;
    int save;

    if (stringp)
	count = (int)strlen(stringp) + 1;/* space required including null */
    else
	count = 1;

    if (is_utf8) {
        count += utf8_prefix_size;
    }

    /* watch for buffer overflow */
    if ((reqp->mp - reqp->data) + count > sizeof(reqp->data)) {
        if ( IoctlDebug() ) {
            save = errno;
            fprintf(stderr, "pioctl MarshallString buffer overflow\r\n");
            errno = save;
        }
	return -1;
    }

    if (is_utf8) {
        memcpy(reqp->mp, utf8_prefix, utf8_prefix_size);
        reqp->mp += utf8_prefix_size;
        count -= utf8_prefix_size;
    }

    if (stringp)
	memcpy(reqp->mp, stringp, count);
    else
	*(reqp->mp) = 0;
    reqp->mp += count;
    return 0;
}

/* take a path with a drive letter, possibly relative, and return a full path
 * without the drive letter.  This is the full path relative to the working
 * dir for that drive letter.  The input and output paths can be the same.
 */
static long
fs_GetFullPath(char *pathp, char *outPathp, long outSize)
{
    char tpath[1000];
    char origPath[1000];
    char *firstp;
    long code;
    int pathHasDrive;
    int doSwitch;
    char newPath[3];
    char * p;
    int save;

    if (pathp[0] != 0 && pathp[1] == ':') {
	/* there's a drive letter there */
	firstp = pathp + 2;
	pathHasDrive = 1;
    } else {
	firstp = pathp;
	pathHasDrive = 0;
    }

    if ( firstp[0] == '\\' && firstp[1] == '\\' ||
	 firstp[0] == '/' && firstp[1] == '/') {
        /* UNC path - strip off the server and sharename */
        int i, count;
        for ( i=2,count=2; count < 4 && firstp[i]; i++ ) {
            if ( firstp[i] == '\\' || firstp[i] == '/' ) {
                count++;
            }
        }
        if ( firstp[i] == 0 ) {
            strcpy(outPathp,"\\");
        } else {
            strcpy(outPathp,&firstp[--i]);
        }
	for (p=outPathp ;*p; p++) {
	    if (*p == '/')
		*p = '\\';
	}
        return 0;
    } else if (firstp[0] == '\\' || firstp[0] == '/') {
        /* already an absolute pathname, just copy it back */
        strcpy(outPathp, firstp);
	for (p=outPathp ;*p; p++) {
	    if (*p == '/')
		*p = '\\';
	}
        return 0;
    }

    GetCurrentDirectory(sizeof(origPath), origPath);

    doSwitch = 0;
    if (pathHasDrive && (*pathp & ~0x20) != (origPath[0] & ~0x20)) {
	/* a drive has been specified and it isn't our current drive.
	 * to get path, switch to it first.  Must case-fold drive letters
	 * for user convenience.
	 */
	doSwitch = 1;
	newPath[0] = *pathp;
	newPath[1] = ':';
	newPath[2] = 0;
	if (!SetCurrentDirectory(newPath)) {
	    code = GetLastError();

            if ( IoctlDebug() ) {
                save = errno;
                fprintf(stderr, "pioctl fs_GetFullPath SetCurrentDirectory(%s) failed: 0x%X\r\n",
                         newPath, code);
                errno = save;
            }
	    return code;
	}
    }

    /* now get the absolute path to the current wdir in this drive */
    GetCurrentDirectory(sizeof(tpath), tpath);
    if (tpath[1] == ':')
        strcpy(outPathp, tpath + 2);	/* skip drive letter */
    else if ( tpath[0] == '\\' && tpath[1] == '\\'||
	      tpath[0] == '/' && tpath[1] == '/') {
        /* UNC path - strip off the server and sharename */
        int i, count;
        for ( i=2,count=2; count < 4 && tpath[i]; i++ ) {
            if ( tpath[i] == '\\' || tpath[i] == '/' ) {
                count++;
            }
        }
        if ( tpath[i] == 0 ) {
            strcpy(outPathp,"\\");
        } else {
            strcpy(outPathp,&tpath[--i]);
        }
    } else {
        /* this should never happen */
        strcpy(outPathp, tpath);
    }

    /* if there is a non-null name after the drive, append it */
    if (*firstp != 0) {
        int len = (int)strlen(outPathp);
        if (outPathp[len-1] != '\\' && outPathp[len-1] != '/')
            strcat(outPathp, "\\");
        strcat(outPathp, firstp);
    }

    /* finally, if necessary, switch back to our home drive letter */
    if (doSwitch) {
	SetCurrentDirectory(origPath);
    }

    for (p=outPathp ;*p; p++) {
	if (*p == '/')
	    *p = '\\';
    }
    return 0;
}

static int
pioctl_int(char *pathp, afs_int32 opcode, struct ViceIoctl *blobp, afs_int32 follow, afs_int32 is_utf8)
{
    fs_ioctlRequest_t preq;
    long code;
    long temp;
    char fullPath[1000];
    char altPath[1024];
    HANDLE reqHandle;
    int save;
    int i,j,count,all;

    /*
     * The pioctl operations for creating a mount point and a symlink are broken.
     * Instead of 'pathp' referring to the directory object in which the symlink
     * or mount point within which the new object is to be created, 'pathp' refers
     * to the object itself.  This results in a problem when the object being created
     * is located within the Freelance root.afs volume.  \\afs\foo will not be a
     * valid share name since the 'foo' object does not yet exist.  Therefore,
     * \\afs\foo\_._.afs_ioctl_._ cannot be opened.  Instead in these two cases
     * we must force the use of the \\afs\all\foo form of the path.
     *
     * We cannot use this form in all cases because of smb submounts which are
     * not located within the Freelance local root.
     */
    switch ( opcode ) {
    case VIOC_AFS_CREATE_MT_PT:
    case VIOC_SYMLINK:
        if (pathp &&
             (pathp[0] == '\\' && pathp[1] == '\\' ||
              pathp[0] == '/' && pathp[1] == '/')) {
            for (all = count = j = 0; pathp[j]; j++) {
                if (pathp[j] == '\\' || pathp[j] == '/')
                    count++;

                /* Test to see if the second component is 'all' */
                if (count == 3) {
                    all = 1;
                    for (i=0; pathp[i+j]; i++) {
                        switch(i) {
                        case 0:
                            if (pathp[i+j] != 'a' &&
                                pathp[i+j] != 'A') {
                                all = 0;
                                goto notall;
                            }
                            break;
                        case 1:
                        case 2:
                            if (pathp[i+j] != 'l' &&
                                 pathp[i+j] != 'L') {
                                all = 0;
                                goto notall;
                            }
                            break;
                        default:
                            all = 0;
                            goto notall;
                        }
                    }
                    if (i != 3)
                        all = 0;
                }

              notall:
                if (all)
                    break;
            }

            /*
             * if count is three and the second component is not 'all',
             * then we are attempting to create an object in the
             * Freelance root.afs volume.  Substitute the path.
             */

            if (count == 3 && !all) {
                /* Normalize the name to use \\afs\all as the root */
                for (count = i = j = 0; pathp[j] && i < sizeof(altPath); j++) {
                    if (pathp[j] == '\\' || pathp[j] == '/') {
                        altPath[i++] = '\\';
                        count++;

                        if (count == 3) {
                            altPath[i++] = 'a';
                            altPath[i++] = 'l';
                            altPath[i++] = 'l';
                            altPath[i++] = '\\';
                            count++;
                        }
                    } else {
                        altPath[i++] = pathp[j];
                    }
                }
                altPath[i] = '\0';
                pathp = altPath;
            }
        }
    }

    code = GetIoctlHandle(pathp, &reqHandle);
    if (code) {
	if (pathp)
	    errno = EINVAL;
	else
	    errno = ENODEV;
	return code;
    }

    /* init the request structure */
    InitFSRequest(&preq);

    /* marshall the opcode, the path name and the input parameters */
    MarshallLong(&preq, opcode);
    /* when marshalling the path, remove the drive letter, since we already
     * used the drive letter to find the AFS daemon; we don't need it any more.
     * Eventually we'll expand relative path names here, too, since again, only
     * we understand those.
     */
    if (pathp) {
	code = fs_GetFullPath(pathp, fullPath, sizeof(fullPath));
	if (code) {
	    CloseHandle(reqHandle);
	    errno = EINVAL;
	    return code;
	}
    } else {
	strcpy(fullPath, "");
    }

    MarshallString(&preq, fullPath, is_utf8);
    if (blobp->in_size) {
        if (blobp->in_size > sizeof(preq.data) - (preq.mp - preq.data)*sizeof(char)) {
            errno = E2BIG;
            return -1;
        }
	memcpy(preq.mp, blobp->in, blobp->in_size);
	preq.mp += blobp->in_size;
    }

    /* now make the call */
    code = Transceive(reqHandle, &preq);
    if (code) {
	CloseHandle(reqHandle);
	return code;
    }

    /* now unmarshall the return value */
    if (UnmarshallLong(&preq, &temp) != 0) {
        CloseHandle(reqHandle);
        return -1;
    }

    if (temp != 0) {
	CloseHandle(reqHandle);
	errno = CMtoUNIXerror(temp);
        if ( IoctlDebug() ) {
            save = errno;
            fprintf(stderr, "pioctl temp != 0: 0x%X\r\n",temp);
            errno = save;
        }
	return -1;
    }

    /* otherwise, unmarshall the output parameters */
    if (blobp->out_size) {
	temp = blobp->out_size;
	if (preq.nbytes < temp)
	    temp = preq.nbytes;
	memcpy(blobp->out, preq.mp, temp);
	blobp->out_size = temp;
    }

    /* and return success */
    CloseHandle(reqHandle);
    return 0;
}

int
pioctl_utf8(char * pathp, afs_int32 opcode, struct ViceIoctl * blobp, afs_int32 follow)
{
    return pioctl_int(pathp, opcode, blobp, follow, TRUE);
}

int
pioctl(char * pathp, afs_int32 opcode, struct ViceIoctl * blobp, afs_int32 follow)
{
    return pioctl_int(pathp, opcode, blobp, follow, FALSE);
}

