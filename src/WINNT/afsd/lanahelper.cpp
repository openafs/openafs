#include <afx.h>
#include <windows.h>
#include <winreg.h>
#include <nb30.h>
#include <tchar.h>
#include <shellapi.h>
#include <objbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wtypes.h>
#include <string.h>
#include <malloc.h>
#include "lanahelper.h"


extern "C" void afsi_log(...);

static HRESULT getname_shellfolder(WCHAR *wGuid, WCHAR *wName, int NameSize)
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
        // Create an instance of the network connections folder.
        hr = CoCreateInstance(CLSID_NetworkConnections, NULL,
			      CLSCTX_INPROC_SERVER, IID_IShellFolder,
			      reinterpret_cast<LPVOID *>(&pShellFolder));
    if (SUCCEEDED(hr))
        hr = pShellFolder->ParseDisplayName(NULL, NULL, szAdapterGuid, NULL,
					    &pidl, NULL);
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
        afsi_log("IShellFolder API not implemented, trying HrLanConnectionNameFromGuidOrPath");
        hNetMan = LoadLibrary("netman.dll");
        if (hNetMan == NULL) {
            free(wGuid);
            return -1;
        }
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
        afsi_log("lana_GetNameFromGuid: failed to get connection name (status %ld)",
		 status);
        return -1;
    }

    // Get the required buffer size, and then convert the string.
    size = WideCharToMultiByte(CP_ACP, 0, wName, -1, NULL, 0, NULL, NULL);
    name = (char *) malloc(size);
    if (name == NULL)
        return -1;
    WideCharToMultiByte(CP_ACP, 0, wName, -1, name, size, NULL, NULL);
    afsi_log("Connection name for %s is '%s'", Guid, name);
    if (*Name)
        *Name = name;
    else
        free(name);
    return 0;
}

// Find the lana number for the given connection name.
extern "C" lana_number_t lana_FindLanaByName(const char *LanaName)
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

    // Open the NetBios Linkage key.
    status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegNetBiosLinkageKeyName, 0, 
                          KEY_QUERY_VALUE, &hkey);
        
    if (status != ERROR_SUCCESS) { 
        afsi_log("Failed to open NetBios Linkage key (status %ld)", status);
        return LANA_INVALID;
    }

    // Read the lana map.
    status = RegQueryValueEx(hkey, "LanaMap", 0, &type,
                         (BYTE *) &lanamap, &lanamapsize);
    if (status != ERROR_SUCCESS) {
        afsi_log("Failed to read LanaMap (status %ld)", status);
        RegCloseKey(hkey);
        return LANA_INVALID;
    }
    if (lanamapsize == 0) {
        afsi_log("No data in LanaMap");
        return LANA_INVALID;
    }
    nlana = lanamapsize / sizeof(lanamap[0]);

    // Get the bind paths for NetBios so we can match them up
    // with the lana map.  First we query for the size, so we
    // can allocate an appropriate buffer.
    status = RegQueryValueEx(hkey, "Bind", 0, &type, NULL, &bindpathsize);
    if (status == ERROR_SUCCESS && bindpathsize != 0) {
        bindpaths = (char *) malloc(bindpathsize * sizeof(char));
        if (bindpaths == NULL) {
            afsi_log("Cannot allocate %ld bytes for bindpaths", bindpathsize);
            RegCloseKey(hkey);
            return LANA_INVALID;
        }
        status = RegQueryValueEx(hkey, "Bind", 0, &type, 
                                 (BYTE *) bindpaths, &bindpathsize);
    }
    RegCloseKey(hkey);
    if (status != ERROR_SUCCESS) {
        afsi_log("Failed to read bind paths (status %ld)", status);
        if (bindpaths != NULL)
            free(bindpaths);
        return LANA_INVALID;
      }
    if (bindpathsize == 0) {
        afsi_log("No bindpath data");
        if (bindpaths != NULL)
            free(bindpaths);
        return LANA_INVALID;
    }

    // Iterate over the lana map entries and bind paths.
    for (i = 0, pBind = bindpaths; i < nlana;
         i++, pBind += strlen(pBind) + 1) {
	// Ignore an invalid map entry.
        if ((lanamap[i].flags & 1) == 0)
            continue;

		// check for a IPv4 binding
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
        if (status == 0 && name != 0) {
            status = strcmp(name, LanaName);
            free(name);
            if (status == 0) {
                free(bindpaths);
		afsi_log("lana_FindLanaByName: Found lana %d for %s",
			 lanamap[i].number, LanaName);
		return lanamap[i].number;
	    }
	}
    }
    free(bindpaths);
    return LANA_INVALID;
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
	afsi_log("Netbios NCBENUM failed: status %ld", status);
	return LANA_INVALID;
    }
    for (i = 0; i < lana_list.length; i++) {
	if (lana_IsLoopback(lana_list.lana[i])) {
	    // Found one, return it.
	    afsi_log("lana_FindLoopback: Found LAN adapter %d",
		     lana_list.lana[i]);
	    return lana_list.lana[i];
	}
    }
    // Could not find a loopback adapter.
    return LANA_INVALID;
}

// Is the given lana a Windows Loopback Adapter?
extern "C" BOOL lana_IsLoopback(lana_number_t lana)
{
    NCB ncb;
    struct {
        ADAPTER_STATUS status;
        NAME_BUFFER names[MAX_LANA+1];
    } astat;
    unsigned char kWLA_MAC[6] = { 0x02, 0x00, 0x4c, 0x4f, 0x4f, 0x50 };
    int status;

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
	afsi_log("NCBRESET failed: lana %u, status %ld", lana, status);
	return FALSE;
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
        afsi_log("NCBASTAT failed: lana %u, status %ld", lana, status);
	return FALSE;
    }
    return (memcmp(astat.status.adapter_address, kWLA_MAC, 6) == 0);
}
