/**
 * Copyright (c) 2003 Lingo Systems Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

// Modified 7/18/03 by Ben Creech for NCSU ITECS
// to add command-line parameters and turn into a non-console app.

// devcon -r install %SYSTEMROOT%\Inf\Netloop.inf *MSLOOP


// Win2k
#define _WIN32_DCOM
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <shellapi.h>
#include <wchar.h>
#include <tchar.h>

// The following two headers are from the Microsoft DDK
#include <netcfgx.h>
#include <netcfgn.h>

#include <objbase.h>
#include <setupapi.h>

#include <devguid.h>
#include <cfgmgr32.h>
#include <regstr.h>
#include <newdev.h>

#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include <wbemcli.h>     // WMI interface declarations

#include <Msi.h>
#include <Msiquery.h>

#define DEFAULT_IPADDR     L"10.99.173.207"
#define DEFAULT_SUBNETMASK L"255.255.255.254"

bool bQuiet;

//
// UpdateDriverForPlugAndPlayDevices
//
typedef BOOL (WINAPI *UpdateDriverForPlugAndPlayDevicesProto)(HWND hwndParent,
                                                         LPCTSTR hwid,
                                                         LPCTSTR FullInfPath,
                                                         DWORD InstallFlags,
                                                         PBOOL bRebootRequired OPTIONAL
                                                         );

#define UPDATEDRIVERFORPLUGANDPLAYDEVICES "UpdateDriverForPlugAndPlayDevicesA"

void display_usage();

void EasyErrorBox (int hr, WCHAR *format, ...)
{

    LPWSTR   systemMessage;
    WCHAR    buf[400];
    ULONG    offset;
    va_list  ap; 

	if (bQuiet) return;

    if(hr)
        swprintf( buf, L"Error %#lx: ", hr );
    else
        buf[0] = 0;

    offset = (ULONG) wcslen( buf );
    va_start( ap, format );
    vswprintf( buf+offset, format,ap );
    va_end( ap );
    if(hr) 
    {
        FormatMessageW( FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL,
                       hr,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPWSTR)&systemMessage,
                       0,
                       NULL );
        if(systemMessage)
        {
            offset = (ULONG) wcslen( buf );
            swprintf( buf+offset, L"\n\nPossible cause:\n\n" );
            offset = (ULONG) wcslen( buf );
            wcscat( buf+offset, systemMessage );
            LocalFree( (HLOCAL)systemMessage );
        }
        else
        {
            switch(hr)
            {
            case WBEM_E_FAILED: systemMessage = L"WBEM request failed."; break;
            case WBEM_E_TYPE_MISMATCH: systemMessage = L"WBEM type mismatch."; break;
            }
            if(systemMessage)
            {
                offset = (ULONG) wcslen( buf );
                swprintf( buf+offset, L"\n\nPossible cause:\n\n" );
                offset = (ULONG) wcslen( buf );
                wcscat( buf+offset, systemMessage );
            }
        }


        MessageBoxW( NULL, buf, L"Error", MB_ICONERROR | MB_OK );
    } 
    else 
    {
        MessageBoxW( NULL, buf, L"loopback_install", MB_ICONINFORMATION | MB_OK );
    }
}

// RSM4: Converted this to stdcall so NSIS System::Call can call it (It defaults to stdcall)
DWORD _stdcall loopback_isInstalled()
{
    TCHAR * hwid = _T("*MSLOOP");
    HDEVINFO DeviceInfoSet;
    SP_DEVINFO_DATA DeviceInfoData;
    DWORD i,err;
    bool found;
    
    //
    // Create a Device Information Set with all present devices.
    //
    DeviceInfoSet = SetupDiGetClassDevs(NULL, 0, 0, DIGCF_ALLCLASSES | DIGCF_PRESENT ); // All devices present on system
    if (DeviceInfoSet == INVALID_HANDLE_VALUE)
    {
        EasyErrorBox(GetLastError(), L"GetClassDevs(All Present Devices) failed\n");
        return false; // nothing installed?
    }
    
    //
    //  Enumerate through all Devices.
    //
    found = FALSE;
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    for (i=0; SetupDiEnumDeviceInfo(DeviceInfoSet,i,&DeviceInfoData); i++)
    {
        DWORD DataT;
        TCHAR  *p, *buffer = NULL;
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
                // What the ... ?
                EasyErrorBox(GetLastError(), L"Failed to detect Loopback adapter: GetDeviceRegistryProperty() returned an unknown error.");
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


// RSM4: Added 
bool disable_loopback()
{
    TCHAR * hwid = _T("*MSLOOP");
    HDEVINFO DeviceInfoSet;
    SP_DEVINFO_DATA DeviceInfoData;
    SP_PROPCHANGE_PARAMS PropChangeParams = {sizeof(SP_CLASSINSTALL_HEADER)};
    DWORD i,err;
    bool found,status=FALSE;
    
    //
    // Create a Device Information Set with all present devices.
    //
    DeviceInfoSet = SetupDiGetClassDevs(NULL, 0, 0, DIGCF_ALLCLASSES | DIGCF_PRESENT ); // All devices present on system
    if (DeviceInfoSet == INVALID_HANDLE_VALUE)
    {
        EasyErrorBox(GetLastError(), L"GetClassDevs(All Present Devices) failed\n");
        return false; // nothing installed?
    }
    
    //
    //  Enumerate through all Devices.
    //
    found = FALSE;
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    for (i=0; SetupDiEnumDeviceInfo(DeviceInfoSet,i,&DeviceInfoData); i++)
    {
        DWORD DataT;
        TCHAR * p, *buffer = NULL;
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
                // What the ... ?
                EasyErrorBox(GetLastError(), L"Failed to detect Loopback adapter: GetDeviceRegistryProperty() returned an unknown error.");
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
    
    // If we found the device, disable it...
    if (found)
    {
       //
       // Set the PropChangeParams structure.
       //
       PropChangeParams.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
       PropChangeParams.Scope = DICS_FLAG_GLOBAL;
       PropChangeParams.StateChange = DICS_DISABLE; 

       if (SetupDiSetClassInstallParams(DeviceInfoSet,
          &DeviceInfoData,
          (SP_CLASSINSTALL_HEADER *)&PropChangeParams,
          sizeof(PropChangeParams)))
          {
             //
             // Call the ClassInstaller and perform the change.
             //
             if (SetupDiCallClassInstaller(DIF_PROPERTYCHANGE,
                DeviceInfoSet,
             &DeviceInfoData))
                   status=TRUE;
             else
                EasyErrorBox(GetLastError(), L"Could not disable LoopBack adapter: SetupDiSetClassInstallParams failed");
          }
      else
          EasyErrorBox(GetLastError(), L"Could not disable LoopBack adapter: SetupDiSetClassInstallParams failed");

    }
    
    
    //  Cleanup.
cleanup_DeviceInfo:
    err = GetLastError();
    SetupDiDestroyDeviceInfoList(DeviceInfoSet);
    SetLastError(err);
    
    return status;
}


bool loopback_install(int *rebootNeeded)
{
    SP_DEVINFO_DATA DeviceInfoData;
    GUID ClassGUID;
    HDEVINFO DeviceInfoSet = INVALID_HANDLE_VALUE;
    TCHAR ClassName[MAX_CLASS_NAME_LEN];
    TCHAR hwIdList[LINE_LEN+4];
    TCHAR InfPath[MAX_PATH];
    bool success = false;
    TCHAR * hwid = _T("*MSLOOP");
    TCHAR * inf = _T("INF\\NETLOOP.INF");
    DWORD flags = 0;
    HMODULE newdevMod = NULL;
    UpdateDriverForPlugAndPlayDevicesProto UpdateFn;
    
    TCHAR *systemRoot = _tgetenv(_T("SYSTEMROOT"));
    SetCurrentDirectory(systemRoot);

    // Inf must be a full pathname
    if(GetFullPathName(inf,MAX_PATH,InfPath,NULL) >= MAX_PATH) {
        puts("Failed to configure Loopback adapter: inf pathname too long");
        return false;
    }

    // List of hardware ID's must be double zero-terminated
    ZeroMemory(hwIdList,sizeof(hwIdList));
    lstrcpyn(hwIdList,hwid,LINE_LEN);

    // Use the INF File to extract the Class GUID.
    if (!SetupDiGetINFClass(InfPath,&ClassGUID,ClassName,sizeof(ClassName),0))
    {
        EasyErrorBox(GetLastError(), L"Failed to configure Loopback adapter: Failed to read INF for %s\n", InfPath);
        goto final;
    }

    //
    // Create the container for the to-be-created Device Information Element.
    //
    DeviceInfoSet = SetupDiCreateDeviceInfoList(&ClassGUID,0);
    if(DeviceInfoSet == INVALID_HANDLE_VALUE)
    {
        EasyErrorBox(GetLastError(), L"Failed to configure Loopback adapter: Failed to create device info list for %s\n", ClassName);
        goto final;
    }

    //
    // Now create the element.
    // Use the Class GUID and Name from the INF file.
    //
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    if (!SetupDiCreateDeviceInfo(DeviceInfoSet, ClassName, &ClassGUID, NULL, 0, DICD_GENERATE_ID, &DeviceInfoData))
        goto final;

    //
    // Add the hwid to the Device's hwid property.
    //
    if(!SetupDiSetDeviceRegistryProperty(DeviceInfoSet, &DeviceInfoData, SPDRP_HARDWAREID, (LPBYTE)hwIdList, (lstrlen(hwIdList)+1+1)*sizeof(TCHAR)))
        goto final;

    //
    // Transform the registry element into an actual devnode
    // in the PnP HW tree.
    //
    if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, DeviceInfoSet, &DeviceInfoData))
    {
        EasyErrorBox(GetLastError(), L"Failed to configure Loopback adapter: Failed to call class installer for %s\n", inf);
        goto final;
    }

    inf = InfPath;
    flags |= INSTALLFLAG_FORCE;

    // make use of UpdateDriverForPlugAndPlayDevices
    newdevMod = LoadLibrary(TEXT("newdev.dll"));
    if(!newdevMod)
    {
        EasyErrorBox(GetLastError(), L"Failed to configure Loopback adapter: Failed to load newdev.dll\n", inf);
        goto final;
    }

    UpdateFn = (UpdateDriverForPlugAndPlayDevicesProto)GetProcAddress(newdevMod,UPDATEDRIVERFORPLUGANDPLAYDEVICES);
    if(!UpdateFn)
    {
        EasyErrorBox(GetLastError(), L"Failed to configure Loopback adapter: Failed to read the driver updating function from newdev.dll\n", inf);
        goto final;
    }

    if(!UpdateFn(NULL,hwid,inf,flags,rebootNeeded))
    {
        EasyErrorBox(GetLastError(), L"Failed to configure Loopback adapter: Failed to update the driver for %s\n", inf);
        goto final;
    }

    success = true;

final:

    if(newdevMod) {
        FreeLibrary(newdevMod);
    }

    if (DeviceInfoSet != INVALID_HANDLE_VALUE) {
        SetupDiDestroyDeviceInfoList(DeviceInfoSet);
    }

    return success;
}

//+---------------------------------------------------------------------------
//    getWriteLock [in]  whether to get write lock
//    ppnc          [in]  pointer to pointer to INetCfg object
//
// Returns:   S_OK on success, otherwise an error code
HRESULT getInetCfg(bool getWriteLock, WCHAR *appName, INetCfg** ppnc, WCHAR **holdingAppName)
{
    HRESULT hr=S_OK;

    // Initialize the output parameters.
    *ppnc = NULL;

    // Create the object implementing INetCfg.
    //
    INetCfg* pnc;
    hr = CoCreateInstance(CLSID_CNetCfg, NULL, CLSCTX_INPROC_SERVER,
                          IID_INetCfg, (void**)&pnc);
    if (SUCCEEDED(hr))
    {
        INetCfgLock * pncLock = NULL;
        if (getWriteLock)
        {
            // Get the locking interface
            hr = pnc->QueryInterface(IID_INetCfgLock,
                                     (LPVOID *)&pncLock);
            if (SUCCEEDED(hr))
            {
                // Attempt to lock the INetCfg for read/write
                static const ULONG c_cmsTimeout = 15000;

                hr = pncLock->AcquireWriteLock(c_cmsTimeout,
                                               appName,
                                               holdingAppName);
                if (S_FALSE == hr)
                {
                    hr = NETCFG_E_NO_WRITE_LOCK;
                    EasyErrorBox(hr, L"Failed to configure Loopback adapter: Could not lock INetcfg, it is already locked by '%s'", *holdingAppName);
                }
            }
        }

        if (SUCCEEDED(hr))
        {
            // Initialize the INetCfg object.
            //
            hr = pnc->Initialize(NULL);
            if (SUCCEEDED(hr))
            {
                *ppnc = pnc;
                pnc->AddRef();
            }
            else
            {
                // initialize failed, if obtained lock, release it
                if (pncLock)
                {
                    pncLock->ReleaseWriteLock();
                }
            }
        }
        if(pncLock) pncLock->Release();
        if(pnc) pnc->Release();
    }


    return hr;
}

//+---------------------------------------------------------------------------
//    hasWriteLock [in]  whether write lock needs to be released.
//    pnc           [in]  pointer to INetCfg object

HRESULT releaseInetCfg(INetCfg* pnc, bool hasWriteLock)
{
    HRESULT hr = S_OK;

    // uninitialize INetCfg
    hr = pnc->Uninitialize();

    // if write lock is present, unlock it
    if (SUCCEEDED(hr) && hasWriteLock)
    {
        INetCfgLock* pncLock;

        // Get the locking interface
        hr = pnc->QueryInterface(IID_INetCfgLock,
                                 (LPVOID *)&pncLock);
        if (SUCCEEDED(hr))
        {
            hr = pncLock->ReleaseWriteLock();
            if(pncLock) pncLock->Release();
        }
    }

    if(pnc) pnc->Release();

    return hr;
}

bool ChangeBinding(WCHAR *inf, WCHAR *binding, bool bind)
{

    INetCfg                   *pnc;
    INetCfgComponent          *pncc;
    INetCfgComponentBindings  *pnccb;
    INetCfgComponent          *pnccToChange;
    WCHAR *                   lpszApp;
    HRESULT                   hr;
    bool                      fChange=false;


    hr = getInetCfg(TRUE, L"loopback_install", &pnc, &lpszApp);
    if(hr == S_OK) 
    {
        // Get a reference to the network component.
        hr = pnc->FindComponent(inf, &pncc);
        if(hr == S_OK) 
        {
            // Get a reference to the component's binding.
            hr = pncc->QueryInterface(IID_INetCfgComponentBindings, (PVOID *)&pnccb);
            if(hr == S_OK) 
            {
                // Get a reference to the selected component.
                hr = pnc->FindComponent(binding, &pnccToChange);
                if(hr == S_OK) 
                {
                    if(bind) 
                    {
                        // Bind the component to the selected component.
                        hr = pnccb->BindTo(pnccToChange);
                        fChange = (fChange || hr == S_OK);

                        if(hr != S_OK) EasyErrorBox(hr, L"Failed to configure Loopback adapter: %s couldn't be bound to %s.", inf, binding);
                    }
                    else 
                    {
                        // Unbind the component from the selected component.
                        hr = pnccb->UnbindFrom(pnccToChange);
                        fChange = (fChange || hr == S_OK);

                        if(hr != S_OK) EasyErrorBox(hr, L"Failed to configure Loopback adapter: %s couldn't be unbound from %s.", inf, binding);
                        //else EasyErrorBox(hr, L"%s will be unbound from %s.", inf, binding);
                    }

                    pnccToChange->Release();
                } 
                else 
                {
                    if(bind) // Don't have to unbind something thats not installed, so only print an error if we're binding it
                        EasyErrorBox(GetLastError(), L"Failed to configure Loopback adapter: Couldn't get an interface pointer to %s. %s will not be bound to it. (Maybe this is not installed on your system.)", binding, inf);
                }

                pnccb->Release();
            }
            else 
            {
                EasyErrorBox(hr, L"Failed to configure Loopback adapter: Couldn't get a binding interface of %s.", inf);
            }

            pncc->Release();
        }
        else 
        {
            EasyErrorBox(hr, L"Couldn't get an interface pointer to %s.", inf);
        }

        //
        // If one or more network components have been bound/unbound,
        // apply the changes.
        //

        if(fChange) 
        {
            hr = pnc->Apply();

            fChange = hr == S_OK;
        }

        releaseInetCfg(pnc, true);
    }
    else 
    {
        if((hr == NETCFG_E_NO_WRITE_LOCK) && lpszApp) 
        {
            EasyErrorBox(hr, L"%s currently holds the lock, try later.", lpszApp);
            CoTaskMemFree(lpszApp);
        }
        else 
        {
            EasyErrorBox(hr, L"Couldn't get the notify object interface.");
        }
    }

    return fChange;
}

// Unbind microsoft services so our NetBIOS will be functional:
// ms_msclient will be unbound from *msloop
// ms_server will be unbound from *MSLOOP
int loopback_unbindmsnet()
{
    // Unbind microsoft's NetBIOS hogs
    // Whats interesting is that CIFS shares on that device still work
    // even when the client for microsoft networks is not bound to that
    // device
    ChangeBinding(L"ms_msclient", L"*MSLOOP", false);
    ChangeBinding(L"ms_server", L"*MSLOOP", false);

    // Bind TCP/IP
    ChangeBinding(L"ms_tcpip", L"*MSLOOP", true);
    return 1;
}

//
// Debugging function to help us print variant records
// This is copied from some microsoft sample
//
#define BLOCKSIZE (32 * sizeof(WCHAR))
#define CVTBUFSIZE (309+40) /* # of digits in max. dp value + slop  (this size stolen from cvt.h in c runtime library) */
LPWSTR ValueToString(VARIANT *pValue, WCHAR **pbuf)
{
   DWORD iNeed = 0;
   DWORD iVSize = 0;
   DWORD iCurBufSize = 0;

   WCHAR *vbuf = NULL;
   WCHAR *buf = NULL;


   switch (pValue->vt) 
   {

   case VT_NULL: 
         buf = (WCHAR *)malloc(BLOCKSIZE);
         wcscpy(buf, L"<null>");
         break;

   case VT_BOOL: {
         VARIANT_BOOL b = pValue->boolVal;
         buf = (WCHAR *)malloc(BLOCKSIZE);

         if (!b) {
            wcscpy(buf, L"FALSE");
         } else {
            wcscpy(buf, L"TRUE");
         }
         break;
      }

   case VT_UI1: {
         BYTE b = pValue->bVal;
	      buf = (WCHAR *)malloc(BLOCKSIZE);
         if (b >= 32) {
            swprintf(buf, L"'%c' (%d, 0x%X)", b, b, b);
         } else {
            swprintf(buf, L"%d (0x%X)", b, b);
         }
         break;
      }

   case VT_I2: {
         SHORT i = pValue->iVal;
         buf = (WCHAR *)malloc(BLOCKSIZE);
         swprintf(buf, L"%d (0x%X)", i, i);
         break;
      }

   case VT_I4: {
         LONG l = pValue->lVal;
         buf = (WCHAR *)malloc(BLOCKSIZE);
         swprintf(buf, L"%d (0x%X)", l, l);
         break;
      }

   case VT_R4: {
         float f = pValue->fltVal;
         buf = (WCHAR *)malloc(CVTBUFSIZE * sizeof(WCHAR));
         swprintf(buf, L"%10.4f", f);
         break;
      }

   case VT_R8: {
         double d = pValue->dblVal;
         buf = (WCHAR *)malloc(CVTBUFSIZE * sizeof(WCHAR));
         swprintf(buf, L"%10.4f", d);
         break;
      }

   case VT_BSTR: {
		 LPWSTR pWStr = pValue->bstrVal;
		 buf = (WCHAR *)malloc((wcslen(pWStr) * sizeof(WCHAR)) + sizeof(WCHAR) + (2 * sizeof(WCHAR)));
	     swprintf(buf, L"\"%wS\"", pWStr);
		 break;
		}

	// the sample GUI is too simple to make it necessary to display
	// these 'complicated' types--so ignore them.
   case VT_DISPATCH:  // Currently only used for embedded objects
   case VT_BOOL|VT_ARRAY: 
   case VT_UI1|VT_ARRAY: 
   case VT_I2|VT_ARRAY: 
   case VT_I4|VT_ARRAY: 
   case VT_R4|VT_ARRAY: 
   case VT_R8|VT_ARRAY: 
   case VT_BSTR|VT_ARRAY: 
   case VT_DISPATCH | VT_ARRAY: 
         break;

   default:
         buf = (WCHAR *)malloc(BLOCKSIZE);
         wcscpy(buf, L"<conversion error>");

   }

   *pbuf = buf;   
   return buf;
}

void VariantInitStringArray(VARIANT &var, BSTR str)
{
    DWORD hr;
    BSTR *arrayContents;
    var.vt = VT_BSTR|VT_ARRAY;
    var.parray = SafeArrayCreateVector(VT_BSTR, 0, 1);
    if((hr = SafeArrayAccessData(var.parray, (void **)&arrayContents))) { EasyErrorBox (0, L"Failed to access contents"); exit(1); }
    arrayContents[0] = str;
}


WCHAR *EnableStatic_strerror(int err)
{
    switch(err)
    {
    case 0:   return L"Successful completion, no reboot required.";
    case 1:   return L"Successful completion, reboot required.";
    case 64:  return L"Method not supported on this platform.";
    case 65:  return L"Unknown failure.";
    case 66:  return L"Invalid subnet mask.";
    case 67:  return L"An error occurred while processing an instance that was returned.";
    case 68:  return L"Invalid input parameter.";
    case 69:  return L"More than five gateways specified.";
    case 70:  return L"Invalid IP address.";
    case 71:  return L"Invalid gateway IP address.";
    case 72:  return L"An error occurred while accessing the registry for the requested information.";
    case 73:  return L"Invalid domain name.";
    case 74:  return L"Invalid host name.";
    case 75:  return L"No primary or secondary WINS server defined.";
    case 76:  return L"Invalid file.";
    case 77:  return L"Invalid system path.";
    case 78:  return L"File copy failed.";
    case 79:  return L"Invalid security parameter.";
    case 80:  return L"Unable to configure TCP/IP service.";
    case 81:  return L"Unable to configure DHCP service.";
    case 82:  return L"Unable to renew DHCP lease.";
    case 83:  return L"Unable to release DHCP lease.";
    case 84:  return L"IP not enabled on adapter.";
    case 85:  return L"IPX not enabled on adapter.";
    case 86:  return L"Frame/network number bounds error.";
    case 87:  return L"Invalid frame type.";
    case 88:  return L"Invalid network number.";
    case 89:  return L"Duplicate network number.";
    case 90:  return L"Parameter out of bounds.";
    case 91:  return L"Access denied.";
    case 92:  return L"Out of memory.";
    case 93:  return L"Already exists.";
    case 94:  return L"Path, file, or object not found.";
    case 95:  return L"Unable to notify service.";
    case 96:  return L"Unable to notify DNS service.";
    case 97:  return L"Interface not configurable.";
    case 98:  return L"Not all DHCP leases could be released or renewed.";
    case 100: return L"DHCP not enabled on adapter.";
    default:  return L"See online for the description of this error";
    }
}

// Change the loopback to use a static IP: 127.0.0.2
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/wmisdk/wmi/retrieving_class_or_instance_data.asp
// GetObject()
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/wmisdk/wmi/iwbemservices_getobject.asp
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/wmisdk/wmi/win32_networkadapter.asp
// EnableStatic()
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/wmisdk/wmi/enablestatic_method_in_class_win32_networkadapterconfiguration.asp
// #define loopback_adapter_path "Win32_NetworkAdapter"
int loopback_configure(int *needReboot, WCHAR *loopbackIP, WCHAR *loopbackNetmask)
{
	HRESULT  hRes;
	BSTR propName = NULL, val = NULL;
	VARIANT pVal;
	ULONG uReturned;
    int result=1;

	IEnumWbemClassObject *pEnumServices = NULL;
	IWbemLocator *pIWbemLocator = NULL;
	HRESULT hr = S_OK;

	//------------------------
    // Create an instance of the WbemLocator interface.
    if(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &pIWbemLocator) == S_OK)
    {
		//------------------------
		// Use the pointer returned in step two to connect to
		//     the server using the passed in namespace.
        // It is very important never to pass any string not allocated using SysAllocString to any COM function
		BSTR pNamespace = SysAllocString(L"\\\\.\\root\\cimv2");
        IWbemServices *pIWbemServices = NULL;

		if((hr = pIWbemLocator->ConnectServer(pNamespace, NULL, NULL, 0L, 0L, NULL, NULL, &pIWbemServices)) == S_OK) 
		{	
			CoSetProxyBlanket(pIWbemServices,
				RPC_C_AUTHN_WINNT,
				RPC_C_AUTHZ_NONE,
				NULL,
				RPC_C_AUTHN_LEVEL_CALL,
				RPC_C_IMP_LEVEL_IMPERSONATE,
				NULL,
				EOAC_NONE
			);

            BSTR className = SysAllocString(L"Win32_NetworkAdapterConfiguration");
            // Lets lookup the class first
            IWbemClassObject *pClass = NULL;
            if((hRes = pIWbemServices->GetObject(className, 0, NULL, &pClass, NULL)) == S_OK)
            {
                BSTR methodName = SysAllocString(L"EnableStatic");


                /** // Uncomment this debugging code to check the qualifiers of your EnableStatic method
                IWbemQualifierSet *quals=NULL;
                hr = pClass->GetMethodQualifierSet(methodName, &quals);
                if(hr == WBEM_S_NO_ERROR)
                {
                    BSTR qualName, qualValue;

                    quals->BeginEnumeration(0);
                    while(quals->Next(0, &qualName, &pVal, NULL) == S_OK)
                    {
                        printf("qualifier: %ws = %ws\n", qualName, ValueToString(&pVal, &qualValue));
                    }
                    quals->Release();
                }
                */

                IWbemClassObject * pInClass = NULL;
                hr = pClass->GetMethod(methodName, 0, &pInClass, NULL);
                if(hr == WBEM_S_NO_ERROR)
                {
                    // WBEM_E_ACCESS_DENIED
	                //----------------------
	                // execute the query.
	                BSTR qLang = SysAllocString(L"WQL");
                    BSTR query = SysAllocString(L"select * from Win32_NetworkAdapterConfiguration where ServiceName = \"msloop\"");
	                if((hRes = pIWbemServices->ExecQuery(qLang, query, 0L, NULL, &pEnumServices)) == S_OK)
	                {
            	        IWbemClassObject *pInstance = NULL;

                        //----------------------
		                // Only configure the first one; if there are multiple loopback adapters they are up to something
                        // and we'd rather not interfere (or this tool mistakenly installed a second one)
		                if(((hRes = pEnumServices->Next(5000, 1, &pInstance, &uReturned)) == S_OK) && (uReturned == 1))
		                {
                            // Lookup the "index" (the primary key for Win32_NetworkAdapterConfiguration)
                            BSTR varName = SysAllocString(L"Index");
                            if(S_OK == pInstance->Get(varName, 0, &pVal, NULL, NULL))
                            {
                                // Create an instance of the input parameters of this method
                                IWbemClassObject * pInInst = NULL;                            
                                hr = pInClass->SpawnInstance(0, &pInInst);
                                if(hr == WBEM_S_NO_ERROR)
                                {
                                    // Now fill that instance in with useful parameters.
                                    BSTR ipAddressStr = SysAllocString(L"IPAddress");
                                    BSTR subnetMaskStr = SysAllocString(L"SubnetMask");
                                    BSTR loopbackIPCopy = SysAllocString(loopbackIP);
                                    BSTR loopbackNetMaskCopy = SysAllocString(loopbackNetmask);

                                    // Two string arrays: one with the ip addresses, the other with subnet masks
                                    VARIANT ipVar, maskVar;
                                    
                                    // Create the IP Address array and put our ip address into it
                                    VariantInitStringArray(ipVar, loopbackIPCopy);

                                    // Create the subnet mask array and put our subnet mask into it
                                    VariantInitStringArray(maskVar, loopbackNetMaskCopy);
                                    
                                    // Write the parameters into the params object
                                    hr = pInInst->Put(ipAddressStr, 0, &ipVar, 0);    if(hr) EasyErrorBox(hr, L"Args->Put(IPAddress) failed\n");
                                    hr = pInInst->Put(subnetMaskStr, 0, &maskVar, 0); if(hr) EasyErrorBox(hr, L"Args->Put(SubnetMask) failed\n");

                                    /** // Uncomment this debugging code to view the parameters before the method is called
                                    BSTR pInText=NULL;
                                    pInInst->GetObjectText(0, &pInText);
                                    printf("Parameters: %ws\n", pInText);
                                    */

                                    // Call the method
                                    IWbemClassObject * pOutInst = NULL;
                                    
                                    // Compute the object path and copy it into a COM string
                                    WCHAR objectPathBuf[255];
                                    wsprintfW(objectPathBuf, L"Win32_NetworkAdapterConfiguration.Index=%d", pVal.intVal);
                                    BSTR objectPath = SysAllocString(objectPathBuf);

                                    // Go go go!
                                    hr = pIWbemServices->ExecMethod(objectPath, methodName, 0, NULL, pInInst, &pOutInst, NULL);
                                    if(hr == WBEM_S_NO_ERROR)
                                    {
                                        // Extract the return value (from the field called "ReturnValue")
                                        VARIANT retVal;
                                        BSTR retValName = SysAllocString(L"ReturnValue");
                                        hr = pOutInst->Get(retValName, 0, &retVal, 0, 0);
                                        if(hr == WBEM_S_NO_ERROR)
                                        {
                                            // If it returns 1, we should reboot
                                            if(retVal.intVal == 1)
                                            {
                                                *needReboot = 1;
                                            }
                                            else if(retVal.intVal == 0)
                                            {
                                                // Everything is great
                                            }
                                            else
                                            {
												if (retVal.intVal == 81) { // "Unable to configure DHCP"; seems to be returned superfluously sometimes
                                                    // Just print it out without bugging the user; that way admins can see it but they can also ignore it
													EasyErrorBox (0, L"Failed to configure Loopback adapter: EnableStatic() returns %d: %s\n", retVal.intVal, EnableStatic_strerror(retVal.intVal));
													result = 0;
												} else {
                                                    // For descriptions of these error codes try:
                                                    // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/wmisdk/wmi/enablestatic_method_in_class_win32_networkadapterconfiguration.asp
													EasyErrorBox(hr, L"Failed to configure Loopback adapter: EnableStatic() returns %d (%s)", retVal.intVal, EnableStatic_strerror(retVal.intVal));
													result = 0;
												}
                                            }
                                        }
                                        else
                                        {
                                            // This shouldn't happen, methinks
                                            BSTR objText=NULL;
                                            pOutInst->GetObjectText(0, &objText);
                                            EasyErrorBox(hr, L"Successfully called EnableStatic(); result = %ws but unable to get ReturnValue\n", objText);
											result = 0;
                                        }
                                    }
                                    else
                                    {
                                        // After all that work, we still couldn't execute the method?  Probably a BSTR was allocated using SysAllocString but thats just my prophesy
                                        EasyErrorBox(hr, L"Failed to configure Loopback adapter: ExecMethod() failed\n");
										result = 0;
                                    }

                                    if(pOutInst)
                                    {
                                        pOutInst->Release();
                                        pOutInst = NULL;
                                    }

                                    // Release some resources; this function isn't likely to be part of a long running
                                    // program, but I've seen stranger things.
                                    SafeArrayDestroy(ipVar.parray);
                                    SafeArrayDestroy(maskVar.parray);
                                    SysFreeString(ipAddressStr);
                                    SysFreeString(subnetMaskStr);
                                    SysFreeString(loopbackNetMaskCopy);
                                    SysFreeString(loopbackIPCopy);
                                }
                                else
                                {
                                    EasyErrorBox(hr, L"Failed to configure Loopback adapter: Failed to Spawn an instance of the method's parameters\n");
									result = 0;
                                }

                                if(pInInst)
                                {
                                    pInInst->Release();
                                    pInInst = NULL;
                                }
                            }
                            else
                            {
                                EasyErrorBox(hr, L"Failed to configure Loopback adapter: Unable to read ServiceName\n");
								result = 0;
                            }

			                // done with the ClassObject
			                if (pInstance)
			                { 
				                pInstance->Release(); 
				                pInstance = NULL;
			                }

                            SysFreeString(varName);

		                }

		                // did the while loop exit due to an error?
		                if((hRes != S_OK) && 
		                   (hRes != 1))
		                {
			                EasyErrorBox(hRes, L"Failed to configure Loopback adapter: pEnumServices->Next() failed %s\n");
							result = 0;
		                }

		                if (pEnumServices)
		                { 
			                pEnumServices->Release(); 
			                pEnumServices = NULL;
		                }
	                }
	                else
	                {
    		            EasyErrorBox(hRes, L"Failed to configure Loopback adapter: ExecQuery('%s', '%s') failed\n", qLang, query);
						result = 0;
	                }

	                SysFreeString(qLang);
	                SysFreeString(query);
                }
                else
                {
                    EasyErrorBox(hRes, L"Failed to configure Loopback adapter: GetMethod(\"EnableStatic\") failed\n");
					result = 0;
                }

                SysFreeString(methodName);
            }
            else
            {
                EasyErrorBox(hRes, L"Failed to configure Loopback adapter: GetObject(\"Win32_NetworkAdapterConfiguration\") failed\n");
				result = 0;
            }

            SysFreeString(className);
        }

        SysFreeString(pNamespace);
    }
    else
    {
        EasyErrorBox(GetLastError(), L"Failed to configure Loopback adapter: CoCreateInstance(CLSID_WbemLocator) failed.\n");
        result = 0;
    } 

   return result;
}

class ARGS {
public:
	bool bQuiet;
	LPWSTR lpIPAddr;
	LPWSTR lpSubnetMask;
	ARGS () : bQuiet (0), lpIPAddr (0), lpSubnetMask (0) { }
	~ARGS () {
		if (lpIPAddr) free (lpIPAddr);
		if (lpSubnetMask) free (lpSubnetMask);
	}
};

void wcsMallocAndCpy (LPWSTR * dst, const LPWSTR src) {
	*dst = (LPWSTR) malloc ((wcslen (src) + 1) * sizeof (WCHAR));
	wcscpy (*dst, src);
}

int process_args (LPWSTR lpCmdLine, ARGS & args) {
	int i, iNumArgs;
	LPWSTR * argvW;

	argvW = CommandLineToArgvW (lpCmdLine, &iNumArgs);
	for (i = 0; i < iNumArgs; i++)
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

	if (!args.lpIPAddr)
		wcsMallocAndCpy (&args.lpIPAddr, DEFAULT_IPADDR);
	if (!args.lpSubnetMask)
		wcsMallocAndCpy (&args.lpSubnetMask, DEFAULT_SUBNETMASK);

	GlobalFree (argvW);
	return 1;
}

void display_usage() {
	EasyErrorBox (0,
		L"Installation utility for the MS Loopback Adapter\r\n\r\n"
		L"Usage:\r\n"
		L"RunDll32 loopback_install.dll doLoopBackEntry [q|quiet] [IP address] [Subnet Mask]\r\n"
		L"  \"Quiet\" supresses error messages\r\n"
		L"  \"IP address\" defaults to %s\r\n"
		L"  \"Subnet Mask\" defaults to %s\r\n",
		DEFAULT_IPADDR, DEFAULT_SUBNETMASK);
}

int doLoopBack (LPWSTR lpIPAddr, LPWSTR lpSubnetMask) {
	int rc;
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

    int rebootNeeded = 0;
    
    if(loopback_isInstalled() || loopback_install(&rebootNeeded))
    {
        rc = loopback_unbindmsnet();
		if (!rc) return 1;
        rc = loopback_configure(&rebootNeeded, lpIPAddr, lpSubnetMask);
		if (!rc) return 1;
    }

	CoUninitialize();

	if(rebootNeeded) {
        EasyErrorBox(0, L"Please reboot.\n");
        return 2;
    }
    return 0;
}

void CALLBACK doLoopBackEntryW (HWND hwnd, HINSTANCE hinst, LPWSTR lpCmdLine, int nCmdShow)
{
	ARGS args;

	if (!process_args(lpCmdLine, args)) return;
	bQuiet = args.bQuiet; // laziness

	doLoopBack (args.lpIPAddr, args.lpSubnetMask);
}

void CALLBACK disableLoopBackEntryW (HWND hwnd, HINSTANCE hinst, LPWSTR lpCmdLine, int nCmdSHow)
{
   ARGS args;
	int rc;

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

   disable_loopback();

	CoUninitialize();
   
   
}

UINT __stdcall installLoopbackMSI (MSIHANDLE hInstall)
{
	LPWSTR szValueBuf;
	DWORD cbValueBuf = 256;
	ARGS args;
	UINT rc;

	szValueBuf = (LPWSTR) malloc (cbValueBuf * sizeof (WCHAR));
	while (rc = MsiGetPropertyW(hInstall, L"CustomActionData", szValueBuf, &cbValueBuf)) {
		free (szValueBuf);
		if (rc == ERROR_MORE_DATA) {
			cbValueBuf++;
			szValueBuf = (LPWSTR) malloc (cbValueBuf * sizeof (WCHAR));
		} else return ERROR_INSTALL_FAILURE;
	}

	if (!process_args(szValueBuf, args)) return ERROR_INSTALL_FAILURE;
		
	rc = doLoopBack (args.lpIPAddr, args.lpSubnetMask);

	if (rc == 1) return ERROR_INSTALL_FAILURE;

	if (rc == 2) {
		MsiDoActionW (hInstall, L"ScheduleReboot");
	}

	return ERROR_SUCCESS;
}
