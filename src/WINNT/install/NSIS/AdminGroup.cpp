#include<windows.h>
#include<string.h>
#include<stdio.h>
#include<lm.h>

#pragma comment(lib,"netapi32.lib")
#pragma comment(lib,"advapi32.lib");

#define AFSCLIENT_ADMIN_GROUPNAMEW L"AFS Client Admins"
#define AFSCLIENT_ADMIN_COMMENTW   L"AFS Client Administrators"
#define LOCAL_ADMIN_GROUP_SID      L"S-1-5-32-544"

/* LookupAliasFromRid is from Microsoft KB 157234 
 * 
 * Author: Scott Field (sfield)    02-Oct-96
 */

BOOL
LookupAliasFromRid( LPWSTR TargetComputer, DWORD Rid, LPWSTR Name,
                    PDWORD cchName )
{
    SID_IDENTIFIER_AUTHORITY sia = SECURITY_NT_AUTHORITY;
    SID_NAME_USE snu;
    PSID pSid;
    WCHAR DomainName[DNLEN+1];
    DWORD cchDomainName = DNLEN;
    BOOL bSuccess = FALSE;

    //
    // Sid is the same regardless of machine, since the well-known
    // BUILTIN domain is referenced.
    //

    if(AllocateAndInitializeSid( &sia, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 Rid, 0, 0, 0, 0, 0, 0, &pSid )) {
        bSuccess = LookupAccountSidW( TargetComputer, pSid, Name, cchName,
                                      DomainName, &cchDomainName, &snu );
        FreeSid(pSid);
    }

    return bSuccess;
}

UINT createAfsAdminGroup(void) {
    LOCALGROUP_INFO_1 gInfo;
    DWORD dwError;
    NET_API_STATUS status;

    gInfo.lgrpi1_name = AFSCLIENT_ADMIN_GROUPNAMEW;
    gInfo.lgrpi1_comment = AFSCLIENT_ADMIN_COMMENTW;
    status = NetLocalGroupAdd(NULL, 1, (LPBYTE) &gInfo, &dwError);

    return status;
}

UINT initializeAfsAdminGroup(void) {
    PSID psidAdmin = NULL;
    SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
    NET_API_STATUS status;
    LOCALGROUP_MEMBERS_INFO_0 *gmAdmins = NULL;
    DWORD dwNEntries, dwTEntries;
    WCHAR AdminGroupName[UNLEN+1];
    DWORD cchName = UNLEN;

    if (!LookupAliasFromRid( NULL, DOMAIN_ALIAS_RID_ADMINS, AdminGroupName, &cchName ))
    {
        /* if we fail, we will try the English string "Administrators" */
        wcsncpy(AdminGroupName, L"Administrators", UNLEN+1);
        AdminGroupName[UNLEN] = 0;
    }

    wprintf(L"Administrators group name is: %s\n", AdminGroupName);

    status = NetLocalGroupGetMembers(NULL, AdminGroupName, 0, (LPBYTE *) &gmAdmins, MAX_PREFERRED_LENGTH, &dwNEntries, &dwTEntries, NULL);
    if (status)
        return status;

    status = NetLocalGroupAddMembers(NULL, AFSCLIENT_ADMIN_GROUPNAMEW, 0, (LPBYTE) gmAdmins, dwNEntries);

    NetApiBufferFree( gmAdmins );

    return status;
}

UINT removeAfsAdminGroup(void) {
    NET_API_STATUS status;
    status = NetLocalGroupDel(NULL, AFSCLIENT_ADMIN_GROUPNAMEW);
    return status;
}

void showUsage(char * progname) {
    printf(
        "Usage: %s [-create | -remove]\n"
        "  -create : Create AFS Client Admins group and populate it with\n"
        "            the members of the Administrators group\n"
        "  -remove : Remove the AFS Client Admins group\n"
        , progname);
}

int main(int argc, char ** argv) {

    UINT rv = 0;

    if (argc < 2) {
        showUsage(argv[0]);
        return 1;
    }

    if(!stricmp(argv[1], "-create")) {
        rv = createAfsAdminGroup();
        if(rv) {
            if(rv != ERROR_ALIAS_EXISTS) {
            fprintf(stderr, "%s: Can't create AFS Client Admin group. NetApi error %u\n", rv);
            } else {
                /* The group already exists. (Preserved config from a
                   prior install). */
                rv = 0;
            }
        } else {
            rv = initializeAfsAdminGroup();
            if(rv)
                fprintf(stderr, "%s: Can't populate AFS Client Admin group. NetApi error %u\n", rv);
        }
    } else if(!stricmp(argv[1], "-remove")) {
        removeAfsAdminGroup();
        rv = 0;
    } else {
        showUsage(argv[0]);
        rv = 0;
    }

    return rv;
}

