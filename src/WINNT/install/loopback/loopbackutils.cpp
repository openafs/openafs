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

#define _WIN32_DCOM
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

struct Args {
	bool bQuiet;
	LPWSTR lpIPAddr;
	LPWSTR lpSubnetMask;
    LPWSTR lpConnectionName;
	Args () : bQuiet (0), lpIPAddr (0), lpSubnetMask (0), lpConnectionName (0) { }
	~Args () {
		if (lpIPAddr) free (lpIPAddr);
		if (lpSubnetMask) free (lpSubnetMask);
        if (lpConnectionName) free (lpConnectionName);
	}
};

#include <shellapi.h>
#define INITGUID
#include <guiddef.h>
#include <devguid.h>
#include <objbase.h>
#include <setupapi.h>
#include <tchar.h>
#include <Msi.h>
#include <Msiquery.h>
#include "loopbackutils.h"

extern "C" DWORD UnInstallLoopBack(void)
{
    BOOL ok;
    DWORD ret = 0;
    GUID netGuid;
    HDEVINFO hDeviceInfo = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA DeviceInfoData;
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

    // enumerate the driver info list
    while (TRUE)
    {
        TCHAR * deviceHwid;

        ok = SetupDiEnumDeviceInfo(hDeviceInfo, index, &DeviceInfoData);

	if(!ok) 
	{
	  if(GetLastError() == ERROR_NO_MORE_ITEMS)
	      break;
	  else 
	  {
   	      index++;
	      continue;
	  }
	}

        // try to get the DeviceDesc registry property
        ok = SetupDiGetDeviceRegistryProperty(hDeviceInfo, 
					      &DeviceInfoData,
                                              SPDRP_HARDWAREID,
                                              NULL,
					      NULL,
                                              0, 
					      &size);
        if (!ok)
        {
            ret = GetLastError();
            if (ret != ERROR_INSUFFICIENT_BUFFER) {
                index++;
                continue;
	    }

            deviceHwid = (TCHAR *)malloc(size);
            ok = SetupDiGetDeviceRegistryProperty(hDeviceInfo,
                                                  &DeviceInfoData,
                                                  SPDRP_HARDWAREID,
                                                  NULL, 
						  (PBYTE)deviceHwid,
                                                  size, 
						  NULL);
            if (!ok) {
	        free(deviceHwid);
	        deviceHwid = NULL;
	        index++;
	        continue;
	    }
        } else {
	    // something is wrong.  This shouldn't have worked with a NULL buffer
	    ReportMessage(0, "GetDeviceRegistryProperty succeeded with a NULL buffer", NULL,  NULL, 0);
	    index++;
	    continue;
	}

	for (TCHAR *t = deviceHwid; t && *t && t < &deviceHwid[size / sizeof(TCHAR)]; t += _tcslen(t) + 1)
	{
	    if(!_tcsicmp(DRIVERHWID, t)) {
	        found = TRUE;
		break;
	    }
	}

	if (deviceHwid) {
	    free(deviceHwid);
	    deviceHwid = NULL;
	}

	if (found)
            break;
	
        index++;
    }

    if (found == FALSE)
    {
        ret = GetLastError();
        ReportMessage(0,"Driver does not seem to be installed", DRIVER_DESC, NULL, ret);
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

    ret = 0;

cleanup:
    // clean up the device info set
    if (hDeviceInfo != INVALID_HANDLE_VALUE)
        SetupDiDestroyDeviceInfoList(hDeviceInfo);

    return ret;
}

BOOL IsLoopbackInstalled(void)
{
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
    for (i=0; ;i++)
    {
        BOOL ok;
        DWORD DataT;
        TCHAR *p, *buffer = NULL;
        DWORD buffersize = 0;

	ok = SetupDiEnumDeviceInfo(DeviceInfoSet, i, &DeviceInfoData);

	if(!ok) {
	  if(GetLastError() == ERROR_NO_MORE_ITEMS)
	    break;
	  else
	    continue;
	}
        
        //
        // We won't know the size of the HardwareID buffer until we call
        // this function. So call it with a null to begin with, and then 
        // use the required buffer size to Alloc the nessicary space.
        // Keep calling we have success or an unknown failure.
        //
        while (!SetupDiGetDeviceRegistryProperty(DeviceInfoSet,
						 &DeviceInfoData,
						 SPDRP_HARDWAREID,
						 &DataT,
						 (PBYTE)buffer,
						 buffersize,
						 &buffersize))
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
	        if (buffer)
                    LocalFree(buffer);
                goto cleanup_DeviceInfo;
            }
        }
        
        if (GetLastError() == ERROR_INVALID_DATA) {
	    if (buffer)
                LocalFree(buffer);
            continue;
	}
        
        // Compare each entry in the buffer multi-sz list with our hwid.
        for (p=buffer; *p && (p < &buffer[buffersize]); p += _tcslen(p)+1)
        {
            if (!_tcsicmp(DRIVERHWID,p))
            {
                found = TRUE;
                break;
            }
        }
        
        if (buffer)
	    LocalFree(buffer);
        if (found) 
	    break;
    }
    
    //  Cleanup.
cleanup_DeviceInfo:
    err = GetLastError();
    SetupDiDestroyDeviceInfoList(DeviceInfoSet);
    SetLastError(err);
    
    return found;
}


extern "C" DWORD InstallLoopBack(LPCTSTR pConnectionName, LPCTSTR ip, LPCTSTR mask)
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
    PSP_DRVINFO_DETAIL_DATA pDriverInfoDetail;
    DWORD detailBuf[2048];    // for our purposes, 8k buffer is more
			      // than enough to obtain the hardware ID
			      // of the loopback driver.

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
    while (TRUE)
    {
        BOOL ret;

	ret = SetupDiEnumDriverInfo(hDeviceInfo, &DeviceInfoData,
				  SPDIT_CLASSDRIVER, index, &DriverInfoData);

	// if the function failed and GetLastError() returned
	// ERROR_NO_MORE_ITEMS, then we have reached the end of the
	// list.  Othewise there was something wrong with this
	// particular driver.
	if(!ret) {
	  if(GetLastError() == ERROR_NO_MORE_ITEMS)
	    break;
	  else {
	    index++;
	    continue;
	  }
	}

	pDriverInfoDetail = (PSP_DRVINFO_DETAIL_DATA) detailBuf;
	pDriverInfoDetail->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);

	// if we successfully find the hardware ID and it turns out to
	// be the one for the loopback driver, then we are done.
	if (SetupDiGetDriverInfoDetail(hDeviceInfo,
				      &DeviceInfoData,
				      &DriverInfoData,
				      pDriverInfoDetail,
				      sizeof(detailBuf),
				      NULL)) {
            TCHAR * t;

	    // pDriverInfoDetail->HardwareID is a MULTISZ string.  Go through the
	    // whole list and see if there is a match somewhere.
	    t = pDriverInfoDetail->HardwareID;
	    while (t && *t && t < (TCHAR *) &detailBuf[sizeof(detailBuf)/sizeof(detailBuf[0])]) {
	      if (!_tcsicmp(t, DRIVERHWID))
		break;

	      t += _tcslen(t) + 1;
	    }

	    if (t && *t && t < (TCHAR *) &detailBuf[sizeof(detailBuf)/sizeof(detailBuf[0])]) {
	      found = TRUE;
	      break;
	    }
	}

        index++;
    }

    if (!found)
    {
        ret = GetLastError();
        ReportMessage(0,"Could not find the driver to install", DRIVER_DESC, NULL, 0);
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
        ReportMessage(0,"Could not set the connection name", NULL, pConnectionName, 0);
        goto cleanup;
    }

    if (!ip) goto cleanup;
    ret = SetIpAddress(pCfgGuidString, ip, mask);
    if (ret)
    {
        ReportMessage(0,"Could not set the ip address and network mask",NULL,NULL,0);
        goto cleanup;
    }
    ret = LoopbackBindings(pCfgGuidString);
    if (ret)
    {
        ReportMessage(0,"Could not properly set the bindings",NULL,NULL,0);
        goto cleanup;
    }
    ret = !UpdateHostsFile( pConnectionName, ip, "hosts", FALSE );
    if (ret)
    {
        ReportMessage(0,"Could not update hosts file",NULL,NULL,0);
        goto cleanup;
    }
    ret = !UpdateHostsFile( pConnectionName, ip, "lmhosts", TRUE );
    if (ret)
    {
        ReportMessage(0,"Could not update lmhosts file",NULL,NULL,0);
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
};

/* The following functions provide the RunDll32 interface 
 *    RunDll32 loopback_install.dll doLoopBackEntry [Interface Name] [IP address] [Subnet Mask]
 */

static void wcsMallocAndCpy (LPWSTR * dst, const LPWSTR src) {
	*dst = (LPWSTR) malloc ((wcslen (src) + 1) * sizeof (WCHAR));
	wcscpy (*dst, src);
}

static void display_usage()
{
    MessageBoxW( NULL, 
                 L"Installation utility for the MS Loopback Adapter\r\n\r\n"
                 L"Usage:\r\n"
                 L"RunDll32 loopback_install.dll doLoopBackEntry [q|quiet] [Connection Name] [IP address] [Submask]\r\n",
                 L"loopback_install", MB_ICONINFORMATION | MB_OK );
}

static int process_args (LPWSTR lpCmdLine, int skip, Args & args) {
	int i, iNumArgs;
	LPWSTR * argvW;

	argvW = CommandLineToArgvW (lpCmdLine, &iNumArgs);
	// Skip over the command name
	for (i = skip; i < iNumArgs; i++)
	{
		if (wcsstr (argvW[i], L"help")
			|| !_wcsicmp (argvW[i], L"?")
			|| (wcslen(argvW[i]) == 2 && argvW[i][1] == L'?'))
		{
			display_usage();
			GlobalFree (argvW);
			return 0;
		}

		if (!_wcsicmp (argvW[i], L"q") || !_wcsicmp (argvW[i], L"quiet")) {
			args.bQuiet = true;
			continue;
		}

		if (!args.lpConnectionName) {
			wcsMallocAndCpy (&args.lpConnectionName, argvW[i]);
			continue;
		}

		if (!args.lpIPAddr) {
			wcsMallocAndCpy (&args.lpIPAddr, argvW[i]);
			continue;
		}

		if (!args.lpSubnetMask) {
			wcsMallocAndCpy (&args.lpSubnetMask, argvW[i]);
			continue;
		}

		display_usage();
		GlobalFree (argvW);
		return 0;
	}

	if (!args.lpConnectionName)
		wcsMallocAndCpy (&args.lpConnectionName, DEFAULT_NAME);
	if (!args.lpIPAddr)
		wcsMallocAndCpy (&args.lpIPAddr, DEFAULT_IP);
	if (!args.lpSubnetMask)
		wcsMallocAndCpy (&args.lpSubnetMask, DEFAULT_MASK);

	GlobalFree (argvW);

	return 1;
}

void CALLBACK doLoopBackEntryW (HWND hwnd, HINSTANCE hinst, LPWSTR lpCmdLine, int nCmdShow)
{
	Args args;

	if (!process_args(lpCmdLine, 0, args)) 
        return;

	InstallLoopBack(args.lpConnectionName, args.lpIPAddr, args.lpSubnetMask);
}

void CALLBACK uninstallLoopBackEntryW (HWND hwnd, HINSTANCE hinst, LPWSTR lpCmdLine, int nCmdSHow)
{
    Args args;

    // initialize COM
	// This and CoInitializeSecurity fail when running under the MSI
	// engine, but there seems to be no ill effect (the security is now
	// set on the specific object via CoSetProxyBlanket in loopback_configure)
    if(CoInitializeEx(NULL, COINIT_DISABLE_OLE1DDE | COINIT_APARTMENTTHREADED ))
    {
		//Don't fail (MSI install will have already initialized COM)
        //EasyErrorBox(0, L"Failed to initialize COM.");
        //return 1;
    }

	// Initialize COM security (otherwise we'll get permission denied when we try to use WMI or NetCfg)
    CoInitializeSecurity( NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);

    UnInstallLoopBack();

	CoUninitialize();
}

/* And an MSI installer interface */

UINT __stdcall installLoopbackMSI (MSIHANDLE hInstall)
{
	LPWSTR szValueBuf;
	DWORD cbValueBuf = 256;
	Args args;
	UINT rc;

	SetMsiReporter("InstallLoopback", "Installing loopback adapter", hInstall);

    /* check if there is already one installed.  If there is, we shouldn't try to 
     * install another.
     */
	if(IsLoopbackInstalled())
        return ERROR_SUCCESS;

	szValueBuf = (LPWSTR) malloc (cbValueBuf * sizeof (WCHAR));
	while (rc = MsiGetPropertyW(hInstall, L"CustomActionData", szValueBuf, &cbValueBuf)) {
		free (szValueBuf);
		if (rc == ERROR_MORE_DATA) {
			cbValueBuf++;
			szValueBuf = (LPWSTR) malloc (cbValueBuf * sizeof (WCHAR));
		} 
        else 
            return ERROR_INSTALL_FAILURE;
	}

	if (!process_args(szValueBuf, 1, args)) 
        return ERROR_INSTALL_FAILURE;
		
	rc = InstallLoopBack (args.lpConnectionName, args.lpIPAddr, args.lpSubnetMask);

	if (rc != 2 && rc != 0) 
        return ERROR_INSTALL_FAILURE;

	if (rc == 2) {
		MsiDoActionW (hInstall, L"ScheduleReboot");
	}

	return ERROR_SUCCESS;
}

UINT __stdcall uninstallLoopbackMSI (MSIHANDLE hInstall)
{
	LPWSTR szValueBuf;
	DWORD cbValueBuf = 256;
	Args args;
	UINT rc;

	SetMsiReporter("RemoveLoopback", "Removing loopback adapter",  hInstall);

	szValueBuf = (LPWSTR) malloc (cbValueBuf * sizeof (WCHAR));
	while (rc = MsiGetPropertyW(hInstall, L"CustomActionData", szValueBuf, &cbValueBuf)) {
		free (szValueBuf);
		if (rc == ERROR_MORE_DATA) {
			cbValueBuf++;
			szValueBuf = (LPWSTR) malloc (cbValueBuf * sizeof (WCHAR));
		} 
        else 
            return ERROR_INSTALL_FAILURE;
	}

	if (!process_args(szValueBuf, 1, args)) 
        return ERROR_INSTALL_FAILURE;
		
	rc = UnInstallLoopBack ();

	if (rc == 1) 
        return ERROR_INSTALL_FAILURE;

	if (rc == 2) {
		MsiDoActionW (hInstall, L"ScheduleReboot");
	}

	return ERROR_SUCCESS;
}

DWORD hMsiHandle = 0;
DWORD dwReporterType = REPORT_PRINTF;

extern "C" void ReportMessage(int level, LPCSTR msg, LPCSTR str, LPCWSTR wstr, DWORD dw) {
    if(dwReporterType == REPORT_PRINTF)
		printf("%s:[%s][%S][%d]\n", (msg?msg:""), (str?str:""), (wstr?wstr:L""), dw);
	else if(dwReporterType == REPORT_MSI && hMsiHandle && level == 0) {
		MSIHANDLE hRec = MsiCreateRecord(5);
        
		MsiRecordClearData(hRec);
		MsiRecordSetStringA(hRec,1,(msg)?msg:"");
		MsiRecordSetStringA(hRec,2,(str)?str:"");
		MsiRecordSetStringW(hRec,3,(wstr)?wstr:L"");
		MsiRecordSetInteger(hRec,4,dw);

		MsiProcessMessage(hMsiHandle,INSTALLMESSAGE_ACTIONDATA,hRec);

		MsiCloseHandle(hRec);
	}
}

extern "C" void SetMsiReporter(LPCSTR strAction, LPCSTR strDesc,DWORD h) {
	dwReporterType = REPORT_MSI;
	hMsiHandle = h;

#ifdef DONT_NEED
    /* this is performed in the Wix installer */
	MSIHANDLE hRec = MsiCreateRecord(4);
  
    MsiRecordClearData(hRec);
	MsiRecordSetStringA(hRec,1,strAction);
	MsiRecordSetStringA(hRec,2,strDesc);
	MsiRecordSetStringA(hRec,3,"[1]:([2])([3])([4])");

	MsiProcessMessage(h,INSTALLMESSAGE_ACTIONSTART, hRec);
	
    MsiCloseHandle(hRec);
#endif
}
