//**************************************************************************
//
// Description:
//
//      Call EnableStatic method of Win32_NetworkAdapterConfiguration
//      for some network adapter GUID.
//
// Note:
//
//	The EnableStatic method is notsupported on Win9x platforms.
//
//**************************************************************************

#define _WIN32_DCOM
#include <windows.h>
#include <comdef.h>
#include <wbemidl.h>
#include <tchar.h>

#include <devguid.h>

/* These two are from the Windows DDK */
#include <netcfgx.h>
#include <netcfgn.h>

#include <shlobj.h>
//#include <objbase.h>

//#ifndef TEST
//inline void printf(char*, ...) {}
//#else
#include <stdio.h>
//#endif

#define CLEANUP_ON_FAILURE(hr) \
    do { if (!SUCCEEDED(hr)) goto cleanup; } while (0)

#define CLEANUP_ON_AND_SET(check, var, value) \
    do { if (check) { (var) = (value); goto cleanup; } } while (0)

#define ETCDIR "\\drivers\\etc"

#define EACCES (13)
#define ENOENT (2)

DWORD AdjustMaxLana(DWORD dwMaxLana);

typedef
HRESULT
(*FindNetworkAdapterConfigurationInstance_t)(
    IN  PVOID pContext,
    IN  IWbemServices *pSvc,
    OUT BSTR* pPath
    );

HRESULT
FindNetworkAdapterConfigurationInstanceByGUID(
    IN  PVOID pContext,
    IN  IWbemServices *pSvc,
    OUT BSTR* pPath
    )
{
    HRESULT hr = 0;
    BOOL bFound = FALSE;
    BSTR Language = 0;
    BSTR Query = 0;
    IEnumWbemClassObject* pEnum = 0;
    IWbemClassObject* pObj = 0;
    VARIANT Value;
    VariantInit(&Value);
    LPCWSTR adapter_guid = (LPCWSTR)pContext;

    // Check arguments
    if (!pPath || !adapter_guid || *pPath)
        return E_INVALIDARG;

    *pPath = 0;

    // Query for all network adapters
    Language = SysAllocString(L"WQL");
    Query = SysAllocString(L"select * from Win32_NetworkAdapterConfiguration");

    // Issue the query.
    hr = pSvc->ExecQuery(Language,
                         Query,
                         WBEM_FLAG_FORWARD_ONLY,         // Flags
                         0,                              // Context
                         &pEnum);
    if (!SUCCEEDED(hr))
    {
        printf("ExecQuery() error (0x%08X)\n", hr);
        goto cleanup;
    }

    // Retrieve the objects in the result set.
    while (!bFound)
    {
        ULONG uReturned = 0;

        hr = pEnum->Next(0,                  // Time out
                         1,                  // One object
                         &pObj,
                         &uReturned);
        CLEANUP_ON_FAILURE(hr);

        if (uReturned == 0)
            break;

        // Use the object.
        hr  = pObj->Get(L"SettingID", // property name
                        0L,
                        &Value,       // output to this variant
                        NULL,
                        NULL);
        CLEANUP_ON_FAILURE(hr);

        bFound = !wcscmp(adapter_guid, V_BSTR(&Value));
        
        if (bFound)
        {
            printf("Found adapter: %S\n", V_BSTR(&Value));
            VariantClear(&Value);
            hr = pObj->Get(L"__RELPATH", // property name
                           0L,
                           &Value,       // output to this variant
                           NULL,
                           NULL);
            CLEANUP_ON_FAILURE(hr);

            *pPath = SysAllocString(V_BSTR(&Value));
            
        }
        
        VariantClear(&Value);
        
        // Release it.
        // ===========
        pObj->Release();    // Release objects not owned.
        pObj = 0;
    }


    // All done.
 cleanup:
    SysFreeString(Query);
    SysFreeString(Language);
    VariantClear(&Value);
    if (pEnum)
        pEnum->Release();
    if (pObj)
        pObj->Release();

    return *pPath ? 0 : ( SUCCEEDED(hr) ? WBEM_E_NOT_FOUND : hr );
}

HRESULT
SetupStringAsSafeArray(LPCWSTR s, VARIANT* v)
{
    HRESULT hr = 0;
    BSTR b = 0;
    SAFEARRAY* array = 0;
    long index[] = {0};

    if (V_VT(v) != VT_EMPTY)
        return E_INVALIDARG;

    b = SysAllocString(s);
    CLEANUP_ON_AND_SET(!b, hr, E_OUTOFMEMORY);

    array = SafeArrayCreateVector(VT_BSTR, 0, 1);
    CLEANUP_ON_AND_SET(!array, hr, E_OUTOFMEMORY);

    hr = SafeArrayPutElement(array, index, b);
    CLEANUP_ON_FAILURE(hr);

    V_VT(v) = VT_ARRAY|VT_BSTR;
    V_ARRAY(v) = array;

 cleanup:
    if (b)
        SysFreeString(b);
    if (!SUCCEEDED(hr))
    {
        if (array)
            SafeArrayDestroy(array);
    }
    return hr;
}


HRESULT
WMIEnableStatic(
    FindNetworkAdapterConfigurationInstance_t pFindInstance,
    PVOID pContext,
    LPCWSTR ip,
    LPCWSTR mask
    )
{
    HRESULT hr = 0;

    IWbemLocator* pLocator = 0;
    IWbemServices* pNamespace = 0;
    IWbemClassObject* pClass = 0;
    IWbemClassObject* pOutInst = 0;
    IWbemClassObject* pInClass = 0;
    IWbemClassObject* pInInst = 0;

    BSTR NamespacePath = 0;
    BSTR ClassPath = 0;
    BSTR InstancePath = 0;
    BSTR MethodName = 0; // needs to be BSTR for ExecMethod()

    VARIANT v_ip_list;
    VariantInit(&v_ip_list);

    VARIANT v_mask_list;
    VariantInit(&v_mask_list);

    VARIANT v_ret_value;
    VariantInit(&v_ret_value);

    int count;

    // end of declarations & NULL initialization

    NamespacePath = SysAllocString(L"root\\cimv2");
    CLEANUP_ON_AND_SET(!NamespacePath, hr, E_OUTOFMEMORY);

    ClassPath = SysAllocString(L"Win32_NetWorkAdapterConfiguration");
    CLEANUP_ON_AND_SET(!ClassPath, hr, E_OUTOFMEMORY);

    MethodName = SysAllocString(L"EnableStatic");
    CLEANUP_ON_AND_SET(!MethodName, hr, E_OUTOFMEMORY);

    // Initialize COM and connect up to CIMOM

    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    CLEANUP_ON_FAILURE(hr);

    hr = CoInitializeSecurity(NULL, -1, NULL, NULL,
                              RPC_C_AUTHN_LEVEL_CONNECT,
                              RPC_C_IMP_LEVEL_IMPERSONATE,
                              NULL, EOAC_NONE, 0);
    CLEANUP_ON_FAILURE(hr);

    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, (LPVOID *) &pLocator);
    CLEANUP_ON_FAILURE(hr);

    hr = pLocator->ConnectServer(NamespacePath, NULL, NULL, NULL, 0,
                                 NULL, NULL, &pNamespace);
    CLEANUP_ON_FAILURE(hr);

    printf("Connected to WMI\n");

    // Set the proxy so that impersonation of the client occurs.
    hr = CoSetProxyBlanket(pNamespace,
                           RPC_C_AUTHN_WINNT,
                           RPC_C_AUTHZ_NONE,
                           NULL,
                           RPC_C_AUTHN_LEVEL_CALL,
                           RPC_C_IMP_LEVEL_IMPERSONATE,
                           NULL,
                           EOAC_NONE);
    CLEANUP_ON_FAILURE(hr);

    // Get the class object
    hr = pNamespace->GetObject(ClassPath, 0, NULL, &pClass, NULL);
    CLEANUP_ON_FAILURE(hr);

    // Get the instance
    hr = pFindInstance(pContext, pNamespace, &InstancePath);
    CLEANUP_ON_FAILURE(hr);

    printf("Found Adapter Instance: %S\n", InstancePath);

#if 0
    // Use the adapter instance index to set MAXLANA in the registry.
    {
        DWORD dwIndex;
        if (swscanf(InstancePath, L"Win32_NetworkAdapterConfiguration.Index=%u", &dwIndex)==1)
        {
            DWORD ret = 0; 
            printf("Setting MAXLANA to at least %u\n",dwIndex+1);
            ret = AdjustMaxLana(dwIndex+1);
            if (ret) printf("AdjustMaxLana returned the error code %u.\n",ret);
        }
    }
#endif

    // Get the input argument and set the property
    hr = pClass->GetMethod(MethodName, 0, &pInClass, NULL);
    CLEANUP_ON_FAILURE(hr);

    hr = pInClass->SpawnInstance(0, &pInInst);
    CLEANUP_ON_FAILURE(hr);

    // Set up parameters
    hr = SetupStringAsSafeArray(ip, &v_ip_list);
    CLEANUP_ON_FAILURE(hr);

    hr = pInInst->Put(L"IPAddress", 0, &v_ip_list, 0);
    CLEANUP_ON_FAILURE(hr);

    hr = SetupStringAsSafeArray(mask, &v_mask_list);
    CLEANUP_ON_FAILURE(hr);

    hr = pInInst->Put(L"SubNetMask", 0, &v_mask_list, 0);
    CLEANUP_ON_FAILURE(hr);

    // Sleep for a twenty seconds
    printf("Calling ExecMethod in 20 seconds...\r");
    Sleep(10000);
    printf("Calling ExecMethod in 10 seconds...\r");
    Sleep(5000);  
    printf("Calling ExecMethod in  5 seconds...\r");
    Sleep(2000);

//    printf("Skipping ExecMethod\n");
//    hr = 0;
//    goto cleanup;

    // Try up to five times, sleeping 3 seconds between tries
    for (count=0; count<5; count++)
    {
        if (count>0) printf("Trying again in 3 seconds...\n");

	Sleep(3000);
  
        printf("Calling ExecMethod NOW...          \n");     

        // Call the method

        hr = pNamespace->ExecMethod(InstancePath, MethodName, 0, NULL, pInInst,
                                  &pOutInst, NULL);   

        if (!SUCCEEDED(hr))
        {
           printf("ExecMethod failed (0x%08X)\n", hr);
           continue;
        }

        // Get the EnableStatic method return value
        hr = pOutInst->Get(L"ReturnValue", 0, &v_ret_value, 0, 0);

        if (!SUCCEEDED(hr))
        {
          printf("WARNING: Could not determine return value for EnableStatic (0x%08X)\n", hr);
          continue;
        }

        hr = V_I4(&v_ret_value);                


        if(hr != 0)
            printf("EnableStatic failed (0x%08X)\n", hr);
        else
        {
            printf("EnableStatic succeeded\n");
            break;
        }

    }



 cleanup:
    // Free up resources
    VariantClear(&v_ret_value);
    VariantClear(&v_ip_list);
    VariantClear(&v_mask_list);

    SysFreeString(NamespacePath);
    SysFreeString(ClassPath);
    SysFreeString(InstancePath);
    SysFreeString(MethodName);

    if (pClass) pClass->Release();
    if (pInInst) pInInst->Release();
    if (pInClass) pInClass->Release();
    if (pOutInst) pOutInst->Release();
    if (pLocator) pLocator->Release();
    if (pNamespace) pNamespace->Release();

    CoUninitialize();
    return hr;
}


/**********************************************************
* LoopbackBindings :  unbind all other 
*       protocols except TCP/IP, netbios, netbt. 
*/
extern "C" HRESULT LoopbackBindings (LPCWSTR loopback_guid)
{
    HRESULT			hr = 0;
    INetCfg			*pCfg = NULL;
    INetCfgLock		*pLock = NULL;
    INetCfgComponent *pAdapter = NULL;
    IEnumNetCfgComponent *pEnumComponent = NULL;
    BOOL			bLockGranted = FALSE;
    BOOL			bInitialized = FALSE;
    BOOL			bConfigChanged = FALSE;
    LPWSTR			swName = NULL;
    GUID            g;
    wchar_t         device_guid[100];
    DWORD			lenDeviceId;    
    
    printf("\nRunning LoopbackBindings()...\n");
    
    hr = CoInitializeEx( NULL, COINIT_DISABLE_OLE1DDE | COINIT_APARTMENTTHREADED ); 
    CLEANUP_ON_FAILURE(hr);
    
    hr = CoCreateInstance( CLSID_CNetCfg, NULL, CLSCTX_INPROC_SERVER, IID_INetCfg, (void**)&pCfg );
    CLEANUP_ON_FAILURE(hr);
    
    hr = pCfg->QueryInterface( IID_INetCfgLock, (void**)&pLock );
    CLEANUP_ON_FAILURE(hr);
    
    hr = pLock->AcquireWriteLock( 1000, L"AFS Configuration", NULL );
    CLEANUP_ON_FAILURE(hr);
    bLockGranted = TRUE;
    
    hr = pCfg->Initialize( NULL );
    CLEANUP_ON_FAILURE(hr);
    bInitialized = TRUE;
    
    hr = pCfg->EnumComponents( &GUID_DEVCLASS_NET, &pEnumComponent );
    CLEANUP_ON_FAILURE(hr);
    
    
    while( pEnumComponent->Next( 1, &pAdapter, NULL ) == S_OK )
    {
        pAdapter->GetDisplayName( &swName );
        pAdapter->GetInstanceGuid( &g );
        StringFromGUID2(g, device_guid, 99);
        
        if(!wcscmp( device_guid, loopback_guid )) // found loopback adapter
        {
            INetCfgComponentBindings *pBindings;
            INetCfgBindingPath *pPath;
            IEnumNetCfgBindingPath *pEnumPaths;
            INetCfgComponent *upper;
            
            wprintf(L"LoopbackBindings found: %s\n", device_guid );
            
            hr = pAdapter->QueryInterface( IID_INetCfgComponentBindings, (void**) &pBindings);
            if(hr==S_OK)
            {
                hr = pBindings->EnumBindingPaths( EBP_ABOVE, &pEnumPaths );
                if(hr==S_OK)
                {
                    while(pEnumPaths->Next( 1, &pPath, NULL ) == S_OK)
                    {
                        pPath->GetOwner( &upper );
                        
                        LPWSTR swId = NULL, swName = NULL;
                        
                        upper->GetDisplayName( &swName );
                        upper->GetId( &swId );
                        
                        wprintf(L"Looking at %s (%s)... \n",swName, swId);
                                                                        
                        {
                            wprintf(L"  Moving to the end of binding order...");
                            INetCfgComponentBindings *pBindings2;
                            hr = upper->QueryInterface( IID_INetCfgComponentBindings, (void**) &pBindings2);
                            if (hr==S_OK)
                            {
                                printf("...");
                                hr = pBindings2->MoveAfter(pPath, NULL);                                
                                pBindings2->Release();                               
                                bConfigChanged=TRUE;
                            }
                            if (hr==S_OK) printf("success\n"); else printf("failed: 0x%0lx\n",hr);                                                        
                                                                                                            
                        }                        
                        
                        if ( !_wcsicmp(swId, L"ms_netbios")  || 
                            !_wcsicmp(swId, L"ms_tcpip")    ||
                            !_wcsicmp(swId, L"ms_netbt")      )
                        {
                            if (pPath->IsEnabled()!=S_OK)
                            {
                                wprintf(L"  Enabling %s: ",swName);
                                hr = pPath->Enable(TRUE);
                                if (hr==S_OK) printf("success\n"); else printf("failed: %ld\n",hr);
                                bConfigChanged=TRUE;
                            }
                            
                            
                        }
                        else //if (!_wcsicmp(swId, L"ms_server") || (!_wcsicmp(swId, L"ms_msclient")) 
                        {
                            if (pPath->IsEnabled()==S_OK)
                            {
                                wprintf(L"  Disabling %s: ",swName);
                                hr = pPath->Enable(FALSE);
                                if (hr==S_OK) printf("success\n"); else printf("failed: %ld\n",hr);
                                bConfigChanged=TRUE;
                            }
                        }
                        
                        CoTaskMemFree( swName );
                        CoTaskMemFree( swId );
                        
                        pPath->Release();
                    }
                    pEnumPaths->Release();
                }
                pBindings->Release();
            } // hr==S_OK for QueryInterface IID_INetCfgComponentBindings
        }
        
        CoTaskMemFree( swName );
        
        pAdapter->Release();
    }
    
    pEnumComponent->Release();    
    
    hr = 0;
    
cleanup:
    
    if(bConfigChanged) pCfg->Apply();
    
    if(pAdapter) pAdapter->Release();
    
    if(bInitialized) pCfg->Uninitialize();
    if(bLockGranted) pLock->ReleaseWriteLock();
    
    if(pLock) pLock->Release();
    if(pCfg) pCfg->Release();
    
    if (hr) printf ("LoopbackBindings() is returning %u\n",hr);
    return hr;
}

        
        extern "C"
            DWORD
SetIpAddress(
    LPCWSTR guid,
    LPCWSTR ip,
    LPCWSTR mask
    )
{
    printf("\nRunning SetIpAddress()...\n");
    HRESULT hr = 0;

    hr = WMIEnableStatic(FindNetworkAdapterConfigurationInstanceByGUID,
                        (PVOID)guid, ip, mask);
    return hr;
}

/* Set MAXLANA in the registry to the specified value, unless the existing registry value is larger */
DWORD AdjustMaxLana(DWORD dwMaxLana)
{

    LONG ret = 0;
    HKEY hNetBiosParamKey = NULL;
    DWORD dwType, dwExistingMaxLana, dwSize;

    printf ("Making sure MaxLana is at least %u...\n", dwMaxLana);

    ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Services\\NetBIOS\\Parameters"), 
        0, KEY_ALL_ACCESS , &hNetBiosParamKey);
    if (ret) return ret;


    
    dwSize = 4;
    ret = RegQueryValueEx(hNetBiosParamKey, _T("MaxLana"), 0, &dwType, (LPBYTE) &dwExistingMaxLana, &dwSize);
    if ((ret) && (ret != ERROR_MORE_DATA) && (ret != ERROR_FILE_NOT_FOUND)) 
    {
        RegCloseKey(hNetBiosParamKey);
        return ret;
    }

    if ((dwType != REG_DWORD) || (ret)) dwExistingMaxLana = 0;

    printf ("  MaxLana is currently %u\n", dwExistingMaxLana);

    if (dwExistingMaxLana < dwMaxLana) 
    {
        printf ("  Changing to %u\n", dwMaxLana);
        ret = RegSetValueEx(hNetBiosParamKey, _T("MaxLana"), 0, REG_DWORD, (const BYTE*)&dwMaxLana, 4);
        if (ret) 
        {
            RegCloseKey(hNetBiosParamKey);
            return ret;
        }       

    }

    RegCloseKey(hNetBiosParamKey);
    return 0;


}

extern "C" 
BOOL UpdateHostsFile( LPCWSTR swName, LPCWSTR swIp, LPCSTR szFilename, BOOL bPre )
{
    char szIp[2048], szName[2048];
    char etcPath[MAX_PATH];
	char tempPath[MAX_PATH];
	char buffer[2048], temp[2048];
	char *str;
	HRESULT rv;
	DWORD fa,len;
	FILE *hFile, *hTemp;
	
    _snprintf(szIp, 2047, "%S", swIp);
    _snprintf(szName, 2047, "%S", swName);
    strupr(szName);
    printf("Starting UpdateHostsFile() on %s file\n",szFilename);

	rv = SHGetFolderPathA( NULL, CSIDL_SYSTEM, NULL, SHGFP_TYPE_CURRENT , etcPath );
	if(rv != S_OK) return FALSE;

	strcat( etcPath, ETCDIR );

	fa = GetFileAttributesA( etcPath );

	if(fa == INVALID_FILE_ATTRIBUTES)
	{
		// the directory doesn't exist
		// it should be there. non-existence implies more things are wrong
		printf( "Path does not exist : %s\n", etcPath );
		return FALSE;
	}

	strcpy( tempPath, etcPath );
	strcat( etcPath, "\\" );
	strcat( etcPath, szFilename );

	fa = GetFileAttributesA( etcPath );

	if(fa == INVALID_FILE_ATTRIBUTES)
	{
		printf( "No %s file found. Creating...", szFilename);

		hFile = fopen( etcPath, "w" );
		if(!hFile)
		{
			printf("FAILED : can't create %s file\nErrno is %d\n",etcPath,errno);
			return FALSE;
		}

		fprintf(hFile, "%s\t%s%s\n", szIp, szName, (bPre)?"\t#PRE":"");

		fclose( hFile );

		printf("done\n");
	}
	else // the file exists. parse and update
	{

		printf( "Updating %s file ...",szFilename );

		hFile = fopen( etcPath, "r");
		if(!hFile)
		{
			printf("FAILED : can't open %s file\nErrno is %d\n",etcPath,errno);
			return FALSE;
		}

		strcat( tempPath, szFilename );
		strcat( tempPath, ".tmp" );
		hTemp = fopen( tempPath, "w");
		if(!hTemp)
		{
			printf("FAILED : can't create temp file %s\nErrno is %d\n",tempPath,errno);
			fclose(hFile);
			return FALSE;
		}

		while(fgets( buffer, 2046, hFile))
		{
			strcpy( temp, buffer );
			strupr( temp );

            if ((strlen(temp)<1) || (*(temp+strlen(temp)-1)!='\n')) strcat(temp, "\n");

			if(!(str = strstr(temp, szName)))
			{
				fputs( buffer, hTemp );
			}
			else
			{
				// check for FOOBAFS or AFSY
				//if(str <= temp || (*(str-1) != '-' && !isspace(*(str+strlen(szName)))))
				if ( (str == temp) || (!*(str+strlen(szName))) || (!isspace(*(str-1))) || (!isspace(*(str+strlen(szName)))) )
                    fputs( buffer, hTemp );
			}
		}


		len = 2048;
		GetComputerNameA( buffer, &len );
		buffer[11] = 0;
		fprintf( hTemp, "%s\t%s%s\n", szIp, szName, (bPre)?"\t#PRE":"");

		fclose( hTemp );
		fclose( hFile );

		strcpy(buffer, etcPath);
		strcat(buffer, ".old");

        errno = 0;
		
        if ((unlink( buffer ) != 0) && (errno == EACCES))
        {
            printf("FAILED : Can't delete %s file\nErrno is %d",buffer,errno);            
            return FALSE;
            
        }
        
        if ((errno) && (errno != ENOENT)) printf("WEIRD : errno after unlink is %d...",errno);

		if(rename( etcPath, buffer) != 0)
		{
			printf("FAILED : Can't rename old %s file\nErrno is %d\n",etcPath,errno);
			return FALSE;
		}

		if(rename( tempPath, etcPath ) != 0)
		{
			printf("FAILED : Can't rename new %s file\nErrno is %d\n",tempPath,errno);
			return FALSE;
		}

		printf("done\n");
	}

	return TRUE;
}

#ifdef TEST
#if 0
int
wmain(
    int argc,
    wchar_t* argv[]
    )
{
    if (argc < 3)
    {
        printf("usage: %S ip mask\n"
               "  example: %S 10.0.0.1 255.0.0.0", argv[0], argv[0]);
        return 0;
    }

    return WMIEnableStatic(FindNetworkAdapterConfigurationInstanceByGUID,
                           L"{B4981E32-551C-4164-96B6-B8874BD2E555}",
                           argv[1], argv[2]);
}
#else
int
wmain(
    int argc,
    wchar_t* argv[]
    )
{
    if (argc < 4)
    {
        printf("usage: %S adapter_guid ip mask\n"
               "  example: %S {B4981E32-551C-4164-96B6-B8874BD2E555} "
               "10.0.0.1 255.0.0.0", argv[0], argv[0]);
        return 0;
    }

    return WMIEnableStatic(FindNetworkAdapterConfigurationInstanceByGUID,
                           argv[1], argv[2], argv[3]);
}
#endif
#endif

