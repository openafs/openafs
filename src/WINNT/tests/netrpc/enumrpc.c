#ifndef UNICODE
#define UNICODE
#endif

#include <stdio.h>
#include <windows.h>
#include <lm.h>

void
CallNetWkstaGetInfo(LPTSTR pszServerName)
{
    DWORD dwLevel = 102;
    LPWKSTA_INFO_102 pBuf = NULL;
    NET_API_STATUS nStatus;

    //
    // Call the NetWkstaGetInfo function, specifying level 102.
    //
    nStatus = NetWkstaGetInfo( pszServerName,
                               dwLevel,
                               (LPBYTE *)&pBuf);
    //
    // If the call is successful,
    //  print the workstation data.
    //
    wprintf(L"== NetWkstaGetInfo( %s) Reports ==\n", pszServerName);
    if (nStatus == NERR_Success)
    {
        printf("\tPlatform: %d\n", pBuf->wki102_platform_id);
        wprintf(L"\tName:     %s\n", pBuf->wki102_computername);
        printf("\tVersion:  %d.%d\n", pBuf->wki102_ver_major,
                pBuf->wki102_ver_minor);
        wprintf(L"\tDomain:   %s\n", pBuf->wki102_langroup);
        wprintf(L"\tLan Root: %s\n", pBuf->wki102_lanroot);
        wprintf(L"\t# Logged On Users: %d\n", pBuf->wki102_logged_on_users);
    }
    //
    // Otherwise, indicate the system error.
    //
    else
        fprintf(stderr, "A system error has occurred: %d\n", nStatus);
    printf("\n\n");

    //
    // Free the allocated memory.
    //
    if (pBuf != NULL)
        NetApiBufferFree(pBuf);
}

void
CallNetServerGetInfo(LPTSTR pszServerName)
{
    DWORD dwLevel = 101;
    LPSERVER_INFO_101 pBuf = NULL;
    NET_API_STATUS nStatus;

    //
    // Call the NetServerGetInfo function, specifying level 101.
    //
    nStatus = NetServerGetInfo(pszServerName,
                                dwLevel,
                                (LPBYTE *)&pBuf);
    wprintf(L"== NetServerGetInfo( %s) Reports ==\n", pszServerName);
    //
    // If the call succeeds,
    //
    if (nStatus == NERR_Success)
    {
        //
        // Check for the type of server.
        //
        if ((pBuf->sv101_type & SV_TYPE_DOMAIN_CTRL) ||
             (pBuf->sv101_type & SV_TYPE_DOMAIN_BAKCTRL) ||
             (pBuf->sv101_type & SV_TYPE_SERVER_NT))
            printf("\tThis is a server\n");
        else
            printf("\tThis is a workstation\n");
    }
    //
    // Otherwise, print the system error.
    //
    else
        fprintf(stderr, "A system error has occurred: %d\n", nStatus);

    printf("\n\n");

    //
    // Free the allocated memory.
    //
    if (pBuf != NULL)
        NetApiBufferFree(pBuf);

}

void
CallNetShareGetInfo(LPTSTR pszServerName, LPTSTR pszShare)
{
    PSHARE_INFO_2 BufPtr;
    NET_API_STATUS res;

    //
    // Call the NetShareGetInfo function, specifying level 502.
    //
    res = NetShareGetInfo (pszServerName,pszShare,2,(LPBYTE *) &BufPtr);

    wprintf(L"== NetShareGetInfo( %s, %s) Reports ==\n", pszServerName, pszShare);
    if (res == ERROR_SUCCESS)
    {
        //
        // Print the retrieved data.
        //
        printf("%S\t%S\t%S\n",BufPtr->shi2_netname, BufPtr->shi2_path, BufPtr->shi2_remark);
        //
        // Free the allocated memory.
        //
        NetApiBufferFree(BufPtr);
    }
    else
        printf("Error: %ld\n",res);
    printf("\n");
}

void
CallNetShareEnum(LPTSTR pszServerName)
{
    PSHARE_INFO_2 BufPtr,p;
    NET_API_STATUS res;
    DWORD er=0,tr=0,resume=0, i;

    //
    // Print a report header.
    //

    wprintf(L"== NetShareEnum( %s) Reports ==\n", pszServerName);
    printf("Share:              Local Path:                   Uses:   Descriptor:\n");
    printf("---------------------------------------------------------------------\n");
    //
    // Call the NetShareEnum function; specify level 2.
    //
    do // begin do
    {
        res = NetShareEnum (pszServerName, 2, (LPBYTE *) &BufPtr, -1, &er, &tr, &resume);
        //
        // If the call succeeds,
        //
        if(res == ERROR_SUCCESS || res == ERROR_MORE_DATA)
        {
            p=BufPtr;
            //
            // Loop through the entries;
            //  print retrieved data.
            //
            for(i=1;i<=er;i++)
            {
                printf("%-20S%-30S%-8u  %S\n",p->shi2_netname, p->shi2_path, p->shi2_current_uses, p->shi2_remark);
                p++;
            }
            printf("\n");

            //
            // Now call Net
            p=BufPtr;
            //
            // Loop through the entries;
            //  print retrieved data.
            //
            for(i=1;i<=er;i++)
            {
                CallNetShareGetInfo( pszServerName, p->shi2_netname);
                p++;
            }
            printf("\n");

            //
            // Free the allocated buffer.
            //
            NetApiBufferFree(BufPtr);
        }
        else
            printf("Error: %ld\n",res);
    }
    // Continue to call NetShareEnum while
    //  there are more entries.
    //
    while (res==ERROR_MORE_DATA); // end do

}

int wmain(int argc, wchar_t *argv[])
{
    LPTSTR pszServerName = NULL;
    //
    // Check command line arguments.
    //
    if (argc != 2)
    {
        fwprintf(stderr, L"Usage: %s [\\\\ServerName]\n", argv[0]);
        exit(1);
    }

    pszServerName = argv[1];

    CallNetWkstaGetInfo( pszServerName);
    CallNetServerGetInfo( pszServerName);
    CallNetShareEnum( pszServerName);

    return 0;
}
