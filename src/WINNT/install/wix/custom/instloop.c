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
#include <stdio.h>
#define INITGUID
#include <guiddef.h>
#include <devguid.h>
#include <setupapi.h>
#include <tchar.h>

#undef REDIRECT_STDOUT

#ifdef USE_PAUSE
#define PAUSE                                          \
    do {                                               \
        char c;                                        \
        printf("PAUSED - PRESS ENTER TO CONTINUE\n");  \
        scanf("%c", &c);                               \
    } while(0)
#else
#define PAUSE
#endif

/*#define USE_SLEEP*/

#ifdef USE_SLEEP
#define SLEEP Sleep(10*1000)
#else
#define SLEEP
#endif


static void ShowUsage(void);
DWORD InstallLoopBack(LPCTSTR pConnectionName, LPCTSTR ip, LPCTSTR mask);
DWORD UnInstallLoopBack(void);
int RenameConnection(PCWSTR GuidString, PCWSTR pszNewName);
DWORD SetIpAddress(LPCWSTR guid, LPCWSTR ip, LPCWSTR mask);
HRESULT LoopbackBindings (LPCWSTR loopback_guid);
BOOL UpdateHostsFile( LPCWSTR swName, LPCWSTR swIp, LPCSTR szFilename, BOOL bPre );

#define DRIVER_DESC "Microsoft Loopback Adapter"
#define DRIVER _T("loopback")
#define MANUFACTURE _T("microsoft")
#define DEFAULT_NAME _T("AFS")
#define DEFAULT_IP _T("10.254.254.253")
#define DEFAULT_MASK _T("255.255.255.252")

static void
ShowUsage(void)
{
    printf("instloop [-i [name [ip mask]] | -u]\n\n");
    printf("    -i  install the %s\n", DRIVER_DESC);
    _tprintf(_T("        (if unspecified, uses name %s,\n"), DEFAULT_NAME);
    _tprintf(_T("         ip %s, and mask %s)\n"), DEFAULT_IP, DEFAULT_MASK);    
    printf("    -u  uninstall the %s\n", DRIVER_DESC);
}

static void
DisplayStartup(BOOL bInstall)
{
    printf("%snstalling the %s\n"
           " (Note: This may take up to a minute or two...)\n",
           bInstall ? "I" : "Un",
           DRIVER_DESC);
    
}

static void
DisplayResult(BOOL bInstall, DWORD rc)
{
    if (rc)
    {
        printf("Could not %sinstall the %s\n", bInstall ? "" : "un",
               DRIVER_DESC);
        SLEEP;
        PAUSE;
    }
}


int _tmain(int argc, TCHAR *argv[])
{
    DWORD rc = 0;
#ifdef REDIRECT_STDOUT
    FILE *fh = NULL;
#endif

    PAUSE;

#ifdef REDIRECT_STDOUT
    fh = freopen("instlog.txt","a+", stdout);
#endif

    if (argc > 1)
    {
        if (_tcsicmp(argv[1], _T("-i")) == 0)
        {
            TCHAR* name = DEFAULT_NAME;
            TCHAR* ip = DEFAULT_IP;
            TCHAR* mask = DEFAULT_MASK;

            if (argc > 2)
            {
                name = argv[2];
                if (argc > 3)
                {
                    if (argc < 5)
                    {
                        ShowUsage();
#ifdef REDIRECT_STDOUT
                        fflush(fh); fclose(fh);
#endif
                        return 1;
                    }
                    else
                    {
                        ip = argv[3];
                        mask = argv[4];
                    }
                }
            }
            DisplayStartup(TRUE);
			if(IsLoopbackInstalled()) {
				printf("Loopback already installed\n");
				rc = 0; /* don't signal an error. */
			} else {
	            rc = InstallLoopBack(name, ip, mask);
			}
            DisplayResult(TRUE, rc);
#ifdef REDIRECT_STDOUT
            fflush(fh); fclose(fh);
#endif
            return rc;
        }
        else if (_tcsicmp(argv[1], _T("-u")) == 0)
        {
            DisplayStartup(FALSE);
            rc = UnInstallLoopBack();
            DisplayResult(FALSE, rc);
#ifdef REDIRECT_STDOUT
            fflush(fh); fclose(fh);
#endif
            return rc;
        }
        ShowUsage();
#ifdef REDIRECT_STDOUT
        fflush(fh); fclose(fh);
#endif
        return 1;
    }
    ShowUsage();
#ifdef REDIRECT_STDOUT
    fflush(fh); fclose(fh);
#endif
    return 0;
}


DWORD UnInstallLoopBack(void)
{
    BOOL ok;
    DWORD ret = 0;
    GUID netGuid;
    HDEVINFO hDeviceInfo = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA DeviceInfoData;
    TCHAR* deviceDesc;
    DWORD index = 0;
    BOOL found = FALSE;
    DWORD size = 0;

    // initialize the structure size
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    // copy the net class GUID
    memcpy(&netGuid, &GUID_DEVCLASS_NET, sizeof(GUID_DEVCLASS_NET));

    // return a device info set contains all installed devices of the Net class
    hDeviceInfo = SetupDiGetClassDevs(&netGuid, NULL, NULL, DIGCF_PRESENT);

    if (hDeviceInfo == INVALID_HANDLE_VALUE)
        return GetLastError();

    deviceDesc = malloc(MAX_PATH*sizeof(TCHAR));
    // enumerate the driver info list
    while (SetupDiEnumDeviceInfo(hDeviceInfo, index, &DeviceInfoData))
    {
        // try to get the DeviceDesc registry property
        ok = SetupDiGetDeviceRegistryProperty(hDeviceInfo, &DeviceInfoData,
                                              SPDRP_DEVICEDESC,
                                              NULL, (PBYTE)deviceDesc,
                                              MAX_PATH * sizeof(TCHAR), &size);
        if (!ok)
        {
            ret = GetLastError();
            if (ret != ERROR_INSUFFICIENT_BUFFER)
                break;
            // if the buffer is too small, reallocate
            free(deviceDesc);
            deviceDesc = malloc(size);
            ok = SetupDiGetDeviceRegistryProperty(hDeviceInfo,
                                                  &DeviceInfoData,
                                                  SPDRP_DEVICEDESC,
                                                  NULL, (PBYTE)deviceDesc,
                                                  size, NULL);
            if (!ok)
                break;
        }

        // case insensitive comparison
        _tcslwr(deviceDesc);
        if( _tcsstr(deviceDesc, DRIVER))
        {
            found = TRUE;
            break;
        }

        index++;
    }

    free(deviceDesc);
    
    if (found == FALSE)
    {
        ret = GetLastError();
        printf("The %s does not seem to be installed\n", DRIVER_DESC);
        goto cleanup;
    }

    ok = SetupDiSetSelectedDevice(hDeviceInfo, &DeviceInfoData);
    if (!ok)
    {
        ret = GetLastError();
        goto cleanup;
    }
    ok = SetupDiCallClassInstaller(DIF_REMOVE, hDeviceInfo, &DeviceInfoData);
    if (!ok)
    {
        ret = GetLastError();
        goto cleanup;
    }

cleanup:
    // clean up the device info set
    if (hDeviceInfo != INVALID_HANDLE_VALUE)
        SetupDiDestroyDeviceInfoList(hDeviceInfo);

    return ret;
}

BOOL IsLoopbackInstalled()
{
    TCHAR * hwid = _T("*MSLOOP");
    HDEVINFO DeviceInfoSet;
    SP_DEVINFO_DATA DeviceInfoData;
    DWORD i,err;
    BOOL found;
    
    //
    // Create a Device Information Set with all present devices.
    //
    DeviceInfoSet = SetupDiGetClassDevs(NULL, 0, 0, DIGCF_ALLCLASSES | DIGCF_PRESENT ); // All devices present on system
    if (DeviceInfoSet == INVALID_HANDLE_VALUE)
    {
        return FALSE; // nothing installed?
    }
    
    //
    //  Enumerate through all Devices.
    //
    found = FALSE;
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    for (i=0; SetupDiEnumDeviceInfo(DeviceInfoSet,i,&DeviceInfoData); i++)
    {
        DWORD DataT;
        TCHAR *p, *buffer = NULL;
        DWORD buffersize = 0;
        
        //
        // We won't know the size of the HardwareID buffer until we call
        // this function. So call it with a null to begin with, and then 
        // use the required buffer size to Alloc the nessicary space.
        // Keep calling we have success or an unknown failure.
        //
        while (!SetupDiGetDeviceRegistryProperty(DeviceInfoSet,&DeviceInfoData,SPDRP_HARDWAREID,&DataT,(PBYTE)buffer,buffersize,&buffersize))
        {
            if (GetLastError() == ERROR_INVALID_DATA)
            {
                // May be a Legacy Device with no hwid. Continue.
                break;
            }
            else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                // We need to change the buffer size.
                if (buffer) 
                    LocalFree(buffer);
                buffer = (TCHAR *)LocalAlloc(LPTR,buffersize);
            }
            else
            {
                goto cleanup_DeviceInfo;
            }            
        }
        
        if (GetLastError() == ERROR_INVALID_DATA) 
            continue;
        
        // Compare each entry in the buffer multi-sz list with our hwid.
        for (p=buffer; *p && (p < &buffer[buffersize]); p += _tcslen(p)+1)
        {
            if (!_tcsicmp(hwid,p))
            {
                found = TRUE;
                break;
            }
        }
        
        if (buffer) LocalFree(buffer);
        if (found) break;
    }
    
    //  Cleanup.
cleanup_DeviceInfo:
    err = GetLastError();
    SetupDiDestroyDeviceInfoList(DeviceInfoSet);
    SetLastError(err);
    
    return found;
}


DWORD InstallLoopBack(LPCTSTR pConnectionName, LPCTSTR ip, LPCTSTR mask)
{
    BOOL ok;
    DWORD ret = 0;
    GUID netGuid;
    HDEVINFO hDeviceInfo = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA DeviceInfoData;
    SP_DRVINFO_DATA DriverInfoData;
    SP_DEVINSTALL_PARAMS  DeviceInstallParams;
    TCHAR className[MAX_PATH];
    TCHAR temp[MAX_PATH];
    DWORD index = 0;
    BOOL found = FALSE;
    BOOL registered = FALSE;
    BOOL destroyList = FALSE;

    HKEY hkey = NULL;
    DWORD cbSize;
    DWORD dwValueType;
    TCHAR pCfgGuidString[40];

    // initialize the structure size
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    DriverInfoData.cbSize = sizeof(SP_DRVINFO_DATA);

    // copy the net class GUID
    memcpy(&netGuid, &GUID_DEVCLASS_NET, sizeof(GUID_DEVCLASS_NET));

    // create an empty device info set associated with the net class GUID
    hDeviceInfo = SetupDiCreateDeviceInfoList(&netGuid, NULL);
    if (hDeviceInfo == INVALID_HANDLE_VALUE)
        return GetLastError();

    // get the class name from GUID
    ok = SetupDiClassNameFromGuid(&netGuid, className, MAX_PATH, NULL);
    if (!ok)
    {
        ret = GetLastError();
        goto cleanup;
    }

    // create a device info element and add the new device instance
    // key to registry
    ok = SetupDiCreateDeviceInfo(hDeviceInfo, className, &netGuid, NULL, NULL,
                                 DICD_GENERATE_ID, &DeviceInfoData);
    if (!ok)
    {
        ret = GetLastError();
        goto cleanup;
    }

    // select the newly created device info to be the currently
    // selected member
    ok = SetupDiSetSelectedDevice(hDeviceInfo, &DeviceInfoData);
    if (!ok)
    {
        ret = GetLastError();
        goto cleanup;
    }

    // build a list of class drivers
    ok = SetupDiBuildDriverInfoList(hDeviceInfo, &DeviceInfoData,
                                    SPDIT_CLASSDRIVER);
    if (!ok)
    {
        ret = GetLastError();
        goto cleanup;
    }

    destroyList = TRUE;

    // enumerate the driver info list
    while (SetupDiEnumDriverInfo(hDeviceInfo, &DeviceInfoData,
                                 SPDIT_CLASSDRIVER, index, &DriverInfoData))
    {
        // if the manufacture is microsoft
        if (_tcsicmp(DriverInfoData.MfgName, MANUFACTURE) == 0)
        {
            // case insensitive search for loopback
            _tcscpy(temp, DriverInfoData.Description);
            _tcslwr(temp);
            if( _tcsstr(temp, DRIVER))
            {
                found = TRUE;
                break;
            }
        }
        index++;
    }

    if (!found)
    {
        ret = GetLastError();
        printf("Could not find the %s driver to install\n", DRIVER_DESC);
        goto cleanup;
    }

    // set the loopback driver to be the currently selected
    ok = SetupDiSetSelectedDriver(hDeviceInfo, &DeviceInfoData,
                                  &DriverInfoData);
    if (!ok)
    {
        ret = GetLastError();
        goto cleanup;
    }

    // register the phantom device to repare for install
    ok = SetupDiCallClassInstaller(DIF_REGISTERDEVICE, hDeviceInfo,
                                   &DeviceInfoData);
    if (!ok)
    {
        ret = GetLastError();
        goto cleanup;
    }

    // registered, but remove if errors occur in the following code
    registered = TRUE;

    // ask the installer if we can install the device
    ok = SetupDiCallClassInstaller(DIF_ALLOW_INSTALL, hDeviceInfo,
                                   &DeviceInfoData);
    if (!ok)
    {
        ret = GetLastError();
        if (ret != ERROR_DI_DO_DEFAULT)
        {
            goto cleanup;
        }
        else
            ret = 0;
    }

    // install the files first
    ok = SetupDiCallClassInstaller(DIF_INSTALLDEVICEFILES, hDeviceInfo,
                                   &DeviceInfoData);
    if (!ok)
    {
        ret = GetLastError();
        goto cleanup;
    }

    // get the device install parameters and disable filecopy
    DeviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
    ok = SetupDiGetDeviceInstallParams(hDeviceInfo, &DeviceInfoData,
                                       &DeviceInstallParams);
    if (ok)
    {
        DeviceInstallParams.Flags |= DI_NOFILECOPY;
        ok = SetupDiSetDeviceInstallParams(hDeviceInfo, &DeviceInfoData,
                                           &DeviceInstallParams);
        if (!ok)
        {
            ret = GetLastError();
            goto cleanup;
        }
    }

    //
    // Register any device-specific co-installers for this device,
    //

    ok = SetupDiCallClassInstaller(DIF_REGISTER_COINSTALLERS,
                                   hDeviceInfo,
                                   &DeviceInfoData);
    if (!ok)
    {
        ret = GetLastError();
        goto cleanup;
    }

    //
    // install any  installer-specified interfaces.
    // and then do the real install
    //
    ok = SetupDiCallClassInstaller(DIF_INSTALLINTERFACES,
                                   hDeviceInfo,
                                   &DeviceInfoData);
    if (!ok)
    {
        ret = GetLastError();
        goto cleanup;
    }
    PAUSE;
    ok = SetupDiCallClassInstaller(DIF_INSTALLDEVICE,
                                   hDeviceInfo,
                                   &DeviceInfoData);
    if (!ok)
    {
        ret = GetLastError();
        PAUSE;
        goto cleanup;
    }

    /* Skip to the end if we aren't setting the name */
    if (!pConnectionName) goto cleanup;

    // Figure out NetCfgInstanceId
    hkey = SetupDiOpenDevRegKey(hDeviceInfo,
                                &DeviceInfoData,
                                DICS_FLAG_GLOBAL,
                                0,
                                DIREG_DRV,
                                KEY_READ);
    if (hkey == INVALID_HANDLE_VALUE)
    {
        ret = GetLastError();
        goto cleanup;
    }

    cbSize = sizeof(pCfgGuidString);
    ret = RegQueryValueEx(hkey, _T("NetCfgInstanceId"), NULL,
                          &dwValueType, (LPBYTE)pCfgGuidString, &cbSize);
    RegCloseKey(hkey);

    ret = RenameConnection(pCfgGuidString, pConnectionName);
    if (ret)
    {
        printf("Could not set the connection name to \"%S\"\n",
               pConnectionName);
        goto cleanup;
    }

    if (!ip) goto cleanup;
    ret = SetIpAddress(pCfgGuidString, ip, mask);
    if (ret)
    {
        printf("Could not set the ip address and network mask\n");
        goto cleanup;
    }
    ret = LoopbackBindings(pCfgGuidString);
    if (ret)
    {
        printf("Could not properly set the bindings\n");
        goto cleanup;
    }
    ret = !UpdateHostsFile( pConnectionName, ip, "hosts", FALSE );
    if (ret)
    {
        printf("Could not update hosts file\n");
        goto cleanup;
    }
    ret = !UpdateHostsFile( pConnectionName, ip, "lmhosts", TRUE );
    if (ret)
    {
        printf("Could not update lmhosts file\n");
        goto cleanup;
    }


cleanup:
    // an error has occured, but the device is registered, we must remove it
    if (ret != 0 && registered)
        SetupDiCallClassInstaller(DIF_REMOVE, hDeviceInfo, &DeviceInfoData);

    found = SetupDiDeleteDeviceInfo(hDeviceInfo, &DeviceInfoData);

    // destroy the driver info list
    if (destroyList)
        SetupDiDestroyDriverInfoList(hDeviceInfo, &DeviceInfoData,
                                     SPDIT_CLASSDRIVER);
    // clean up the device info set
    if (hDeviceInfo != INVALID_HANDLE_VALUE)
        SetupDiDestroyDeviceInfoList(hDeviceInfo);

    return ret;
}
