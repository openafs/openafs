/*
 * This test code was obtained from
 * http://msdn.microsoft.com/en-us/library/aa385341(VS.85).aspx
 *
 * No license specified.
 */

#pragma comment(lib, "mpr.lib")

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <stdio.h>
#include <winnetwk.h>

BOOL WINAPI EnumerateFunc(DWORD dwScope, LPNETRESOURCE lpnr);
void DisplayStruct(int i, LPNETRESOURCE lpnrLocal);
BOOL WINAPI NetErrorHandler(HWND hwnd, DWORD dwErrorCode, LPSTR lpszFunction);

int main()
{

    LPNETRESOURCE lpnr = NULL;

    printf("Connected Resources\n");
    printf("-------------------\n");
    if (EnumerateFunc(RESOURCE_CONNECTED, lpnr) == FALSE) {
        printf("Call to EnumerateFunc(CONNECTED) failed\n");
        return 1;
    }

    printf("\n");
    printf("Context Resources\n");
    printf("-----------------\n");
    if (EnumerateFunc(RESOURCE_CONTEXT, lpnr) == FALSE) {
        printf("Call to EnumerateFunc(CONTEXT) failed\n");
        return 1;
    }

    printf("\n");
    printf("Global Resources\n");
    printf("----------------\n");
    if (EnumerateFunc(RESOURCE_GLOBALNET, lpnr) == FALSE) {
        printf("Call to EnumerateFunc(GLOBALNET) failed\n");
        return 1;
    }

    printf("\n");
    printf("Remembered Resources\n");
    printf("--------------------\n");
    if (EnumerateFunc(RESOURCE_REMEMBERED, lpnr) == FALSE) {
        printf("Call to EnumerateFunc(REMEMBERED) failed\n");
        return 1;
    }

    return 0;
}

BOOL WINAPI EnumerateFunc(DWORD dwScope, LPNETRESOURCE lpnr)
{
    DWORD dwResult, dwResultEnum;
    HANDLE hEnum;
    DWORD cbBuffer = 16384;     // 16K is a good size
    DWORD cEntries = -1;        // enumerate all possible entries
    LPNETRESOURCE lpnrLocal;    // pointer to enumerated structures
    DWORD i;
    //
    // Call the WNetOpenEnum function to begin the enumeration.
    //
    dwResult = WNetOpenEnum(dwScope, // all network resources
                            RESOURCETYPE_DISK,   // all resources
                            RESOURCEUSAGE_ALL,  // enumerate all resources
                            lpnr,       // NULL first time the function is called
                            &hEnum);    // handle to the resource

    if (dwResult != NO_ERROR) {
        printf("WnetOpenEnum failed with error %d\n", dwResult);
        return FALSE;
    }
    //
    // Call the GlobalAlloc function to allocate resources.
    //
    lpnrLocal = (LPNETRESOURCE) GlobalAlloc(GPTR, cbBuffer);
    if (lpnrLocal == NULL) {
        printf("WnetOpenEnum failed with error %d\n", dwResult);
        return FALSE;
    }

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
        dwResultEnum = WNetEnumResource(hEnum,  // resource handle
                                        &cEntries,      // defined locally as -1
                                        lpnrLocal,      // LPNETRESOURCE
                                        &cbBuffer);     // buffer size
        //
        // If the call succeeds, loop through the structures.
        //
        if (dwResultEnum == NO_ERROR) {
            for (i = 0; i < cEntries; i++) {
                // Call an application-defined function to
                //  display the contents of the NETRESOURCE structures.
                //
                DisplayStruct(i, &lpnrLocal[i]);

                // If the NETRESOURCE structure represents a container resource,
                //  call the EnumerateFunc function recursively.

                if (dwScope == RESOURCE_GLOBALNET &&
                     RESOURCEUSAGE_CONTAINER == (lpnrLocal[i].dwUsage
                                                & RESOURCEUSAGE_CONTAINER))
                    if (!EnumerateFunc(dwScope, &lpnrLocal[i]))
                        printf("EnumerateFunc returned FALSE\n");
            }
        }
        // Process errors.
        //
        else if (dwResultEnum != ERROR_NO_MORE_ITEMS) {
            printf("WNetEnumResource failed with error %d\n", dwResultEnum);
            break;
        }
    }
    //
    // End do.
    //
    while (dwResultEnum != ERROR_NO_MORE_ITEMS);
    //
    // Call the GlobalFree function to free the memory.
    //
    GlobalFree((HGLOBAL) lpnrLocal);
    //
    // Call WNetCloseEnum to end the enumeration.
    //
    dwResult = WNetCloseEnum(hEnum);

    if (dwResult != NO_ERROR) {
        //
        // Process errors.
        //
        printf("WNetCloseEnum failed with error %d\n\n", dwResult);
//    NetErrorHandler(hwnd, dwResult, (LPSTR)"WNetCloseEnum");
        return FALSE;
    }

    return TRUE;
}

void DisplayStruct(int i, LPNETRESOURCE lpnrLocal)
{
    printf("NETRESOURCE[%d] Scope: ", i);
    switch (lpnrLocal->dwScope) {
    case (RESOURCE_CONNECTED):
        printf("connected\n");
        break;
    case (RESOURCE_GLOBALNET):
        printf("all resources\n");
        break;
    case (RESOURCE_REMEMBERED):
        printf("remembered\n");
        break;
    case RESOURCE_RECENT:
        printf("recent\n");
        break;
    case RESOURCE_CONTEXT:
        printf("context\n");
        break;
    default:
        printf("unknown scope %d\n", lpnrLocal->dwScope);
        break;
    }

    printf("NETRESOURCE[%d] Type: ", i);
    switch (lpnrLocal->dwType) {
    case (RESOURCETYPE_ANY):
        printf("any\n");
        break;
    case (RESOURCETYPE_DISK):
        printf("disk\n");
        break;
    case (RESOURCETYPE_PRINT):
        printf("print\n");
        break;
    case RESOURCETYPE_RESERVED:
        printf("reserved\n");
        break;
    default:
        printf("unknown type %d\n", lpnrLocal->dwType);
        break;
    }

    printf("NETRESOURCE[%d] DisplayType: ", i);
    switch (lpnrLocal->dwDisplayType) {
    case (RESOURCEDISPLAYTYPE_GENERIC):
        printf("generic\n");
        break;
    case (RESOURCEDISPLAYTYPE_DOMAIN):
        printf("domain\n");
        break;
    case (RESOURCEDISPLAYTYPE_SERVER):
        printf("server\n");
        break;
    case (RESOURCEDISPLAYTYPE_SHARE):
        printf("share\n");
        break;
    case (RESOURCEDISPLAYTYPE_FILE):
        printf("file\n");
        break;
    case (RESOURCEDISPLAYTYPE_GROUP):
        printf("group\n");
        break;
    case (RESOURCEDISPLAYTYPE_NETWORK):
        printf("network\n");
        break;
    case RESOURCEDISPLAYTYPE_ROOT:
        printf("root\n");
        break;
    case RESOURCEDISPLAYTYPE_SHAREADMIN:
        printf("share-admin\n");
        break;
    case RESOURCEDISPLAYTYPE_DIRECTORY:
        printf("directory\n");
        break;
    case RESOURCEDISPLAYTYPE_TREE:
        printf("tree\n");
        break;
    case RESOURCEDISPLAYTYPE_NDSCONTAINER:
        printf("nds-container\n");
        break;
    default:
        printf("unknown display type %d\n", lpnrLocal->dwDisplayType);
        break;
    }

    printf("NETRESOURCE[%d] Usage: 0x%x = ", i, lpnrLocal->dwUsage);
    if (lpnrLocal->dwUsage & RESOURCEUSAGE_CONNECTABLE)
        printf("connectable ");
    if (lpnrLocal->dwUsage & RESOURCEUSAGE_CONTAINER)
        printf("container ");
    if (lpnrLocal->dwUsage & RESOURCEUSAGE_NOLOCALDEVICE)
        printf("no-local ");
    if (lpnrLocal->dwUsage & RESOURCEUSAGE_SIBLING)
        printf("sibling ");
    if (lpnrLocal->dwUsage & RESOURCEUSAGE_ATTACHED)
        printf("attached ");
    if (lpnrLocal->dwUsage & RESOURCEUSAGE_RESERVED)
        printf("reserved ");
    printf("\n");

    printf("NETRESOURCE[%d] Localname: %S\n", i, lpnrLocal->lpLocalName);
    printf("NETRESOURCE[%d] Remotename: %S\n", i, lpnrLocal->lpRemoteName);
    printf("NETRESOURCE[%d] Comment: %S\n", i, lpnrLocal->lpComment);
    printf("NETRESOURCE[%d] Provider: %S\n", i, lpnrLocal->lpProvider);
    printf("\n");
}


/*
BOOL WINAPI NetErrorHandler(HWND hwnd,
                            DWORD dwErrorCode,
                            LPSTR lpszFunction)
{
    DWORD dwWNetResult, dwLastError;
    CHAR szError[256];
    CHAR szCaption[256];
    CHAR szDescription[256];
    CHAR szProvider[256];

    // The following code performs standard error-handling.

    if (dwErrorCode != ERROR_EXTENDED_ERROR)
    {
        sprintf_s((LPSTR) szError, sizeof(szError), "%s failed; \nResult is %ld",
            lpszFunction, dwErrorCode);
        sprintf_s((LPSTR) szCaption, sizeof(szCaption), "%s error", lpszFunction);
        MessageBox(hwnd, (LPSTR) szError, (LPSTR) szCaption, MB_OK);
        return TRUE;
    }

    // The following code performs error-handling when the
    //  ERROR_EXTENDED_ERROR return value indicates that the
    //  WNetGetLastError function can retrieve additional information.

    else
    {
        dwWNetResult = WNetGetLastError(&dwLastError, // error code
            (LPSTR) szDescription,  // buffer for error description
            sizeof(szDescription),  // size of error buffer
            (LPSTR) szProvider,     // buffer for provider name
            sizeof(szProvider));    // size of name buffer

        //
        // Process errors.
        //
        if(dwWNetResult != NO_ERROR) {
            sprintf_s((LPSTR) szError, sizeof(szError),
                "WNetGetLastError failed; error %ld", dwWNetResult);
            MessageBox(hwnd, (LPSTR) szError,
                "WNetGetLastError", MB_OK);
            return FALSE;
        }

        //
        // Otherwise, print the additional error information.
        //
        sprintf_((LPSTR) szError, sizeof(szError),
            "%s failed with code %ld;\n%s",
            (LPSTR) szProvider, dwLastError, (LPSTR) szDescription);
        sprintf_s((LPSTR) szCaption, sizeof(szCaption), "%s error", lpszFunction);
        MessageBox(hwnd, (LPSTR) szError, (LPSTR) szCaption, MB_OK);
        return TRUE;
    }
}
*/
