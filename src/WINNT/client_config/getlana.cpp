// getlana.cpp : Defines the entry point for the console application.
//

#include <afx.h>
#include <windows.h>
#include <winreg.h>
#include <nb30.h>
#include <tchar.h>
#include <shellapi.h>
#include <iostream>
#include <objbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wtypes.h>
#include <string.h>
#include <malloc.h>
#include <winsock2.h>
#include <getlana.h>



#define LANA_INVALID 0xff
using namespace std;
BOOL lana_IsLoopback(lana_number_t lana, TCHAR*);
lana_number_t lana_FindLoopback(TCHAR*);



// Use the IShellFolder API to get the connection name for the given Guid.
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
      {
        // Create an instance of the network connections folder.
        hr = CoCreateInstance(CLSID_NetworkConnections, NULL,
			      CLSCTX_INPROC_SERVER, IID_IShellFolder,
			      reinterpret_cast<LPVOID *>(&pShellFolder));
      }
    if (SUCCEEDED(hr))
        hr = pShellFolder->ParseDisplayName(NULL, NULL, szAdapterGuid, NULL,
					    &pidl, NULL);
    if (SUCCEEDED(hr))
      {
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
static int lana_GetNameFromGuid(char *Guid, char **Name)
{
    typedef HRESULT (WINAPI *HrLanProcAddr)(GUID *, PCWSTR, PWSTR, LPDWORD);
    HrLanProcAddr HrLanProc = NULL;
    HMODULE hNetMan;
    int size;
    WCHAR *wGuid = NULL;
    WCHAR wName[MAX_PATH];
    DWORD NameSize = (sizeof(wName) / sizeof(wName[0]));
    HRESULT status;
        
    // Convert the Guid string to Unicode.  First we ask only for the size
    // of the converted string.  Then we allocate a buffer of sufficient
    // size to hold the result of the conversion.
    size = MultiByteToWideChar(CP_ACP, 0, Guid, -1, NULL, 0);
    wGuid = (WCHAR *) malloc(size * sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, Guid, -1, wGuid, size);

    // First try the IShellFolder interface, which was unimplemented
    // for the network connections folder before XP.
    status = getname_shellfolder(wGuid, wName, NameSize);
    if (status == E_NOTIMPL)
      {
	// The IShellFolder interface is not implemented on this platform.
	// Try the (undocumented) HrLanConnectionNameFromGuidOrPath API
	// from the netman DLL.
        hNetMan = LoadLibrary("netman.dll");
	if (hNetMan == NULL)
	  {
	    free(wGuid);
            return -1;
	  }
	HrLanProc =
          (HrLanProcAddr) GetProcAddress(hNetMan,
					"HrLanConnectionNameFromGuidOrPath");
	if (HrLanProc == NULL)
	  {
            FreeLibrary(hNetMan);
	    free(wGuid);
            return -1;
	  }
	// Super Secret Microsoft Call
	status = HrLanProc(NULL, wGuid, wName, &NameSize);
	FreeLibrary(hNetMan);
    }
    free(wGuid);
    if (FAILED(status))
      {
        cerr << "lana_GetNameFromGuid: failed to get connection name (status "
	     << status << ")\r\n";
	return -1;
      }

    // Get the required buffer size, and then convert the string.
    size = WideCharToMultiByte(CP_ACP, 0, wName, -1, NULL, 0, NULL, NULL);
    *Name = (char *) malloc(size);
    if (*Name == NULL)
        return -1;
    WideCharToMultiByte(CP_ACP, 0, wName, -1, *Name, size, NULL, NULL);
    return 0;
}

LANAINFO* GetLana(TCHAR* msg, const char *LanaName)
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

    LANAINFO* lanainfo;

    // Open the NetBios Linkage key.
    status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegNetBiosLinkageKeyName, 0, 
                          KEY_QUERY_VALUE, &hkey);
        
    if (status != ERROR_SUCCESS)
      { 
	      _stprintf(msg, _T("Failed to open NetBios Linkage key (status %d)"),  status);
        return NULL;
      }

    // Read the lana map.
    status = RegQueryValueEx(hkey, "LanaMap", 0, &type,
                         (BYTE *) &lanamap, &lanamapsize);
    if (status != ERROR_SUCCESS)
      {
        _stprintf(msg, _T("Failed to read LanaMap (status %d)"), status);
        RegCloseKey(hkey);
        return NULL;
      }
    if (lanamapsize == 0)
      {
        _stprintf(msg, _T("No data in LanaMap"));
        return NULL;
      }
    nlana = lanamapsize / sizeof(lanamap[0]);

    // Get the bind paths for NetBios so we can match them up
    // with the lana map.  First we query for the size, so we
    // can allocate an appropriate buffer.
    status = RegQueryValueEx(hkey, "Bind", 0, &type, NULL, &bindpathsize);
    if (status == ERROR_SUCCESS && bindpathsize != 0)
      {
        bindpaths = (char *) malloc(bindpathsize * sizeof(char));
        if (bindpaths == NULL)
	  {
            _stprintf(msg, _T("Cannot allocate %d bytes for bindpaths"), bindpathsize);
		
            RegCloseKey(hkey);
            return NULL;
	  }
        status = RegQueryValueEx(hkey, "Bind", 0, &type, 
                                 (BYTE *) bindpaths, &bindpathsize);
      }
    RegCloseKey(hkey);
    if (status != ERROR_SUCCESS)
      {
        _stprintf(msg, _T("Failed to read bind paths (status %d)"), status); 
        if (bindpaths != NULL)
            free(bindpaths);
        return NULL;
      }
    if (bindpathsize == 0)
      {
        _stprintf(msg,  _T("No bindpath data"));
        if (bindpaths != NULL)
            free(bindpaths);
        return NULL;
    }

    if (LanaName)
    {
      lanainfo = new LANAINFO[1];
      lanainfo[0].lana_number = LANA_INVALID;
      memset(lanainfo[0].lana_name, 0, sizeof(lanainfo[0].lana_name));
    }
    else
    {
      lanainfo = new LANAINFO[nlana+1];
      memset(lanainfo, 0, sizeof(LANAINFO) * nlana);
    }
    int index = 0;
    for (i = 0, pBind = bindpaths; i < nlana;
         i++, pBind += strlen(pBind) + 1)
      {
	// Ignore an invalid map entry.
        if ((lanamap[i].flags & 1) == 0)
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
        if (p == NULL)
	  {
	    free(guid);                 // Malformed GUID?
            continue;
	  }
        *++p = '\0';                    // Ignore anything after the GUID.
        status = lana_GetNameFromGuid(guid, &name);
        if (status == 0)
        {
          if (LanaName)
          {
            if (strcmp(name, LanaName) ==0)
            {
              lanainfo[index].lana_number = lanamap[i].number;
              _tcscpy(lanainfo[index].lana_name ,name);
              free(name);
              free(guid);
              break;
            }
          }
          else
          {
            lanainfo[index].lana_number = lanamap[i].number;
            _tcscpy(lanainfo[index].lana_name ,name);
            free(name);
            index++;
          }
        }

        free(guid);
      }
    free(bindpaths);
    return lanainfo;
} 


lana_number_t lana_FindLoopback(TCHAR* msg)
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
	    _stprintf(msg, _T("Netbios NCBENUM failed: status %ld"), status);
	    return LANA_INVALID;
    }
    
    for (i = 0; i < lana_list.length; i++) {
	    if (lana_IsLoopback(lana_list.lana[i], msg)) {
	    // Found one, return it.
	      return lana_list.lana[i];
      }
    }
    // Could not find a loopback adapter.
    return LANA_INVALID;
}


// Is the given lana a Windows Loopback Adapter?
BOOL lana_IsLoopback(lana_number_t lana, TCHAR* msg)
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
	    sprintf(msg, "NCBRESET failed: lana %u, status %ld", lana, status);
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
        sprintf(msg, "NCBASTAT failed: lana %u, status %ld", lana, status);
	      return FALSE;
    }
    return (memcmp(astat.status.adapter_address, kWLA_MAC, 6) == 0);
}

#define NETBIOS_NAME_FULL 0
#define NETBIOS_NAME_SUFFIX 1
#define AFSCONFIGKEYNAME TEXT("SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters")

void GetUncServerName(int lanaNumber, BOOL isGateway, TCHAR* name, int type)
{
    lana_number_t lana = LANA_INVALID;
    LANAINFO* lanainfo;
    WSADATA WSAjunk;
    char cm_HostName[MAX_PATH];
    TCHAR tmpName[MAX_PATH];
    TCHAR msg[MAX_PATH];
    memset(msg, 0, sizeof(msg));
    memset(tmpName, 0, sizeof(tmpName));
    memset(cm_HostName, 0, sizeof(cm_HostName));
	WSAStartup(0x0101, &WSAjunk);

    if (lanaNumber == -1) {
		/* Find the default LAN adapter to use.  First look for
         * the adapter named AFS; otherwise, unless we are doing
		 * gateway service, look for any valid loopback adapter.
		 */
		lanainfo = GetLana(msg, "AFS");
        if (lanainfo)
        {
            lana = lanainfo[0].lana_number;
            delete lanainfo;
        }
		if (lana == LANA_INVALID && !isGateway)
			lana = lana_FindLoopback(msg);
		if (lana != LANA_INVALID)
            lanaNumber = lana;
	}
	/* If we are using a loopback adapter, we can use the preferred
     * (but non-unique) server name; otherwise, we must fall back to
	 * the <machine>-AFS name.
     */
    if (lanaNumber >= 0 && lana_IsLoopback(lanaNumber, msg))
    {
        HKEY parmKey;
        char explicitNetbiosName[MAX_PATH+1];
        DWORD len=sizeof(explicitNetbiosName)-1;
        if ((RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSCONFIGKEYNAME,0, KEY_QUERY_VALUE, &parmKey)!= ERROR_SUCCESS) 
             || (RegQueryValueEx(parmKey, "NetbiosName", NULL, NULL,(LPBYTE)(explicitNetbiosName), &len)!= ERROR_SUCCESS)
             || (len > sizeof(explicitNetbiosName)-1)
             ) 
            strcpy(explicitNetbiosName, "AFS"); 
        RegCloseKey(parmKey);
        explicitNetbiosName[len]=0;       /*safety see ms-help://MS.MSDNQTR.2002OCT.1033/sysinfo/base/regqueryvalueex.htm*/
        _tcscpy(name, explicitNetbiosName);
    } else {
        gethostname(cm_HostName, sizeof(cm_HostName));
		_tcscpy(tmpName, cm_HostName);
		char* ctemp = _tcschr(tmpName, '.');	/* turn ntdfs.* into ntdfs */
		if (ctemp) *ctemp = 0;
		tmpName[11] = 0; /* ensure that even after adding the -A, we
                          * leave one byte free for the netbios server
                          * type.
                          */
        if (type == NETBIOS_NAME_FULL) {
		    _tcscat(tmpName, _T("-afs"));
            _tcscpy(name, tmpName);
        } else {
            _tcscpy(name, _T("-afs"));
        }
	}
}

void GetAfsName(int lanaNumber, BOOL isGateway, TCHAR* name)
{
    TCHAR tmpName[MAX_PATH];
    GetUncServerName(lanaNumber, isGateway, tmpName, NETBIOS_NAME_FULL);
    _stprintf(name, _T("Your UNC name to reach the root of AFS is \\\\%s\\all"), tmpName);
}

