/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * INCLUDES _________________________________________________________________
 *
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
#include <afs/fileutil.h>
}

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <io.h>
#include <string.h>
#include <SYS\STAT.H>
#include <shellapi.h>

#include <WINNT/afsreg.h>
#include <WINNT/afssw.h>
#include <WINNT/talocale.h>

#include "resource.h"
#include "progress_dlg.h"
#include "sutil.h"
#include "forceremove.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static char *GetAppInstallDir(struct APPINFO *pApp, BOOL bRemembered);
BOOL UninstallCredsTool();
BOOL ServerSpecificUninstall();
BOOL ClientSpecificUninstall();



/*
 * DEFINITIONS _________________________________________________________________
 *
 */
#define SUCALLCONV  WINAPI

#define UNINSTALL_TEMP_INFO_KEY     "HKEY_LOCAL_MACHINE\\Software\\AfsUninstallTempInfo"
#define INSTALL_DIR_VALUE_NAME      "InstallDir"

#define AFS_PRESERVED_CFG_INFO_KEY  "HKEY_LOCAL_MACHINE\\Software\\AfsPreservedConfigInfo"

#define MS_SHARED_FILES_KEY         "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\SharedDLLs"

// Log file to use when running in silent mode
#define UNINSTALL_ERROR_LOG_NAME    "\\AfsUninstallErrorLog.txt"
#define INSTALL_ERROR_LOG_NAME      "\\AfsInstallErrorLog.txt"

#define TARGETDIR                   "<TARGETDIR>"
#define WINDIR                      "<WINDIR>"
#define WINSYSDIR                   "<WINSYSDIR>"

#define WIN9X_START_MENU_REG_KEY    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"
#define WIN9X_START_MENU_REG_VALUE  "Programs"
    
#define WINNT_START_MENU_REG_KEY    "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"
#define WINNT_START_MENU_REG_VALUE  "Common Programs"

#define LOCALE_ID_LEN               4

struct REGVALUE {
    char *pszKey;
    char *pszValue;
};


typedef BOOL (APP_UNINSTALL_FUNC)();



struct APPINFO {
    char *pszAppName;

    // Service Info
    char *pszSvcName;
    char *pszSvcKey;
    char *pszSvcDependOn;
    char *pszSvcDisplayName;

    char *pszNetworkProviderOrder;

    // Message to use to tell the user that we have to stop the service
    int nServiceShutdownMsgID;

    // Message to use for the progress dialog that is shown while
    // waiting for the service to stop.
    int nServiceShutdownProgressMsgID;

    // Location in registry of a key we can use to know that the app is installed
    char *pszAppKey;

    // Location in registry of this app's install dir
    struct REGVALUE regInstallDir;

    // Path Info
    char *pszLocalRoot;     // The root dir below the install dir
    char *pszBinPath;       // Path to remove from the system path

    // Generated files and directories to delete.  These are both multistring lists.
    char *pszDirsToDel;     // All files in these dirs will be deleted
    char *pszFilesToDel;    // Use this if you want to delete files but leave the dir.  Wildcards can be used.

    // Registry values to remove
    struct REGVALUE *pRegValues;
    struct REGVALUE *pWinNTRegValues;   // Only remove these if running WinNT
    struct REGVALUE *pWin9XRegValues;   // Only remove these if running Win9X

    // Start menu entries to delete
    char *pszStartMenuEntries;

    // Registry keys to save if a user wants to preserve config info during uninstall
    char *pszRegKeysToPreserve;
    int nPreserveConfigInfoMsgID;

    // Uninstall func - used for things specific to this app
    APP_UNINSTALL_FUNC *pUninstallFunc;
};


/*
 * App info structure for the Server product
 */
struct APPINFO appServer = {
    "AFS Server",
    
    AFSREG_SVR_SVC_NAME,
    AFSREG_SVR_SVC_KEY,
    0,  // No depend on
    AFSREG_SVR_SVC_DISPLAYNAME_DATA,

    0,  // No network provider order

    IDS_MUST_STOP_SERVER,
    IDS_WAITING_FOR_SERVER_TO_STOP,

    AFSREG_SVR_SW_VERSION_KEY,
    
    { AFSREG_SVR_SW_VERSION_KEY, AFSREG_SVR_SW_VERSION_DIR_VALUE },

    "\\Server",
    "\\usr\\afs\\bin",

    // Dirs to delete
    TARGETDIR"\\Server\\usr\\afs\\bin\\backup\0"
    TARGETDIR"\\Server\\usr\\afs\\bin\0"
    TARGETDIR"\\Server\\usr\\afs\\db\0"
    TARGETDIR"\\Server\\usr\\afs\\logs\0"
    TARGETDIR"\\Server\\usr\\afs\\etc\0"
    TARGETDIR"\\Server\\usr\\afs\\local\0"
    TARGETDIR"\\Server\\usr\\afs\0"
    TARGETDIR"\\Server\\usr\0",
    
    // Files to delete
    TARGETDIR"\\Common\\*.gid\0"
    TARGETDIR"\\Common\\*.fts\0",

    0,  // No reg values
    0,  // No NT only reg values
    0,  // No 9x only reg values

    "Server\0",

    // Config info to preserve
    AFSREG_SVR_SVC_KEY"\0", 
    IDS_PRESERVE_SERVER_CONFIG_INFO,

    0   // No special uninstall function
};

// Registry values to remove for the Client
struct REGVALUE clientRegValues[] = {
    { "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", "{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}" },
    { 0, 0 }    // This indicates there are no more entries
};

struct REGVALUE clientWinNTRegValues[] = {
    { "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\File Manager\\AddOns", "AFS Client FME" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\NetBT\\Parameters", "SMBDeviceEnabled" },
    { 0, 0 }
};

struct REGVALUE clientWin9XRegValues[] = {
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\NetworkProvider\\Order", "TransarcAFSDaemon" },
    { 0, 0 }
};

/*
 * App info structure for the Client product
 */
struct APPINFO appClient = {
    "AFS Client",
    
    AFSREG_CLT_SVC_NAME,
    AFSREG_CLT_SVC_KEY,
    "5250435353004E657462696F730000",
    AFSREG_CLT_SVC_DISPLAYNAME_DATA,

    AFSREG_CLT_SVC_NAME,

    IDS_MUST_STOP_CLIENT,
    IDS_WAITING_FOR_CLIENT_TO_STOP,

    AFSREG_CLT_SW_VERSION_KEY,

    { AFSREG_CLT_SW_VERSION_KEY, AFSREG_CLT_SW_VERSION_DIR_VALUE },

    "\\Client",
    "\\Program",

    // No dirs to delete
    0,
    
    // Files to delete
    TARGETDIR"\\Common\\*.gid\0"
    TARGETDIR"\\Common\\*.fts\0"
    WINDIR"\\..\\AFSCache\0"
    WINDIR"\\TEMP\\afsd.log\0"
    TARGETDIR"\\Client\\afsd.ini\0"
    TARGETDIR"\\Client\\afsdsbmt.ini\0"
    TARGETDIR"\\Client\\afsdcell.ini\0"
    WINDIR"\\TEMP\\afsd_init.log\0",
    
    clientRegValues,
    clientWinNTRegValues,
    clientWin9XRegValues,

    // Start menu entries to remove
    "Client\0",

    // Config info to preserve
    AFSREG_CLT_SVC_KEY"\0",
    IDS_PRESERVE_CLIENT_CONFIG_INFO,

    ClientSpecificUninstall
};


/*
 * App info structure for the Light Client product
 */
struct APPINFO appLightClient = {
    "AFS Light",
    
    // No service info 
    0,
    0,
    0,
    0,

    AFSREG_CLT_SVC_NAME,

    // No service shutdown messages
    0,
    0,

    "HKEY_LOCAL_MACHINE\\SOFTWARE\\TransarcCorporation\\AFS Light Client",

    { AFSREG_CLT_SW_VERSION_KEY, AFSREG_CLT_SW_VERSION_DIR_VALUE },

    "\\Client",
    "\\Program",

    // No dirs to delete
    0,
    
    // Files to delete
    TARGETDIR"\\Common\\*.gid\0"
    TARGETDIR"\\Common\\*.fts\0",

    clientRegValues,
    clientWinNTRegValues,
    clientWin9XRegValues,

    // Start menu entries to remove
    "Light\0",

    // Config info to preserve
    AFSREG_CLT_SVC_KEY"\0",
    IDS_PRESERVE_LIGHT_CLIENT_CONFIG_INFO,

    UninstallCredsTool
};


/*
 * App info structure for the Control Center product
 */
struct APPINFO appControlCenter = {
    "AFS Control Center",
    
    // No service info
    0,
    0,
    0,
    0,

    // No network provider order
    0,

    // No service shutdown messages    
    0,
    0,

    "HKEY_LOCAL_MACHINE\\SOFTWARE\\TransarcCorporation\\AFS Control Center",
    
    { "HKEY_LOCAL_MACHINE\\SOFTWARE\\TransarcCorporation\\AFS Control Center\\CurrentVersion", "PathName" },

    "\\Control Center",
    "",

    // No dirs to delete
    0,
    
    // Files to delete
    TARGETDIR"\\Common\\*.gid\0"
    TARGETDIR"\\Common\\*.fts\0",
    
    0,  // No reg values
    0,  // No NT only reg values
    0,  // No 9x only reg values

    // Start menu entries to remove
    "Control Center\0",

    // Config info to preserve
    AFSREG_CLT_SVC_KEY"\0",
    IDS_PRESERVE_CC_CONFIG_INFO,

    0   // No uninstall function
};


/*
 * App info structure for the Sys Admin Doc files
 */
struct APPINFO appDocs = {
    "AFS Supplemental Documentation",

    // No service info
    0,
    0,
    0,
    0,

    // No network provider order
    0,

    // No service shutdown messages    
    0,
    0,

    "HKEY_LOCAL_MACHINE\\SOFTWARE\\TransarcCorporation\\AFS Supplemental Documentation",
    
    { "HKEY_LOCAL_MACHINE\\SOFTWARE\\TransarcCorporation\\AFS Supplemental Documentation\\CurrentVersion", "PathName" },

    "\\Documentation",
    "",

    // No dirs to delete
    0,
    
    // Files to delete
    TARGETDIR"\\Common\\*.gid\0"
    TARGETDIR"\\Common\\*.fts\0",

    0,  // No reg values
    0,  // No NT only reg values
    0,  // No 9x only reg values

    // Start menu entries to remove
    "Documentation\\AFS for Windows Backup Command Reference.lnk\0Documentation\\AFS Command Reference Manual.lnk\0Documentation\\AFS System Administrator's Guide.lnk\0",

    0,  // No config info to preserve

    0   // No uninstall function
};


// Shared and in-use files
struct FILEINFO {
    char *pszName;
    int nUsedBy;
};

#define SERVER  1
#define CLIENT  2
#define LCLIENT 4
#define CC      8
#define DOCS    16


struct FILEINFO fileInfo[] = {
    { TARGETDIR"\\Common\\afsbosadmin.dll",             SERVER | CC },
    { TARGETDIR"\\Common\\afscfgadmin.dll",             SERVER | CC },
    { TARGETDIR"\\Common\\afsclientadmin.dll",          SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afskasadmin.dll",             SERVER | CC },
    { TARGETDIR"\\Common\\afsptsadmin.dll",             SERVER | CC },
    { TARGETDIR"\\Common\\afsvosadmin.dll",             SERVER | CC },
    { TARGETDIR"\\Common\\afsadminutil.dll",            SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afsrpc.dll",                  SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afsauthent.dll",              SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afspthread.dll",              SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\TaAfsAppLib.dll",             SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afsprocmgmt.dll",             SERVER | CLIENT | LCLIENT },
    { TARGETDIR"\\Common\\afs_config.exe",              CLIENT | LCLIENT| CC },
    { TARGETDIR"\\Common\\afseventmsg_????.dll",        SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afslegal_????.dll",           SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afsserver_????.dll",          SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afssvrcfg_????.dll",          SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\TaAfsAccountManager_????.dll",SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\TaAfsAppLib_????.dll",        SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\TaAfsServerManager_????.dll", SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afscreds_????.dll",           SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afs_config_????.dll",         SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afs_cpa_????.dll",            SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afs_shl_ext_????.dll",        SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afs-nt.hlp",                  SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afs-nt.cnt",                  SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\taafssvrmgr.cnt",             SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\taafssvrmgr.hlp",             SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\taafsusrmgr.cnt",             SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\taafsusrmgr.hlp",             SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afs-cc.cnt",                  SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afs-cc.hlp",                  SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afs-light.cnt",               SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\afs-light.hlp",               SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\taafscfg.cnt",                SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Common\\taafscfg.hlp",                SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Client\\PROGRAM\\afs_shl_ext.dll",    CLIENT | LCLIENT },
    { TARGETDIR"\\Client\\PROGRAM\\libafsconf.dll",     CLIENT | LCLIENT },
    { TARGETDIR"\\Client\\PROGRAM\\afslogon.dll",       CLIENT },
    { TARGETDIR"\\Client\\PROGRAM\\afslog95.dll",       LCLIENT },
    { TARGETDIR"\\Control Center\\TaAfsAdmSvr.exe",     CC },
    { WINSYSDIR"\\afs_cpa.cpl",                         CLIENT | LCLIENT | CC },
    { WINSYSDIR"\\afsserver.cpl",                       SERVER },
    { TARGETDIR"\\Common\\afsdcell.ini",                CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Documentation\\Html\\banner.gif",     SERVER | CLIENT | LCLIENT | CC | DOCS },
    { TARGETDIR"\\Documentation\\Html\\books.gif",      SERVER | CLIENT | LCLIENT | CC | DOCS },
    { TARGETDIR"\\Documentation\\Html\\bot.gif",        SERVER | CLIENT | LCLIENT | CC | DOCS },
    { TARGETDIR"\\Documentation\\Html\\index.gif",      SERVER | CLIENT | LCLIENT | CC | DOCS },
    { TARGETDIR"\\Documentation\\Html\\index.htm",      SERVER | CLIENT | LCLIENT | CC | DOCS },
    { TARGETDIR"\\Documentation\\Html\\next.gif",       SERVER | CLIENT | LCLIENT | CC | DOCS },
    { TARGETDIR"\\Documentation\\Html\\prev.gif",       SERVER | CLIENT | LCLIENT | CC | DOCS },
    { TARGETDIR"\\Documentation\\Html\\toc.gif",        SERVER | CLIENT | LCLIENT | CC | DOCS },
    { TARGETDIR"\\Documentation\\Html\\top.gif",        SERVER | CLIENT | LCLIENT | CC | DOCS },
    { TARGETDIR"\\Documentation\\Html\\ReleaseNotes\\relnotes.htm",
                                                        SERVER | CLIENT | LCLIENT | CC },
    { TARGETDIR"\\Documentation\\Html\\InstallGd\\afsnt35i.htm",
                                                        SERVER | CLIENT | LCLIENT | CC },
    { 0,                                                0 }     // End of list
};


/*
 * VARIABLES _________________________________________________________________
 *
 */
HINSTANCE hinst;

static HWND hDlg;
static BOOL bPreserveConfigInfo;
static BOOL bSilentMode;
static char *pszInstallDir;


/*
 * FUNCTIONS _________________________________________________________________
 *
 */

static BOOL UpgradeClientIntParm(HKEY hKey, char *pszOldParm, char *pszNewParm)
{
    int nData;
    LONG result = ERROR_SUCCESS;

    nData = GetPrivateProfileInt("AFS Client", pszOldParm, -1, "afsd.ini");
    if (nData > -1)
        result = RegSetValueEx(hKey, pszNewParm, 0, REG_DWORD, (BYTE *)&nData, sizeof(nData));

    return (result == ERROR_SUCCESS);
}

static BOOL UpgradeClientStringParm(HKEY hKey, char *pszOldParm, char *pszNewParm)
{
    char szData[1024];
    LONG result = ERROR_SUCCESS;

    GetPrivateProfileString("AFS Client", pszOldParm, "", szData, sizeof(szData), "afsd.ini");
    if (szData[0])
        result = RegSetValueEx(hKey, pszNewParm, 0, REG_SZ, (PBYTE)szData, strlen(szData) + 1);

    return (result == ERROR_SUCCESS);
}

int SUCALLCONV Upgrade34ClientConfigInfo()
{
    HKEY hKey;
    LONG result;
    int nData;
    
    result = RegOpenKeyAlt(AFSREG_NULL_KEY, AFSREG_CLT_SVC_PARAM_KEY, KEY_WRITE, TRUE, &hKey, 0);
    if (result != ERROR_SUCCESS)
        return -1;

    UpgradeClientIntParm(hKey, "CacheSize", "CacheSize");
    UpgradeClientIntParm(hKey, "Stats", "Stats");
    UpgradeClientIntParm(hKey, "LogoffTokenTransfer", "LogoffTokenTransfer");
    UpgradeClientIntParm(hKey, "LogoffTokenTransferTimeout", "LogoffTokenTransferTimeout");
    UpgradeClientIntParm(hKey, "TrapOnPanic", "TrapOnPanic");
    UpgradeClientIntParm(hKey, "TraceBufferSize", "TraceBufferSize");
    UpgradeClientIntParm(hKey, "TraceOnShutdown", "TraceOnShutdown");
    UpgradeClientIntParm(hKey, "ReportSessionStartups", "ReportSessionStartups");
    
    UpgradeClientStringParm(hKey, "MountRoot", "MountRoot");
    UpgradeClientStringParm(hKey, "Cell", "Cell");

    /* BlockSize to ChunkSize requires convertion */
    nData = GetPrivateProfileInt("AFS Client", "BlockSize", -1, "afsd.ini");
    if (nData > -1) {
	DWORD chunkSize;
	for (chunkSize = 0; (1 << chunkSize) < nData; chunkSize++);
        (void) RegSetValueEx(hKey, "ChunkSize", 0, REG_DWORD, (BYTE *)&chunkSize, sizeof(chunkSize));
    }

    RegCloseKey(hKey);

    return 0;
}

int SUCALLCONV Eradicate34Client()
{
    if (Client34Eradicate(TRUE) != ERROR_SUCCESS)
        return -1;

    return 0;
}

// This function was written a long time ago by Mike Comer for use by the 
// original DFS Client for NT install program.
int SUCALLCONV CheckIfAdmin(void)
{
    HANDLE                  token = INVALID_HANDLE_VALUE;
    PVOID                   buffer = 0;
    DWORD                   bufLength;
    DWORD                   realBufLength;
    TOKEN_PRIMARY_GROUP     *pgroup;
    TOKEN_GROUPS            *groups;
    int                     result = -1;
    DWORD                   groupCount;
    LONG                    status;
    PSID                    AdministratorSID = NULL;
    SID_IDENTIFIER_AUTHORITY    authority = SECURITY_NT_AUTHORITY;

    // allocate the SID for the Administrators group
    if (!AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdministratorSID)) {
        status = GetLastError();
        goto getout;
    }

    // open the process token
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &token)) {
        status = GetLastError();
        token = INVALID_HANDLE_VALUE;
        goto getout;
    }

    // check primary group first
    buffer = GlobalAlloc(GMEM_FIXED, sizeof(TOKEN_PRIMARY_GROUP));
    if (!buffer) {
        goto getout;
    }

    bufLength = sizeof(TOKEN_PRIMARY_GROUP);
    while(1) {
        if (!GetTokenInformation(token, TokenPrimaryGroup, buffer, bufLength, &realBufLength)) {
            if (realBufLength > bufLength) {
                // not enough space
                GlobalFree(buffer);
                bufLength = realBufLength;
                buffer = GlobalAlloc(GMEM_FIXED, realBufLength);
                if (!buffer) {
                    goto getout;
                }
                continue;
            }

            goto getout;
        }
        break;
    }

    pgroup = (TOKEN_PRIMARY_GROUP *)buffer;
    if (EqualSid(pgroup->PrimaryGroup, AdministratorSID)) {
        result = 0;
    } else {
        // okay, try the secondary groups
        while(1) {
            if (!GetTokenInformation(token, TokenGroups, buffer, bufLength, &realBufLength)) {
                if (realBufLength > bufLength) {
                    // not enough space
                    GlobalFree(buffer);
                    bufLength = realBufLength;
                    buffer = GlobalAlloc(GMEM_FIXED, realBufLength);
                    if (!buffer) {
                        goto getout;
                    }
                    continue;
                }

                // a real error
                goto getout;
            }
            break;
        }

        // we have the list of groups here.  Process them:
        groups = (TOKEN_GROUPS *)buffer;
        for(groupCount = 0; groupCount < groups->GroupCount; groupCount++) {
            if (EqualSid(groups->Groups[groupCount].Sid, AdministratorSID)) {
                result = 0;
                break;
            }
        }
    }

getout:

    if (token != INVALID_HANDLE_VALUE) {
        CloseHandle(token);
    }

    if (buffer) {
        GlobalFree(buffer);
    }

    if (AdministratorSID) {
        FreeSid(AdministratorSID);
    }

    return result;
}

static void SetSharedFileRefCount(char *pszFile, int nRefCount)
{
    LONG result;
    HKEY hKey;
    
    result = RegOpenKeyAlt(AFSREG_NULL_KEY, MS_SHARED_FILES_KEY, KEY_WRITE, FALSE, &hKey, 0);
    if (result != ERROR_SUCCESS)
        return;
    
    if (nRefCount <= 0)
        RegDeleteValue(hKey, pszFile);
    else
        RegSetValueEx(hKey, pszFile, 0, REG_DWORD, (BYTE *)&nRefCount, sizeof(int));    
    
    RegCloseKey(hKey);
}

static char *GetTimeStamp()
{
    char szTime[64], szDate[64];
    static char szTimeDate[128];

    _strtime(szTime);
    _strdate(szDate);

    sprintf(szTimeDate, "[%s %s] ", szTime, szDate);
    
    return szTimeDate;
}

int SUCALLCONV WriteToInstallErrorLog(char *pszMsg)
{
    static BOOL bWritten = FALSE;
    FILE *fp;

    // On the first write, recreate the file    
    fp = fopen(INSTALL_ERROR_LOG_NAME, bWritten ? "a" : "w");
    if (!fp)
        return -1;
        
    fprintf(fp, "%s%s\r\n", GetTimeStamp(), pszMsg);
    
    fclose(fp);
    
    bWritten = TRUE;
    
    return 0;
}

static void WriteToUninstallErrorLog(char *pszMsg)
{
    static BOOL bWritten = FALSE;
    FILE *fp;

    // On the first write, recreate the file    
    fp = fopen(UNINSTALL_ERROR_LOG_NAME, bWritten ? "a" : "w");
    if (!fp)
        return;
        
    fprintf(fp, "%s%s\r\n", GetTimeStamp(), pszMsg);
    
    fclose(fp);
    
    bWritten = TRUE;
}

static char *LoadResString(UINT uID)
{
    static char str[256];
    GetString (str, uID);
    return str;
}

static void ShowError(UINT nResID, LONG nError)
{
    char szErr[256];
    char szPrompt[256];
    char szMsg[256];
    char szTitle[256];
    char *psz;

    psz = LoadResString(nResID);
    if (psz)
        strcpy(szErr, psz);
    else
        sprintf(szErr, "unknown error msg (Msg ID = %d)", nResID);
    
    psz = LoadResString(IDS_INSTALLATION_FAILURE);
    strcpy(szPrompt, psz ? psz : "An error has occurred:  %s (Last Error = %ld).");

    sprintf(szMsg, szPrompt, szErr, nError);

    psz = LoadResString(IDS_TITLE);
    strcpy(szTitle, psz ? psz : "AFS");

    if (bSilentMode)
        WriteToUninstallErrorLog(szMsg);
    else
        MessageBox(hDlg, szMsg, szTitle, MB_OK);
}

static int ShowMsg(UINT nResID, int nType)
{
    char szTitle[256];
    char *psz;

    psz = LoadResString(IDS_TITLE);
    strcpy(szTitle, psz ? psz : "AFS");

    return MessageBox(hDlg, LoadResString(nResID), szTitle, nType);
}

static char *GetAppInstallDir(struct APPINFO *pApp, BOOL bRemembered)
{
    HKEY hKey;
    LONG nResult;
    DWORD dwType;
    static char szInstallDir[256];
    DWORD dwSize;
    char *pszKey;
    char *pszValue;

    pszKey = bRemembered ? UNINSTALL_TEMP_INFO_KEY : pApp->regInstallDir.pszKey;
    pszValue = bRemembered ? INSTALL_DIR_VALUE_NAME : pApp->regInstallDir.pszValue;

    dwSize = sizeof(szInstallDir);

    nResult = RegOpenKeyAlt(AFSREG_NULL_KEY, pszKey, KEY_READ, FALSE, &hKey, 0);
    if (nResult == ERROR_SUCCESS) {
        nResult = RegQueryValueEx(hKey, pszValue, 0, &dwType, (PBYTE)szInstallDir, &dwSize);
        RegCloseKey(hKey);
    }

    if (nResult != ERROR_SUCCESS) {
        ShowError(IDS_CANT_DETERMINE_APP_PATH, nResult);
        return 0;
    }

    FilepathNormalizeEx(szInstallDir, FPN_BACK_SLASHES);

    return szInstallDir;
}

static BOOL DoesRegKeyExist(char *pszKey)
{
    HKEY hKey;
    LONG nResult;

    nResult = RegOpenKeyAlt(AFSREG_NULL_KEY, pszKey, KEY_READ, FALSE, &hKey, 0);
    if (nResult == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return TRUE;
    }

    return FALSE;
}

static BOOL IsAppInstalled(struct APPINFO *pApp)
{
    return DoesRegKeyExist(pApp->pszAppKey);
}

static void BuildShortPath(char *pszShortPath, UINT nShortPathLen, char *pszInstallDir, char *pszPath)
{
    strncpy(pszShortPath, pszInstallDir, nShortPathLen);
    strncat(pszShortPath, pszPath, nShortPathLen);
    
    GetShortPathName(pszShortPath, pszShortPath, nShortPathLen);
}

static BOOL IsWin95()
{
    OSVERSIONINFO versionInformation;

    versionInformation.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    
    GetVersionEx(&versionInformation);
    
    if ((versionInformation.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) &&
        (versionInformation.dwMinorVersion == 0))
        return TRUE;
        
    return FALSE;
}

int IsWin98()
{
    OSVERSIONINFO versionInformation;

    versionInformation.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    
    GetVersionEx(&versionInformation);
    
    if ((versionInformation.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) &&
        (versionInformation.dwMinorVersion == 10))
        return 0;
        
    return -1;
}

static BOOL IsServiceInstalled(char *pszServiceKey)
{
    HKEY hKey;

    if (RegOpenKeyAlt(0, pszServiceKey, KEY_READ, FALSE, &hKey, 0) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return TRUE;
    }

    return FALSE;
}

// If this fails in anyway we just return.  No error is displayed.
static void MakeSureServiceDoesNotExist(char *pszName)
{
    SC_HANDLE hServer = 0, hSCM = 0;
    SERVICE_STATUS status;

    hSCM = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
    if (hSCM) {
        hServer = OpenService(hSCM, pszName, SERVICE_ALL_ACCESS | DELETE);
        if (hServer) {
            if (QueryServiceStatus(hServer, &status)) {
                if (status.dwCurrentState != SERVICE_STOPPED) {
                    if (!ControlService(hServer, SERVICE_CONTROL_STOP, &status)) {
                        CloseServiceHandle(hServer);
                        CloseServiceHandle(hSCM);
                        return;
                    }
                }
            }
            
            // Try to delete even if status query fails
            DeleteService(hServer);
        }
    }

    if (hServer)
        CloseServiceHandle(hServer);
    if (hSCM)
        CloseServiceHandle(hSCM);
}

static int InstallService(char *pszName, char *pszDependOn, char *pszDisplayName, char *pszServicePath, BOOL bInteractive)
{
    SC_HANDLE hServer = 0, hSCM;
    BOOL bRestoreOldConfig = FALSE;

    if (!AddToProviderOrder(AFSREG_CLT_SVC_NAME)) {
        ShowError(ERROR_FILE_NOT_FOUND, GetLastError());
		return -1;
    }
    hSCM = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) {
        ShowError(IDS_SCM_OPEN_FAILED, GetLastError());
        return -1;
    }

/*  This code is not used, but it could be handy in the future so I am keeping it here.

    // If the service exists, then we (most probably) are in the middle of an upgrade or reinstall.
    bRestoreOldConfig = IsServiceInstalled(pszName);

    if (bRestoreOldConfig) {
        hServer = OpenService(hSCM, pszName, SERVICE_ALL_ACCESS);
        if (!hServer || !ChangeServiceConfig(hServer, SERVICE_NO_CHANGE, SERVICE_AUTO_START, SERVICE_NO_CHANGE, 0, 0, 0, 0, 0, 0, 0)) {
            ShowError(IDS_RESTORE_OF_PREVIOUS_CONFIG_FAILED, GetLastError());
            bRestoreOldConfig = FALSE;
            // Fall through to service creation below
        }
    } 
*/
    
    if (!bRestoreOldConfig) {
        DWORD dwServiceType;

        // If the service already exists, the create call will fail.  This can
        // happen if uninstall failed (which is not infrequent).  Making sure the
        // service does not exist makes it easier for a user to install over top of
        // a previously failed uninstall.
        MakeSureServiceDoesNotExist(pszName);

        dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        if (bInteractive)
            dwServiceType |= SERVICE_INTERACTIVE_PROCESS;
    
        hServer = CreateService(hSCM, pszName, pszDisplayName,
            SERVICE_ALL_ACCESS, dwServiceType, SERVICE_AUTO_START, 
            SERVICE_ERROR_NORMAL, pszServicePath, 0, 0, "RPCSS\0Netbios\0\0", 0, 0);
    
        if (!hServer)
            ShowError(IDS_SERVICE_CREATE_FAILED, GetLastError());
    }

    if (hServer)
        CloseServiceHandle(hServer);

    CloseServiceHandle(hSCM);

    return 0;
}

int SUCALLCONV InstallServerService(char *pszServicePath)
{
    return InstallService(appServer.pszSvcName, 0, appServer.pszSvcDisplayName, pszServicePath, TRUE);
}

int SUCALLCONV InstallClientService(char *pszServicePath)
{
    return InstallService(appClient.pszSvcName, appClient.pszSvcDependOn, appClient.pszSvcDisplayName, pszServicePath, FALSE);
}

static int UninstallService(struct APPINFO *pAppInfo)
{
    SC_HANDLE hServer, hSCM;
    SERVICE_STATUS status;
    BOOL bOk;
    BOOL bServer = FALSE;
    BOOL bShowingProgressDlg = FALSE;

    if (!RemoveFromProviderOrder(AFSREG_CLT_SVC_NAME)) {
        ShowError(ERROR_FILE_NOT_FOUND, GetLastError());
		return -1;
    }
    hSCM = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) {
        ShowError(IDS_SCM_OPEN_FAILED, GetLastError());
        return -1;
    }
    
    hServer = OpenService(hSCM, pAppInfo->pszSvcName, SERVICE_ALL_ACCESS | DELETE);
    if (!hServer) {
        ShowError(IDS_SERVICE_OPEN_FAILED, GetLastError());
        CloseServiceHandle(hSCM);
        return -1;
    }

    if (!QueryServiceStatus(hServer, &status)) {
        ShowError(IDS_SERVICE_QUERY_FAILED, GetLastError());
        CloseServiceHandle(hServer);
        CloseServiceHandle(hSCM);
        return -1;
    }

    if (status.dwCurrentState != SERVICE_STOPPED) {
        if (pAppInfo->nServiceShutdownMsgID) {
            if (!bSilentMode && (ShowMsg(pAppInfo->nServiceShutdownMsgID, MB_YESNO | MB_ICONQUESTION) == IDNO)) {
                CloseServiceHandle(hServer);
                CloseServiceHandle(hSCM);
                return 1;
            }
        }

        if (!bSilentMode)
            bShowingProgressDlg = ShowProgressDialog(LoadResString(pAppInfo->nServiceShutdownProgressMsgID));

        if (!ControlService(hServer, SERVICE_CONTROL_STOP, &status)) {
            if (bShowingProgressDlg)
                HideProgressDialog();
            ShowError(IDS_SERVICE_STOP_FAILED, GetLastError());
            CloseServiceHandle(hServer);
            CloseServiceHandle(hSCM);
            return -1;
        }
    }

    // Wait for the service to stop
    while (status.dwCurrentState != SERVICE_STOPPED) {
        // I stopped waiting on dwWaitHint because it seemed the wait hint was too long.
        // The service would be stopped but we'd still be asleep for a long time yet.
        Sleep(5000);    //status.dwWaitHint);

        if (!QueryServiceStatus(hServer, &status)) {
            if (bShowingProgressDlg)
                HideProgressDialog();
            ShowError(IDS_SERVICE_QUERY_FAILED, GetLastError());
            CloseServiceHandle(hServer);
            CloseServiceHandle(hSCM);
            return -1;
        }
    }

    // The service has been stopped
    if (bShowingProgressDlg)
        HideProgressDialog();

    // This code to disable the service may be of use some day so I am keeping it here.
    // bOk = ChangeServiceConfig(hServer, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE, 0, 0, 0, 0, 0, 0, 0); 

    bOk = DeleteService(hServer);

    if (!bOk)
        ShowError(IDS_SERVICE_DELETE_FAILED, GetLastError());

    CloseServiceHandle(hServer);
    CloseServiceHandle(hSCM);

    return (bOk ? 0 : -1);
}

int SUCALLCONV AddToNetworkProviderOrder(char *pszWhatToAdd)
{
    return AddToProviderOrder(pszWhatToAdd) ? 0 : -1;
}

static int RemoveFromNetworkProviderOrder(char *pszWhatToDel)
{
    return RemoveFromProviderOrder(pszWhatToDel) ? 0 : -1;
}

int SUCALLCONV AddToPath(char *pszPath)
{
    return AddToSystemPath(pszPath) ? 0 : -1;
}

static int RemoveFromPath(char *pszPath)
{
    return RemoveFromSystemPath(pszPath) ? 0 : -1;
}

static void RemoveFiles(char *pszFileSpec)
{
    struct _finddata_t fileinfo;
    long hSearch;
    char szDel[MAX_PATH];
    char szDir[MAX_PATH];
    char *p;

    strcpy(szDir, pszFileSpec);
    p = strrchr(szDir, '\\');
    if (p)
        *p = 0;
    
    hSearch = _findfirst(pszFileSpec, &fileinfo);
    if (hSearch == -1)
        return;

    while (1) {
        if ((strcmp(fileinfo.name, ".") != 0) && (strcmp(fileinfo.name, "..") != 0)) {
            sprintf(szDel, "%s\\%s", szDir, fileinfo.name);
            DeleteFile(szDel);
        }

        if (_findnext(hSearch, &fileinfo) == -1)
            break;
    }

    _findclose(hSearch);
}

static void RemoveDir(char *pszDir)
{
    char szFileSpec[MAX_PATH];

    sprintf(szFileSpec, "%s\\*.*", pszDir);

    RemoveFiles(szFileSpec);
    RemoveDirectory(pszDir);
}

static void RemoveRegValues(struct REGVALUE *pRegValues)
{
    struct REGVALUE *pCurValue;
    HKEY hKey;
    LONG nResult;

    if (!pRegValues)
        return;

    for (pCurValue = pRegValues; pCurValue->pszKey; pCurValue++) {
        nResult = RegOpenKeyAlt(AFSREG_NULL_KEY, pCurValue->pszKey, KEY_ALL_ACCESS, FALSE, &hKey, 0);

        if (nResult == ERROR_SUCCESS) {
            nResult = RegDeleteValue(hKey, pCurValue->pszValue);
            RegCloseKey(hKey);
        }

        if (nResult != ERROR_SUCCESS)
            ShowError(IDS_REG_DELETE_VALUE_ERROR, nResult);
    }
}

static BOOL UninstallCredsTool()
{
    int nResult = WinExec("afscreds /uninstall", SW_HIDE);

    if (nResult <= 31) {
        if (nResult != ERROR_FILE_NOT_FOUND)
            ShowError(IDS_CANT_UNINSTALL_AFSCREDS, nResult);
    }

    // Always return true.  We don't want the uninstall to completely fail just
    // because the creds tool didn't uninstall.
    return TRUE;
}


static char *GetTempDir()
{
    DWORD result;
    static char szTempDir[MAX_PATH];

    result = GetTempPath(sizeof(szTempDir) - 1, szTempDir);
    if (result == 0)
        return "\\";
        
    return szTempDir;
}

static char *GetRootInstallDir()
{
    char *psz;
    static char szRootInstallDir[MAX_PATH] = "";

    if (szRootInstallDir[0] == 0) {
        strcpy(szRootInstallDir, pszInstallDir);
    
        // Strip off the app specific part of the install dir
        psz = strrchr(szRootInstallDir, '\\');
        if (psz)
            *psz = 0;
    }

    return szRootInstallDir;
}

static BOOL ClientSpecificUninstall()
{
    int nChoice;

    // This function needs to do two things.  First it needs to see if the server is
    // installed, and if it is, ask the user if they really want to uninstall the
    // client given that the server needs the client.  Second, if we are uninstalling
    // the client, we need to uninstall the creds tool.

    if (!bSilentMode) {
        if (IsAppInstalled(&appServer)) {
            nChoice = ShowMsg(IDS_CLIENT_NEEDED_BY_SERVER, MB_ICONQUESTION | MB_YESNO);
            if (nChoice == IDNO)
                return FALSE;       // Cancel the uninstall
        }
    }
    
    UninstallCredsTool();

    return TRUE;
}

static struct APPINFO *GetApp()
{
#ifdef SERVER_UNINST
    return &appServer;
#elif CLIENT_UNINST
    return &appClient;
#elif CC_UNINST
    return &appControlCenter;
#elif LIGHT_CLIENT_UNINST
    return &appLightClient;
#elif DOCS_UNINST
    return &appDocs;
#else
    return 0;
#endif;
}

static void RememberInstallDir(char *pszInstallDir)
{
    HKEY hKey;

    // We remember the install dir so that when the UninstUninitialize function is called
    // by the InstallShield uninstaller, we can find out where we were installed to.  We
    // have to do this because by the time that function is called, the registry values
    // created at install time are already gone.  We need to be able to find out where we
    // were installed so we can clean up anything IS couldn't uninstall.  If this fails in 
    // any way then we don't care.  The only consequence is that some junk might be left on
    // the users' system after an uninstall.
    
    LONG result = RegOpenKeyAlt(AFSREG_NULL_KEY, UNINSTALL_TEMP_INFO_KEY, KEY_WRITE, TRUE, &hKey, 0);
    if (result != ERROR_SUCCESS)
        return;

    RegSetValueEx(hKey, INSTALL_DIR_VALUE_NAME, 0, REG_SZ, (PBYTE)pszInstallDir, strlen(pszInstallDir) + 1);    

    RegCloseKey(hKey);
}

int SUCALLCONV SetSilentMode()
{
    bSilentMode = TRUE;

    return 0;
}

static char *GetWinDir()
{
    static char szWinDir[MAX_PATH] = "";

    if (!szWinDir[0]) 
        GetWindowsDirectory(szWinDir, sizeof(szWinDir));
    
    return szWinDir;
}

static char *GetWinSysDir()
{
    static char szWinSysDir[MAX_PATH] = "";

    if (!szWinSysDir[0])
        GetSystemDirectory(szWinSysDir, sizeof(szWinSysDir));
    
    return szWinSysDir;
} 

static char *GetLocaleID()
{
    static char szID[25] = "";

    if (szID[0] == 0) {
        LCID dwID = GetSystemDefaultLCID();
        
         // Nuke the high word.  It contains a sort ID.
        dwID &= 0x0000FFFF;
        
        // Convert locale ID to a string
        itoa(dwID, szID, 10);

        // This thing should never be more than LOCALE_ID_LEN characters long.
        szID[LOCALE_ID_LEN] = 0;
    }

    return szID;
}

static char *ExpandPath(char *pszFile)
{
    static char szPath[MAX_PATH];
    char *psz;

    szPath[0] = 0;

    // Convert a path containing TARGETDIR, WINDIR, or WINSYSDIR to a 
    // real path in the file system.  One of these MUST be the start of
    // the file path passed in.  Also convert the string ???? to an
    // actual locale number.
    if (strncmp(pszFile, TARGETDIR, strlen(TARGETDIR)) == 0)
        strcpy(szPath, GetRootInstallDir());
    else if (strncmp(pszFile, WINDIR, strlen(WINDIR)) == 0)
        strcpy(szPath, GetWinDir());
    else if (strncmp(pszFile, WINSYSDIR, strlen(WINSYSDIR)) == 0)
        strcpy(szPath, GetWinSysDir());
    
    if (szPath[0]) {    
        psz = strchr(pszFile, '\\');
        if (psz)
            strcat(szPath, psz);
    } else
        strcpy(szPath, pszFile);

    // Is this a language dll?
    psz = strstr(szPath, "????.");
    
    // If it is, replace ???? with the locale number
    if (psz)
        strncpy(psz, GetLocaleID(), LOCALE_ID_LEN);

    return szPath;
}

static BOOL FileNeededByOtherApp(struct APPINFO *pApp, struct FILEINFO *pFileInfo)
{
    // If the file is used by the server, the app being uninstalled is not the server, and
    // the server is installed, then this file is used by another app.
    if (!IsWinNT()) {
        if ((pFileInfo->nUsedBy & LCLIENT) && (pApp != &appLightClient) && IsAppInstalled(&appLightClient))
            return TRUE;
        return FALSE;
    }

    if ((pFileInfo->nUsedBy & SERVER) && (pApp != &appServer) && IsAppInstalled(&appServer))
        return TRUE;

    if ((pFileInfo->nUsedBy & CLIENT) && (pApp != &appClient) && IsAppInstalled(&appClient))
        return TRUE;

    if ((pFileInfo->nUsedBy & CC) && (pApp != &appControlCenter) && IsAppInstalled(&appControlCenter))
        return TRUE;
    
    return FALSE;
}

static void DeleteInUseFiles(struct APPINFO *pAppInfo, struct FILEINFO *pFileInfo)
{
    char szSrcPath[MAX_PATH];
    char szDestPath[MAX_PATH];
    char szTempDir[MAX_PATH];
    int ii;

    // If some app's file has been loaded before the app is uninstalled, then
    // when an uninstall is attempted, the application and all of the dlls that
    // its uses will be in use and IS will not be able to delete them.  Normally this
    // is not a problem because IS will tell the user to reboot to finish the uninstall.
    // However, we must support the ability to perform a silent uninstall followed
    // immediatly by an install of the same product to the same directories.  If we let
    // IS handle the uninstall of these files, this is not possible.  The reason is that
    // when IS fails to remove these in use files, it marks them for deletion after the
    // next reboot, which is fine.  Unfortunately, it leaves them in the dirs they were
    // installed to.  So if we don't immediately reboot and perform an install to the
    // same dirs, once a reboot is performed, those files get deleted and we have a 
    // broken installation.

    // What we will do to fix all of this, is when the client is uninstalled, but
    // before IS does anything, we will move the in use files and associated dlls
    // into the temp dir and mark them for delete after a reboot.  Then an install
    // that follows will succeed.

    // Delete the files that may be in use.  If they are we actually move
    // them to the temp dir and mark them for deletion after the next reboot.
    for (ii = 0; pFileInfo[ii].pszName != 0; ii++) {
        // Get the source path
        strcpy(szSrcPath, ExpandPath(pFileInfo[ii].pszName));

        // Only delete the file if it is not used by some other app
        if (FileNeededByOtherApp(pAppInfo, &pFileInfo[ii]))
            continue;

        // If the file doesn't exist then go on to the next file.
        if (_access(szSrcPath, 0) != 0)
            continue;
            
        // See if we can do a regular delete of the file
        if (DeleteFile(szSrcPath)) {
            SetSharedFileRefCount(szSrcPath, 0);
            continue;
        }

        // Get a temp dir that is on the same drive as the src path.
        // We can't move an in use file to a different drive.
        strcpy(szTempDir, GetTempDir());
        if (szTempDir[0] != szSrcPath[0]) {
            // Get the drive, colon, and slash of the src path
            strncpy(szTempDir, szSrcPath, 3);
            szTempDir[3] = 0;
        }
        
        // Get the dest path - we will rename the file during the move
        GetTempFileName(szTempDir, "AFS", 0, szDestPath);

        // Move from source to dest, marking the file for deletion after a reboot
        if (IsWin95()) {
            if (MoveFile(szSrcPath, szDestPath)) {            
                WritePrivateProfileString("rename", szSrcPath, szDestPath, "wininit.ini");
                SetSharedFileRefCount(szSrcPath, 0);
            }
        } else {    // WinNT or Win98
            if (MoveFileEx(szSrcPath, szDestPath, MOVEFILE_REPLACE_EXISTING)) {
                SetFileAttributes(szDestPath, FILE_ATTRIBUTE_NORMAL);
                MoveFileEx(szDestPath, 0, MOVEFILE_DELAY_UNTIL_REBOOT);
                SetSharedFileRefCount(szSrcPath, 0);
            }
        }
    }
}

// Delete a directory and all its files and subdirectories - Yee haaa!
static void RemoveDirectoryTree(char *pszDir)
{
    HANDLE hFind;
    WIN32_FIND_DATA findFileData;
    char szSpec[MAX_PATH];
    char szSubFileOrDir[MAX_PATH];
    BOOL bContinue;

    sprintf(szSpec, "%s\\*.*", pszDir);
    
    // First delete the contents of the dir
    hFind = FindFirstFile(szSpec, &findFileData);
    bContinue = (hFind != INVALID_HANDLE_VALUE);
    
    while (bContinue) {
        if ((strcmp(findFileData.cFileName, ".") != 0) && (strcmp(findFileData.cFileName, "..") != 0)) {
            sprintf(szSubFileOrDir, "%s\\%s", pszDir, findFileData.cFileName);
            
            if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                RemoveDirectoryTree(szSubFileOrDir);
            else
                DeleteFile(szSubFileOrDir);
        }

        bContinue = FindNextFile(hFind, &findFileData);
    }

    FindClose(hFind);
        
    // Now remove the dir
    RemoveDirectory(pszDir);
} 

static char *GetStartMenuRoot()
{
    HKEY hKey;
    LONG nResult;
    DWORD dwType;
    DWORD dwSize;
    char *pszKey;
    char *pszValue;

    static char szStartMenuRoot[MAX_PATH] = "";

    if (szStartMenuRoot[0] == 0) {
        dwSize = sizeof(szStartMenuRoot);
    
        if (IsWinNT()) {
            pszKey = WINNT_START_MENU_REG_KEY;
            pszValue = WINNT_START_MENU_REG_VALUE;
        } else {
            pszKey = WIN9X_START_MENU_REG_KEY;
            pszValue = WIN9X_START_MENU_REG_VALUE;
        }
        
        nResult = RegOpenKeyAlt(AFSREG_NULL_KEY, pszKey, KEY_READ, FALSE, &hKey, 0);
        if (nResult == ERROR_SUCCESS) {
            nResult = RegQueryValueEx(hKey, pszValue, 0, &dwType, (PBYTE)szStartMenuRoot, &dwSize);
            RegCloseKey(hKey);
        }

        if (nResult != ERROR_SUCCESS)
            return 0;
    }

    FilepathNormalizeEx(szStartMenuRoot, FPN_BACK_SLASHES);

    return szStartMenuRoot;
}

static char *GetAfsStartMenuRoot()
{
    static char szAfsStartMenuRoot[MAX_PATH] = "";
    char *pszStartMenuRoot;
    
    if (szAfsStartMenuRoot[0] == 0) {    
        pszStartMenuRoot = GetStartMenuRoot();
        if (!pszStartMenuRoot)
            return 0;

        if (bSilentMode)
            sprintf(szAfsStartMenuRoot, "%s\\IBM WebSphere\\Performance Pack\\AFS", pszStartMenuRoot );
        else
            sprintf(szAfsStartMenuRoot, "%s\\IBM AFS", pszStartMenuRoot );
    }

    return szAfsStartMenuRoot;
}

static BOOL IsADir(char *pszName)
{
    struct _stat statbuf;

    if (_stat(pszName, &statbuf) < 0)
        return FALSE;

    return statbuf.st_mode & _S_IFDIR;
}

static void DeleteStartMenuEntries(char *pszEntries)
{
    char szStartMenuPath[MAX_PATH];
    char *pszAfsStartMenuRoot;
    char *pszCurEntry;

    pszAfsStartMenuRoot = GetAfsStartMenuRoot();

    if (!pszAfsStartMenuRoot)
        return;
        
    for (pszCurEntry = pszEntries; *pszCurEntry; pszCurEntry += strlen(pszCurEntry) + 1) {
        sprintf(szStartMenuPath, "%s\\%s", pszAfsStartMenuRoot, pszCurEntry);
        if (IsADir(szStartMenuPath))
            RemoveDirectoryTree(szStartMenuPath);
        else
            DeleteFile(szStartMenuPath);
    }
}

static void RefreshStartMenu()
{
    char *pszAfsStartMenuRoot;
    char szTemp[MAX_PATH];
    
    pszAfsStartMenuRoot = GetAfsStartMenuRoot();
    if (!pszAfsStartMenuRoot)
        return;

    sprintf(szTemp, "%s - Refresh Attempt", pszAfsStartMenuRoot);
        
    // Deleting items from below the root level of the start menu does not 
    // cause it to refresh.  In order that users can see changes without
    // rebooting we will temporarily rename our root most entry, which 
    // does cause a refresh of the start menu.
    MoveFileEx(pszAfsStartMenuRoot, szTemp, MOVEFILE_REPLACE_EXISTING);
    MoveFileEx(szTemp, pszAfsStartMenuRoot, MOVEFILE_REPLACE_EXISTING);
}

static BOOL PreserveConfigInfo(struct APPINFO *pApp)
{
    char *pszRegKey;
    char szDestKey[256];
    LONG result;

    bPreserveConfigInfo = TRUE;

    // If not in silent mode, ask user if they want to preserve the cfg info
    if (!bSilentMode) {
        int nChoice = ShowMsg(pApp->nPreserveConfigInfoMsgID, MB_ICONQUESTION | MB_YESNOCANCEL);
        if (nChoice == IDCANCEL)
            return FALSE;                   // Cancel the uninstall
        else if (nChoice == IDNO) {     
            bPreserveConfigInfo = FALSE;    // User doesn't want to preserve the config info
            return TRUE;
        }
    }

    // Copy each reg key (and all of its subkeys and values) to another place in the registry.
    for (pszRegKey = pApp->pszRegKeysToPreserve; *pszRegKey; pszRegKey += strlen(pszRegKey) + 1) {
        if (!DoesRegKeyExist(pszRegKey))
            continue;

        // Create the destination path for the copy
        sprintf(szDestKey, "%s\\%s\\%s", AFS_PRESERVED_CFG_INFO_KEY, pApp->pszAppName, pszRegKey);

        // Try to copy it
        result = RegDupKeyAlt(pszRegKey, szDestKey);

        if ((result != ERROR_SUCCESS) && (result != ERROR_FILE_NOT_FOUND)) {
            // If the copy failed, then delete any copies that succeeded
            sprintf(szDestKey, "%s\\%s", AFS_PRESERVED_CFG_INFO_KEY, pApp->pszAppName);
            RegDeleteEntryAlt(szDestKey, REGENTRY_KEY);
     		goto done;
        }
    }

	// Remember the integrated login setting if this app supports that and it was turned on
	if (pApp->pszNetworkProviderOrder) {
		// Was integerated login turned on?
		BOOL bOn, bOk;
		bOk = InNetworkProviderOrder(pApp->pszNetworkProviderOrder, &bOn);
		if (bOk && bOn) {
			HKEY hKey;
			sprintf(szDestKey, "%s\\%s\\IntegratedLogin", AFS_PRESERVED_CFG_INFO_KEY, pApp->pszAppName);
			result = RegOpenKeyAlt(AFSREG_NULL_KEY, szDestKey, KEY_WRITE, TRUE, &hKey, 0);
			// The existance of the key is a flag indicating that integrated login was turned on
			RegCloseKey(hKey);
		}
	}
	
done:
	if ((result == ERROR_SUCCESS) || bSilentMode)
	    return TRUE;    // Continue with uninstall

    // Report the error and ask the user if they want to continue the uninstall
    return (ShowMsg(IDS_SAVE_OF_CONFIG_INFO_FAILED, MB_ICONEXCLAMATION | MB_YESNO) == IDYES);			
}

int SUCALLCONV RestoreConfigInfo(int nApp)
{
    char *pszRegKey;
    char szSrcKey[256];
    struct APPINFO *pApp = 0;
    BOOL bError = FALSE;
    LONG result;

    switch (nApp) {
        case SERVER:    pApp = &appServer;          break;
        case CLIENT:    pApp = &appClient;          break;
        case LCLIENT:   pApp = &appLightClient;     break;
        case CC:        pApp = &appControlCenter;   break;
    }
    
    if (!pApp)
        return -1;
        
    // Copy each reg key (and all of its subkeys and values) back to its original place in the registry.
    for (pszRegKey = pApp->pszRegKeysToPreserve; *pszRegKey; pszRegKey += strlen(pszRegKey) + 1) {
        // Create the source path for the copy
        sprintf(szSrcKey, "%s\\%s\\%s", AFS_PRESERVED_CFG_INFO_KEY, pApp->pszAppName, pszRegKey);

        if (!DoesRegKeyExist(szSrcKey))
            continue;

        // Try to restore as many of the keys as possible.  Report any errors at the end.

        // Try to copy it
        result = RegDupKeyAlt(szSrcKey, pszRegKey);
        if ((result != ERROR_SUCCESS) && (result != ERROR_FILE_NOT_FOUND))
            bError = TRUE;
    }

	// Restore integrated login if this app was using it
	if (pApp->pszNetworkProviderOrder) {
		// Check if integrated login was turned on.  The IntegratedLogin key is a flag
		// telling us that it was on.  If the key does not exist, integrated login was
		// not on.
		sprintf(szSrcKey, "%s\\%s\\IntegratedLogin", AFS_PRESERVED_CFG_INFO_KEY, pApp->pszAppName);
		if (DoesRegKeyExist(szSrcKey)) {
			if (!AddToProviderOrder(pApp->pszNetworkProviderOrder))
				bError = TRUE;
		}
	}

    // Remove our saved copies of the config info
    sprintf(szSrcKey, "%s\\%s", AFS_PRESERVED_CFG_INFO_KEY, pApp->pszAppName);
    RegDeleteEntryAlt(szSrcKey, REGENTRY_KEY);
            
    if (bError)
        ShowError(IDS_RESTORE_OF_PREVIOUS_CONFIG_FAILED, 0);

    return TRUE;
}

static BOOL DoSubKeysExist(char *pszKey)
{
    LONG result;
    HKEY hKey;
    char *pszSubKeys = 0;
    BOOL bExist;
    
    result = RegOpenKeyAlt(AFSREG_NULL_KEY, pszKey, KEY_READ, FALSE, &hKey, 0);
    if (result != ERROR_SUCCESS)
        return FALSE;
        
    result = RegEnumKeyAlt(hKey,  &pszSubKeys);
    RegCloseKey(hKey);
    
    if (result != ERROR_SUCCESS)
        return FALSE;
   
    if (pszSubKeys) {
        bExist = TRUE;
        free(pszSubKeys);
    } else
        bExist = FALSE;    

    return bExist;
}

/*
 * The following definitions are taken from richedit.h:
 *
 */

#define EM_SETBKGNDCOLOR		(WM_USER + 67) // from Richedit.h
#define EM_STREAMIN				(WM_USER + 73) // from Richedit.h
#define SF_RTF			        0x0002

typedef DWORD (CALLBACK *EDITSTREAMCALLBACK)(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb);

typedef struct _editstream {
	DWORD dwCookie;		/* user value passed to callback as first parameter */
	DWORD dwError;		/* last error */
	EDITSTREAMCALLBACK pfnCallback;
} EDITSTREAM;

/*
 *
 */

DWORD CALLBACK License_StreamText (DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
{
   LPTSTR psz = (LPTSTR)dwCookie;
   LONG cchAvail = lstrlen(psz);
   if ((*pcb = min(cchAvail, cb)) != 0) {
      memcpy (pbBuff, psz, *pcb);
      memmove (psz, &psz[*pcb], cchAvail - *pcb + 1);
   }
   return 0;
}


void License_OnInitDialog (HWND hDlg, LPTSTR pszFile)
{
    // Open the license file and shove its text in our RichEdit control
    //
    HANDLE hFile;
    if ((hFile = CreateFile (pszFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE) {

        size_t cbText;
        if ((cbText = GetFileSize (hFile, NULL)) != 0) {

            LPTSTR abText = (LPTSTR)GlobalAlloc (GMEM_FIXED, cbText + 3);

            DWORD cbRead;
            if (ReadFile (hFile, abText, cbText, &cbRead, NULL)) {
                abText[ cbRead ] = 0;

                EDITSTREAM Stream;
                memset (&Stream, 0x00, sizeof(Stream));
                Stream.dwCookie = (DWORD)abText;
                Stream.pfnCallback = License_StreamText;

                SendDlgItemMessage (hDlg, IDC_TEXT, EM_STREAMIN, SF_RTF, (LPARAM)&Stream);
            }

            GlobalFree (abText);
        }

        CloseHandle (hFile);
    }

    // Make the control's background be gray
    //
    SendDlgItemMessage (hDlg, IDC_TEXT, EM_SETBKGNDCOLOR, FALSE, (LPARAM)GetSysColor(COLOR_BTNFACE));
}

BOOL CALLBACK License_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_INITDIALOG:
            SetWindowLong (hDlg, DWL_USER, lp);
            License_OnInitDialog (hDlg, (LPTSTR)lp);
            break;

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDCANCEL:
                case IDOK:
                    EndDialog (hDlg, LOWORD(wp));
                    break;

                case IDC_PRINT:
                    TCHAR szDir[ MAX_PATH ];
                    GetCurrentDirectory (MAX_PATH, szDir);
                    ShellExecute (hDlg, TEXT("print"), (LPTSTR)GetWindowLong (hDlg, DWL_USER), NULL, szDir, SW_HIDE);
                    break;
            }
            break;
    }
    return FALSE;
}

BOOL FindAfsInstallationPathByComponent (LPTSTR pszInstallationPath, LPTSTR pszComponent)
{
    *pszInstallationPath = 0;

    TCHAR szRegPath[ MAX_PATH ];
    wsprintf (szRegPath, TEXT("Software\\TransarcCorporation\\%s\\CurrentVersion"), pszComponent);

    HKEY hk;
    if (RegOpenKey (HKEY_LOCAL_MACHINE, szRegPath, &hk) == 0) {
        DWORD dwType = REG_SZ;
        DWORD dwSize = MAX_PATH;

        if (RegQueryValueEx (hk, TEXT("PathName"), NULL, &dwType, (PBYTE)pszInstallationPath, &dwSize) == 0) {
            *(LPTSTR)FindBaseFileName (pszInstallationPath) = TEXT('\0');

            if (pszInstallationPath[0] && (pszInstallationPath[ lstrlen(pszInstallationPath)-1 ] == TEXT('\\')))
            pszInstallationPath[ lstrlen(pszInstallationPath)-1 ] = TEXT('\0');
        }

        RegCloseKey (hk);
    }

    return !!*pszInstallationPath;
}

BOOL FindAfsInstallationPath (LPTSTR pszInstallationPath)
{
   if (FindAfsInstallationPathByComponent (pszInstallationPath, TEXT("AFS Client")))
      return TRUE;
   if (FindAfsInstallationPathByComponent (pszInstallationPath, TEXT("AFS Control Center")))
      return TRUE;
   if (FindAfsInstallationPathByComponent (pszInstallationPath, TEXT("AFS Server")))
      return TRUE;
   if (FindAfsInstallationPathByComponent (pszInstallationPath, TEXT("AFS Supplemental Documentation")))
      return TRUE;
   return FALSE;
}

HINSTANCE LoadRichTextControl (void)
{
    HINSTANCE hInst;
    if ((hInst = LoadLibrary ("riched20.dll")) != NULL)
        return hInst;
    if ((hInst = LoadLibrary ("riched32.dll")) != NULL)
        return hInst;
    if ((hInst = LoadLibrary ("riched.dll")) != NULL)
        return hInst;
    if ((hInst = LoadLibrary ("richedit.dll")) != NULL)
        return hInst;
    return NULL;
}

int SUCALLCONV ShowLicense (char *pszTarget, char *pszSource)
{
    // If the license already lives on this user's machine, don't show
    // it again. This only has to be done if the user has never
    // accepted the license agreement before (it's part of the setup
    // program, so it gets installed if they've accepted it).
    //
    // We were handed a relative path of the form:
    //    Documentation/html/license.rtf
    //
    // We'll need to find the AFS installation directory, in order to
    // find that Documentation subtree.
    //
    BOOL fShowLicense = TRUE;

    TCHAR szInstallationPath[ MAX_PATH ];
    if (FindAfsInstallationPath (szInstallationPath)) {
        TCHAR szLicensePath[ MAX_PATH ];
        wsprintf (szLicensePath, TEXT("%s\\%s"), szInstallationPath, pszTarget);

        if (GetFileAttributes (szLicensePath) != (DWORD)-1) {
            fShowLicense = FALSE;
        }
    }

    // Before we can show the license file, we have to prepare the RichEdit
    // control. That means loading the appropriate library and calling its
    // initialization functions.
    //
    HINSTANCE hRichEdit;
    if ((hRichEdit = LoadRichTextControl()) != NULL) {

        // If we must show the license, do so now. This is a modal dialog,
        // so we'll know whether or not the user accepts the license.
        //
        if (ModalDialogParam (IDD_LICENSE, GetActiveWindow(), License_DlgProc, (LPARAM)pszSource) == IDCANCEL) {
            // The user rejected the license; fail setup
            return FALSE;
        }

    	FreeLibrary (hRichEdit);
    }

    // The user accepted the license, so we can continue with Setup.
    // The license file is installed as part of Setup.
    return TRUE;
}

int SUCALLCONV UninstInitialize(HWND hIS, HINSTANCE hIS5, long Reserved)
{
    char szPath[MAX_PATH];
    struct APPINFO *pAppInfo;
    char *pszFile = 0;
    char *pszSubDir = 0;

    hDlg = hIS;

    bSilentMode = !IsWindowVisible(hIS);

    // Which app are we uninstalling?
    pAppInfo = GetApp();
    if (!pAppInfo) {
        ShowError(IDS_CANT_DETERMINE_PRODUCT, 0);
        return -1;
    }

    // Get the app's install dir
    pszInstallDir = GetAppInstallDir(pAppInfo, FALSE);
    if (!pszInstallDir)
        return -1;

    // If this app has a custom uninstall func, call it here
    if (pAppInfo->pUninstallFunc)
        if (!pAppInfo->pUninstallFunc())
            return -1;

    if (pAppInfo->pszRegKeysToPreserve)
        if (!PreserveConfigInfo(pAppInfo))
            return -1;

    // Unconfigure the service, if there is one for this app
    if (pAppInfo->pszSvcKey) {
        if (IsServiceInstalled(pAppInfo->pszSvcKey))
            if (UninstallService(pAppInfo) == 1)
                return -1;
    }

    RememberInstallDir(pszInstallDir);

    DeleteInUseFiles(pAppInfo, fileInfo);

    // Remove the app's bin path from the system path
    if (pAppInfo->pszBinPath) {
        BuildShortPath(szPath, sizeof(szPath), pszInstallDir, pAppInfo->pszBinPath);
        RemoveFromPath(szPath);
    }

    // Remove entry from NetworkProvider\Order key in registry
    if (pAppInfo->pszNetworkProviderOrder)
        RemoveFromNetworkProviderOrder(pAppInfo->pszNetworkProviderOrder);

    // Remove any generated subdirectories
    if (!bPreserveConfigInfo && pAppInfo->pszDirsToDel) {
        for (pszSubDir = pAppInfo->pszDirsToDel; *pszSubDir; pszSubDir += strlen(pszSubDir) + 1)
            RemoveDir(ExpandPath(pszSubDir));
    }

    // Remove any generated files
    if (!bPreserveConfigInfo && pAppInfo->pszFilesToDel) {
        for (pszFile = pAppInfo->pszFilesToDel; *pszFile; pszFile += strlen(pszFile) + 1)
            RemoveFiles(ExpandPath(pszFile));
    }

    // Remove any registry values that IS can't handle
    RemoveRegValues(pAppInfo->pRegValues);
    if (IsWinNT())
        RemoveRegValues(pAppInfo->pWinNTRegValues);
    else    
        RemoveRegValues(pAppInfo->pWin9XRegValues);

    // Remove the start menu entries for this app
    if (pAppInfo->pszStartMenuEntries) {
        DeleteStartMenuEntries(pAppInfo->pszStartMenuEntries);
        RefreshStartMenu();
    }

    // Remove the install dir
    RemoveDirectory(pszInstallDir);

    return 0;
}

void SUCALLCONV UninstUnInitialize(HWND hIS, HINSTANCE hIS5, long Reserved)
{
    char *pszInstallDir;
    char szDirPath[MAX_PATH];
    char *psz;
    struct APPINFO *pAppInfo;

    // If we just uninstalled the last AFS app, then do some cleanup.
    if (IsAppInstalled(&appServer) || IsAppInstalled(&appClient) ||
        IsAppInstalled(&appControlCenter) || IsAppInstalled(&appLightClient) ||
        IsAppInstalled(&appDocs))
    {
        return;
    }

    bSilentMode = !IsWindowVisible(hIS);
    
    // Which app did we just uninstall?
    pAppInfo = GetApp();
    if (!pAppInfo) {
        ShowError(IDS_CANT_DETERMINE_PRODUCT, 0);
        return;
    }

    // Get the app's install dir
    pszInstallDir = GetAppInstallDir(pAppInfo, TRUE);
    if (!pszInstallDir)
        return;

    // Remove the reg key we used to remember the app install dir
    RegDeleteEntryAlt(UNINSTALL_TEMP_INFO_KEY, REGENTRY_KEY);

    // Try to remove the reg key used to store config info, but only
    // if there are no app config info sub keys present.
    if (!DoSubKeysExist(AFS_PRESERVED_CFG_INFO_KEY))
        RegDeleteEntryAlt(AFS_PRESERVED_CFG_INFO_KEY, REGENTRY_KEY);

    // Remove the install dir
    RemoveDirectory(pszInstallDir);

    // Attempt to remove the install root and common directories.  The are 
    // shared and so no single app knows to delete them.

    // Strip off the app specific part of the install dir
    psz = strrchr(pszInstallDir, '\\');
    if (psz)
        *psz = 0;

    sprintf(szDirPath, "%s\\%s", pszInstallDir, "Common");
    RemoveDirectory(szDirPath);

    // Remove the Common directory from the system path
    RemoveFromPath(szDirPath);

    // Remove all of the documentation dirs
    sprintf(szDirPath, "%s\\%s", pszInstallDir, "Documentation");
    RemoveDirectoryTree(szDirPath);

    // Ok, up to this point we have been removing files we know we
    // created.  However, after this point we are into the path
    // that the user chose for our install root.  The default for
    // this is IBM/Afs, but they could have chosen anything,
    // including a dir or dirs that have other products in them.
    // We will check to see if it is IBM\AFS and if it is then we 
    // will attempt to remove them.
    
    // Back up a level and look for AFS
    psz = strrchr(pszInstallDir, '\\');
    if (psz) {
        if (stricmp(psz + 1, "AFS") == 0) {
            RemoveDirectory(pszInstallDir);
            *psz = 0;
        }
    }

    // Back up a level and look for IBM
    psz = strrchr(pszInstallDir, '\\');
    if (psz) {
        if (stricmp(psz + 1, "IBM") == 0) {
            RemoveDirectory(pszInstallDir);
            *psz = 0;
        }
    }

    // Remove the root afs start menu entry
    psz = GetStartMenuRoot();
    if (psz) {
        if (bSilentMode) {
            // Remove everything under our branch
            sprintf(szDirPath, "%s\\IBM WebSphere\\Performance Pack\\AFS", psz);
            RemoveDirectoryTree(szDirPath);
            
            // Remove the IBM stuff only if the dirs are empty
            sprintf(szDirPath, "%s\\IBM WebSphere\\Performance Pack", psz);
            if (RemoveDirectory(szDirPath)) {
                sprintf(szDirPath, "%s\\IBM WebSphere", psz);
                RemoveDirectory(szDirPath);
            }
        } else {
            sprintf(szDirPath, "%s\\IBM AFS", psz);
            RemoveDirectoryTree(szDirPath);
        }
    }
}

BOOLEAN _stdcall DllEntryPoint(HANDLE dll, DWORD reason, PVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        hinst = (HINSTANCE)dll;
        TaLocale_LoadCorrespondingModuleByName (hinst, "afs_setup_utils.dll");
    }

    return TRUE;
}

extern "C" int WINAPI Test (HINSTANCE hInst, HINSTANCE hPrev, LPSTR psz, int nCmdShow)
{
   ShowLicense ("TEST", "\\\\fury\\afssetup\\license\\ja_JP.rtf");
   return 0;
}


