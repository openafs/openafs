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

#include <windows.h>
#include <winreg.h>
#include <nb30.h>
#include <tchar.h>
#include <shellapi.h>
#include <objbase.h>
#include <shlobj.h>
#include <wtypes.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <lanahelper.h>
#include <WINNT\afsreg.h>

#define NOLOGGING
#ifndef NOLOGGING
extern "C" {
#include <stdarg.h>

    void afsi_log(TCHAR *p, ...) {
        va_list marker;
        TCHAR buffer[200];

        va_start(marker,p);
        _vstprintf(buffer,p,marker);
        va_end(marker);
        _tcscat(buffer,_T("\n"));

        OutputDebugString(buffer);
    }
}
#endif

static const char *szNetbiosNameValue = "NetbiosName";
static const char *szIsGatewayValue = "IsGateway";
static const char *szLanAdapterValue = "LanAdapter";
#ifdef USE_FINDLANABYNAME
static const char *szNoFindLanaByName = "NoFindLanaByName";
#endif
static const char *szForceLanaLoopback = "ForceLanaLoopback";

// Use the IShellFolder API to get the connection name for the given Guid.
static HRESULT lana_ShellGetNameFromGuidW(WCHAR *wGuid, WCHAR *wName, int NameSize)
{
    // This is the GUID for the network connections folder. It is constant.
    // {7007ACC7-3202-11D1-AAD2-00805FC1270E}
    const GUID CLSID_NetworkConnections = {
        0x7007ACC7, 0x3202, 0x11D1, {
	    0xAA, 0xD2, 0x00, 0x80, 0x5F, 0xC1, 0x27, 0x0E
	}
    };
    LPITEMIDLIST pidl;
    IShellFolder *pShellFolder;
    IMalloc *pShellMalloc;

    // Build the display name in the form "::{GUID}".
    if (wcslen(wGuid) >= MAX_PATH)
        return E_INVALIDARG;
    WCHAR szAdapterGuid[MAX_PATH + 2];
    swprintf(szAdapterGuid, L"::%ls", wGuid);

    // Initialize COM.
    CoInitialize(NULL);

    // Get the shell allocator.
    HRESULT hr = SHGetMalloc(&pShellMalloc);
    if (SUCCEEDED(hr))
    {
        // Create an instance of the network connections folder.
        hr = CoCreateInstance(CLSID_NetworkConnections, NULL,
			      CLSCTX_INPROC_SERVER, IID_IShellFolder,
			      reinterpret_cast<LPVOID *>(&pShellFolder));
    }
    if (SUCCEEDED(hr)) 
    {
        hr = pShellFolder->ParseDisplayName(NULL, NULL, szAdapterGuid, NULL,
					    &pidl, NULL);
    }
    if (SUCCEEDED(hr)) {
        // Get the display name; this returns the friendly name.
        STRRET sName;
	hr = pShellFolder->GetDisplayNameOf(pidl, SHGDN_NORMAL, &sName);
	if (SUCCEEDED(hr))
            wcsncpy(wName, sName.pOleStr, NameSize);
	pShellMalloc->Free(pidl);
    }

    CoUninitialize();
    return hr;
}

// Get the Connection Name for the given GUID.
extern "C" int lana_GetNameFromGuid(char *Guid, char **Name)
{
    typedef HRESULT (WINAPI *HrLanProcAddr)(GUID *, PCWSTR, PWSTR, LPDWORD);
    HrLanProcAddr HrLanProc = NULL;
    HMODULE hNetMan;
    int size;
    WCHAR *wGuid = NULL;
    WCHAR wName[MAX_PATH];
    DWORD NameSize = MAX_PATH;
    char *name = NULL;
    HRESULT status;
        
    // Convert the Guid string to Unicode.  First we ask only for the size
    // of the converted string.  Then we allocate a buffer of sufficient
    // size to hold the result of the conversion.
    size = MultiByteToWideChar(CP_ACP, 0, Guid, -1, NULL, 0);
    wGuid = (WCHAR *) malloc(size * sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, Guid, -1, wGuid, size);

    // First try the IShellFolder interface, which was unimplemented
    // for the network connections folder before XP.

    /* XXX pbh 9/11/03 - revert to using the undocumented APIs on XP while
     *   waiting to hear back from PSS about the slow reboot issue.
     *   This is an ugly, misleading hack, but is minimally invasive
     *   and will be easy to rollback.
     */

    //status = getname_shellfolder(wGuid, wName, NameSize);
    status = E_NOTIMPL;

    /* XXX end of pbh 9/11/03 temporary hack*/	

    if (status == E_NOTIMPL) {
        // The IShellFolder interface is not implemented on this platform.
        // Try the (undocumented) HrLanConnectionNameFromGuidOrPath API
        // from the netman DLL.
#ifndef NOLOGGING
        afsi_log("IShellFolder API not implemented, trying HrLanConnectionNameFromGuidOrPath");
#endif
        hNetMan = LoadLibrary("netman.dll");
        if (hNetMan == NULL) {
            free(wGuid);
            return -1;
        }
        /* Super Secret Microsoft Call */
        HrLanProc = (HrLanProcAddr) GetProcAddress(hNetMan,
                                                   "HrLanConnectionNameFromGuidOrPath");
        if (HrLanProc == NULL) {
            FreeLibrary(hNetMan);
            free(wGuid);
            return -1;
        }
        status = HrLanProc(NULL, wGuid, wName, &NameSize);
        FreeLibrary(hNetMan);
    }
    free(wGuid);
    if (FAILED(status)) {
#ifndef NOLOGGING
        afsi_log("lana_GetNameFromGuid: failed to get connection name (status %ld)",
		 status);
#endif
        return -1;
    }

    // Get the required buffer size, and then convert the string.
    size = WideCharToMultiByte(CP_ACP, 0, wName, -1, NULL, 0, NULL, NULL);
    name = (char *) malloc(size);
    if (name == NULL)
        return -1;
    WideCharToMultiByte(CP_ACP, 0, wName, -1, name, size, NULL, NULL);
#ifndef NOLOGGING
    afsi_log("Connection name for %s is '%s'", Guid, name);
#endif
    if (*Name)
        *Name = name;
    else
        free(name);
    return 0;
}

// Return an array of LANAINFOs corresponding to a connection named LanaName
// (NULL LanaName matches all connections), and has an IPv4 binding. Returns
// NULL if something goes wrong.
// NOTE: caller must free the returned block if non NULL.
extern "C" LANAINFO * lana_FindLanaByName(const char *LanaName)
{
    const char RegNetBiosLinkageKeyName[] =
        "System\\CurrentControlSet\\Services\\NetBios\\Linkage";
    HKEY hkey;
    LONG status;
    struct {
        BYTE flags;
        BYTE number;
    } lanamap[MAX_LANA+1];
    DWORD lanamapsize = sizeof(lanamap);
    DWORD type;
    char *bindpaths = NULL;
    DWORD bindpathsize;
    int nlana;
    int i;
    char *guid;
    char *name;
    char *pBind;
    char *p;

    LANAINFO * lanainfo;

    // Open the NetBios Linkage key.
    status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegNetBiosLinkageKeyName, 0, 
                          KEY_QUERY_VALUE, &hkey);
        
    if (status != ERROR_SUCCESS) {
#ifndef NOLOGGING
        afsi_log("Failed to open NetBios Linkage key (status %ld)", status);
#endif
        return NULL;
    }

    // Read the lana map.
    status = RegQueryValueEx(hkey, "LanaMap", 0, &type,
                         (BYTE *) &lanamap, &lanamapsize);
    if (status != ERROR_SUCCESS) {
#ifndef NOLOGGING
        afsi_log("Failed to read LanaMap (status %ld)", status);
#endif
        RegCloseKey(hkey);
        return NULL;
    }
    if (lanamapsize == 0) {
#ifndef NOLOGGING
        afsi_log("No data in LanaMap");
#endif
        return NULL;
    }
    nlana = lanamapsize / sizeof(lanamap[0]);

    // Get the bind paths for NetBios so we can match them up
    // with the lana map.  First we query for the size, so we
    // can allocate an appropriate buffer.
    status = RegQueryValueEx(hkey, "Bind", 0, &type, NULL, &bindpathsize);
    if (status == ERROR_SUCCESS && bindpathsize != 0) {
        bindpaths = (char *) malloc(bindpathsize * sizeof(char));
        if (bindpaths == NULL) {
#ifndef NOLOGGING
            afsi_log("Cannot allocate %ld bytes for bindpaths", bindpathsize);
#endif
            RegCloseKey(hkey);
            return NULL;
        }
        status = RegQueryValueEx(hkey, "Bind", 0, &type, 
                                 (BYTE *) bindpaths, &bindpathsize);
    }
    RegCloseKey(hkey);
    if (status != ERROR_SUCCESS) {
#ifndef NOLOGGING
        afsi_log("Failed to read bind paths (status %ld)", status);
#endif
        if (bindpaths != NULL)
            free(bindpaths);
        return NULL;
      }
    if (bindpathsize == 0) {
#ifndef NOLOGGING
        afsi_log("No bindpath data");
#endif
        if (bindpaths != NULL)
            free(bindpaths);
        return NULL;
    }

    if (LanaName)
    {
        lanainfo = (LANAINFO *) malloc(sizeof(LANAINFO)*2);
        if(lanainfo == NULL) {
            free(bindpaths);
            return NULL;
        }
        memset(lanainfo, 0, sizeof(LANAINFO) * 2);
        lanainfo[0].lana_number = LANA_INVALID;
    }
    else
    {
        lanainfo = (LANAINFO *) malloc(sizeof(LANAINFO)*(nlana+1));
        if(lanainfo == NULL) {
            free(bindpaths);
            return NULL;
        }
        memset(lanainfo, 0, sizeof(LANAINFO) * (nlana+1));
    }
    
    int index = 0;

    // Iterate over the lana map entries and bind paths.
    for (i = 0, pBind = bindpaths; i < nlana;
         i++, pBind += strlen(pBind) + 1) {
	// Ignore an invalid map entry.
        if ((lanamap[i].flags & 1) == 0)
            continue;

        // check for an IPv4 binding
        if(!strstr(pBind,"_Tcpip_"))
            continue;

        // Find the beginning of the GUID.
        guid = strchr(pBind, '{');
        if (guid == NULL)
	    continue;                   // Malformed path entry?
        guid = strdup(guid);
        if (guid == NULL)
            continue;
	// Find the end of the GUID.
        p = strchr(guid, '}');
        if (p == NULL) {
	    free(guid);                 // Malformed GUID?
            continue;
        }
        *++p = '\0';                    // Ignore anything after the GUID.
        status = lana_GetNameFromGuid(guid, &name);
        free(guid);

        if (status == 0 && name != 0)
        {
            if (LanaName)
            {
                if (strcmp(name, LanaName) ==0)
                {
                    lanainfo[index].lana_number = lanamap[i].number;
                    _tcscpy(lanainfo[index].lana_name, name);
                    free(name);
                    index++;
                    break;
                }
            }
            else
            {
                lanainfo[index].lana_number = lanamap[i].number;
                _tcscpy(lanainfo[index].lana_name, name);
                free(name);
                index++;
            }
        }
    }

    lanainfo[index].lana_number = LANA_INVALID;

    free(bindpaths);
    return lanainfo;
}

extern "C" lana_number_t lana_FindLoopback(void)
{
    NCB ncb;
    LANA_ENUM lana_list;
    int status;
    int i;

    memset(&ncb, 0, sizeof(ncb));
    ncb.ncb_command = NCBENUM;
    ncb.ncb_buffer = (UCHAR *) &lana_list;
    ncb.ncb_length = sizeof(lana_list);
    status = Netbios(&ncb);
    if (status != 0) {
#ifndef NOLOGGING
        afsi_log("Netbios NCBENUM failed: status %ld", status);
#endif
        return LANA_INVALID;
    }
    for (i = 0; i < lana_list.length; i++) {
	if (lana_IsLoopback(lana_list.lana[i],TRUE)) {
	    // Found one, return it.
#ifndef NOLOGGING
	    afsi_log("lana_FindLoopback: Found LAN adapter %d",
		     lana_list.lana[i]);
#endif
	    return lana_list.lana[i];
	}
    }
    // Could not find a loopback adapter.
    return LANA_INVALID;
}

/* Returns TRUE if all adapters are loopback adapters */
extern "C" BOOL lana_OnlyLoopback(void)
{
    NCB ncb;
    LANA_ENUM lana_list;
    int status;
    int i;

    memset(&ncb, 0, sizeof(ncb));
    ncb.ncb_command = NCBENUM;
    ncb.ncb_buffer = (UCHAR *) &lana_list;
    ncb.ncb_length = sizeof(lana_list);
    status = Netbios(&ncb);
    if (status != 0) {
#ifndef NOLOGGING
        afsi_log("Netbios NCBENUM failed: status %ld", status);
#endif
        return FALSE;
    }
    for (i = 0; i < lana_list.length; i++) {
	if (!lana_IsLoopback(lana_list.lana[i],FALSE)) {
	    // Found one non-Loopback adapter
	    return FALSE;
	}
    }
    // All adapters are loopback
    return TRUE;
}

// Is the given lana a Windows Loopback Adapter?
// TODO: implement a better check for loopback
// TODO: also check for proper bindings (IPv4)
// For VMWare we only check the first five octets since the last one may vary
extern "C" BOOL lana_IsLoopback(lana_number_t lana, BOOL reset)
{
    NCB ncb;
    struct {
        ADAPTER_STATUS status;
        NAME_BUFFER names[MAX_LANA+1];
    } astat;
    unsigned char kWLA_MAC[6] = { 0x02, 0x00, 0x4c, 0x4f, 0x4f, 0x50 };
    unsigned char kVista_WLA_MAC[6] = { 0x7F, 0x00, 0x00, 0x01, 0x4f, 0x50 };
    unsigned char kVMWare_MAC[5] = { 0x00, 0x50, 0x56, 0xC0, 0x00 };
    int status;
    HKEY hkConfig;
    LONG rv;
    int regLana = -1;
    DWORD dummyLen;

    rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE,AFSREG_CLT_SVC_PARAM_SUBKEY,0,KEY_READ,&hkConfig);
    if (rv == ERROR_SUCCESS) {
        rv = RegQueryValueEx(hkConfig, szForceLanaLoopback, NULL, NULL, (LPBYTE) &regLana, &dummyLen);
        RegCloseKey(hkConfig);

        if (regLana == lana)
            return TRUE;
    }

    if (reset) {
	// Reset the adapter: in Win32, this is required for every process, and
	// acts as an init call, not as a real hardware reset.
	memset(&ncb, 0, sizeof(ncb));
	ncb.ncb_command = NCBRESET;
	ncb.ncb_callname[0] = 100;
	ncb.ncb_callname[2] = 100;
	ncb.ncb_lana_num = lana;
	status = Netbios(&ncb);
	if (status == 0)
	    status = ncb.ncb_retcode;
	if (status != 0) {
#ifndef NOLOGGING
	    afsi_log("NCBRESET failed: lana %u, status %ld", lana, status);
#endif
	    return FALSE;
	}
    }

    // Use the NCBASTAT command to get the adapter address.
    memset(&ncb, 0, sizeof(ncb));
    ncb.ncb_command = NCBASTAT;
    ncb.ncb_lana_num = lana;
    strcpy((char *) ncb.ncb_callname, "*               ");
    ncb.ncb_buffer = (UCHAR *) &astat;
    ncb.ncb_length = sizeof(astat);
    status = Netbios(&ncb);
    if (status == 0)
        status = ncb.ncb_retcode;
    if (ncb.ncb_retcode != 0) {
#ifndef NOLOGGING   
        afsi_log("NCBASTAT failed: lana %u, status %ld", lana, status);
#endif
        return FALSE;
    }
    return (memcmp(astat.status.adapter_address, kWLA_MAC, 6) == 0 ||
	    memcmp(astat.status.adapter_address, kVista_WLA_MAC, 6) == 0 ||
	    memcmp(astat.status.adapter_address, kVMWare_MAC, 5) == 0);
}

// Get the netbios named used/to-be-used by the AFS SMB server.
// IF <lana specified> THEN
//     Use specified lana
// ELSE
//	   Look for an adapter named "AFS", failing which,
//     look for a loopback adapter.
// ENDIF
// IF lana is for a loopback && !IsGateway THEN
//    IF netbios name is specified THEN
//       use specified netbios name
//    ELSE
//       use "AFS"
//    ENDIF
// ELSE
//    use netbios name "<hostname>-AFS"
// ENDIF
// Return ERROR_SUCCESS if netbios name was successfully generated.
// Returns the lana number to use in *pLana (if pLana is non-NULL) and also
//         the IsGateway setting in *pIsGateway (if pIsGateway is non-NULL).
//         the type of name returned.
//
// buffer is assumed to hold at least MAX_NB_NAME_LENGTH bytes.
//
// flags :
//      LANA_NETBIOS_NAME_IN : Use the values of *pLana and *pIsGateway as [in] parameters.
//      LANA_NETBIOS_NAME_SUFFIX : Only return the suffix of netbios name
//      LANA_NETBIOS_NAME_FULL : Return full netbios name
extern "C" long lana_GetUncServerNameEx(char *buffer, lana_number_t * pLana, int * pIsGateway, int flags) {
    HKEY hkConfig;
    DWORD dummyLen;
    LONG rv;
    int regLana;
    int regGateway;
#ifdef USE_FINDLANABYNAME
    int regNoFindLanaByName;
#endif
    TCHAR regNbName[MAX_NB_NAME_LENGTH] = "AFS";
    TCHAR nbName[MAX_NB_NAME_LENGTH];
    TCHAR hostname[MAX_COMPUTERNAME_LENGTH+1];

    rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE,AFSREG_CLT_SVC_PARAM_SUBKEY,0,KEY_READ,&hkConfig);
    if(rv == ERROR_SUCCESS) {
	if(!(flags & LANA_NETBIOS_NAME_IN) || !pLana) {
	    dummyLen = sizeof(regLana);
	    rv = RegQueryValueEx(hkConfig, szLanAdapterValue, NULL, NULL, (LPBYTE) &regLana, &dummyLen);
	    if(rv != ERROR_SUCCESS) 
		regLana = -1;
	} else
	    regLana = *pLana;

	if(!(flags & LANA_NETBIOS_NAME_IN) || !pIsGateway) {
	    dummyLen = sizeof(regGateway);
	    rv = RegQueryValueEx(hkConfig, szIsGatewayValue, NULL, NULL, (LPBYTE) &regGateway, &dummyLen);
	    if(rv != ERROR_SUCCESS) 
		regGateway = 0;
	} else
	    regGateway = *pIsGateway;

#ifdef USE_FINDLANABYNAME
	dummyLen = sizeof(regNoFindLanaByName);
	rv = RegQueryValueEx(hkConfig, szNoFindLanaByName, NULL, NULL, (LPBYTE) &regNoFindLanaByName, &dummyLen);
	if(rv != ERROR_SUCCESS) 
	    regNoFindLanaByName = 0;
#endif

	// Do not care if the call fails for insufficient buffer size.  We are not interested
	// in netbios names over 15 chars.
	dummyLen = sizeof(regNbName);
	rv = RegQueryValueEx(hkConfig, szNetbiosNameValue, NULL, NULL, (LPBYTE) &regNbName, &dummyLen);
	if(rv != ERROR_SUCCESS) 
	    strcpy(regNbName, "AFS");
	else 
	    regNbName[15] = 0;

	RegCloseKey(hkConfig);
    } else {
	if(flags & LANA_NETBIOS_NAME_IN) {
	    regLana = (pLana)? *pLana: -1;
	    regGateway = (pIsGateway)? *pIsGateway: 0;
	} else {
	    regLana = -1;
	    regGateway = 0;
	}
#ifdef USE_FINDLANABYNAME 
        regNoFindLanaByName = 0;
#endif
	strcpy(regNbName, "AFS");
    }

    if(regLana < 0 || regLana > MAX_LANA) 
        regLana = -1;

    if(regLana == -1) {
	LANAINFO *lanaInfo = NULL;
        int nLana = LANA_INVALID;

#ifdef USE_FINDLANABYNAME
        if (!regNoFindLanaByName)
            lanaInfo = lana_FindLanaByName("AFS");
#endif
	if(lanaInfo != NULL) {
            nLana = lanaInfo[0].lana_number;
	    free(lanaInfo);
	} else
	    nLana = LANA_INVALID;

	if(nLana == LANA_INVALID && !regGateway) {
	    nLana = lana_FindLoopback();
	}
	if(nLana != LANA_INVALID) 
            regLana = nLana;
	}	

    if(regNbName[0] &&
	(regLana >=0 && lana_IsLoopback((lana_number_t) regLana,FALSE))) 	
    {
        strncpy(nbName,regNbName,15);
        nbName[16] = 0;
        strupr(nbName);
    } else {
	char * dot;

	if(flags & LANA_NETBIOS_NAME_SUFFIX) {
	    strcpy(nbName,"-AFS");
	} else {
	    dummyLen = sizeof(hostname);
	    // assume we are not a cluster.
	    rv = GetComputerName(hostname, &dummyLen);
	    if(!SUCCEEDED(rv)) { // should not happen, but...
		return rv;
	    }
	    strncpy(nbName, hostname, 11);
	    nbName[11] = 0;
	    if(dot = strchr(nbName,'.'))
		*dot = 0;
	    strcat(nbName,"-AFS");
	}
    }

    if(pLana) 
	*pLana = regLana;
    if(pIsGateway) 
	*pIsGateway = regGateway;

    strcpy(buffer, nbName);

    return ERROR_SUCCESS;
}

extern "C" void lana_GetUncServerNameDynamic(int lanaNumber, BOOL isGateway, TCHAR *name, int type) {
    char szName[MAX_NB_NAME_LENGTH];
    lana_number_t lana = (lana_number_t) lanaNumber;
    int gateway = (int) isGateway;

    if(SUCCEEDED(lana_GetUncServerNameEx(szName, &lana, &gateway, LANA_NETBIOS_NAME_IN | type))) {
#ifdef _UNICODE
	mbswcs(name,szName,MAX_NB_NAME_LENGTH);
#else
	strncpy(name,szName,MAX_NB_NAME_LENGTH);
#endif
    } else
	*name = _T('\0');
}

extern "C" void lana_GetUncServerName(TCHAR *name, int type) {
    char szName[MAX_NB_NAME_LENGTH];

    if(SUCCEEDED(lana_GetUncServerNameEx(szName,NULL,NULL,type))) {
#ifdef _UNICODE
	mbswcs(name,szName,MAX_NB_NAME_LENGTH);
#else	
	strncpy(name,szName,MAX_NB_NAME_LENGTH);
#endif
    } else {
	*name = _T('\0');
    }
}

extern "C" void lana_GetAfsNameString(int lanaNumber, BOOL isGateway, TCHAR* name)
{
    TCHAR netbiosName[32];
    lana_GetUncServerNameDynamic(lanaNumber, isGateway, netbiosName, LANA_NETBIOS_NAME_FULL);
    _stprintf(name, _T("Your UNC name to reach the root of AFS is \\\\%s\\all"), netbiosName);
}

extern "C" void lana_GetNetbiosName(LPTSTR pszName, int type)
{
    HKEY hkCfg;
    TCHAR name[MAX_NB_NAME_LENGTH];
    DWORD dummyLen;

    memset(name, 0, sizeof(name));
    if (GetVersion() >= 0x80000000) // not WindowsNT
    {
        if (type == LANA_NETBIOS_NAME_SUFFIX)
        {
            _tcscpy(pszName, TEXT("-afs"));
            return;
        }

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,AFSREG_CLT_SVC_PARAM_SUBKEY,0,KEY_READ,&hkCfg) == ERROR_SUCCESS) {
	    dummyLen = sizeof(name);
	    if(RegQueryValueEx(hkCfg,TEXT("Gateway"),NULL,NULL,(LPBYTE) name,&dummyLen) == ERROR_SUCCESS)
		name[0] = _T('\0');
	    RegCloseKey(hkCfg);
	}

        if (_tcslen(name) == 0)
        {
            _tcscpy(pszName, TEXT("unknown"));
            return;
        }

	_tcscpy(pszName, name);
        _tcscat(pszName, TEXT("-afs"));
        return;
    }

    lana_GetUncServerName(name,type);
    _tcslwr(name);
    _tcscpy(pszName, name);
    return;
}

