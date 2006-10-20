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

/* $Id */

#include<windows.h>
#include<aclapi.h>
#include<sddl.h>
#include<stdio.h>
#include<string.h>

#define SETDACL     1
#define RESETDACL   2

#define AFSSERVICE                  "TransarcAFSDaemon"
#define AFSCLIENT_ADMIN_GROUPNAME   "AFS Client Admins"
#define EVERYONE_GROUPNAME          "Everyone"

char * progname = NULL;

void show_usage(void) {
    fprintf(stderr,
        "%s : Set or reset the DACL to allow starting or stopping\n"
        "     the afsd service by any ordinary user.\n"
        "\n"
        "Usage : %s [-set | -reset] [-show]\n"
        "      -set   : Sets the DACL\n"
        "      -reset : Reset the DACL\n"
        "      -show  : Show current DACL (SDSF)\n"
        , progname, progname);
}

void show_last_error(DWORD code) {
   LPVOID lpvMessageBuffer;

   if(!code)
        code = GetLastError();

   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                 FORMAT_MESSAGE_FROM_SYSTEM,
                 NULL, code,
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&lpvMessageBuffer, 0, NULL);

   fprintf(stderr,
       "%s: Error %d : %s\n",
       progname,
       code,
       (LPSTR) lpvMessageBuffer);

   LocalFree(lpvMessageBuffer);
}

int set_dacl(int action) {
    int             rv = 1;
    BOOL            bDaclPresent = FALSE;
    BOOL            bDaclDefaulted = FALSE;
    SC_HANDLE       scm = NULL;
    SC_HANDLE       s_afs = NULL;
    PSECURITY_DESCRIPTOR psdesc = NULL;
    PACL            pacl = NULL;
    PACL            pnewacl = NULL;
    EXPLICIT_ACCESS exa[2];
    DWORD           dwSize = 0;
    DWORD           code = ERROR_SUCCESS;
    SECURITY_DESCRIPTOR sd;

    scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if(!scm) {
        show_last_error(0);
        goto exit0;
    }

    s_afs = OpenService(scm, AFSSERVICE, READ_CONTROL | WRITE_DAC);
    if(!s_afs) {
        show_last_error(0);
        goto exit0;
    }

    if (!QueryServiceObjectSecurity(s_afs, DACL_SECURITY_INFORMATION, 
        &sd, 0, &dwSize))
    {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            psdesc = (PSECURITY_DESCRIPTOR)HeapAlloc(GetProcessHeap(),
                HEAP_ZERO_MEMORY, dwSize);
            if (psdesc == NULL)
            {
                show_last_error(0);
                goto exit0;
            }

            if (!QueryServiceObjectSecurity(s_afs,
                DACL_SECURITY_INFORMATION, psdesc, dwSize, &dwSize)) {
                show_last_error(0);
                goto exit0;
                }
        }
        else {
            show_last_error(0);
            goto exit0;
        }
    }
    /* else : shouldn't happen. */

    if (!GetSecurityDescriptorDacl(psdesc, &bDaclPresent, &pacl, &bDaclDefaulted))
        show_last_error(0);

    BuildExplicitAccessWithName(&exa[0], AFSCLIENT_ADMIN_GROUPNAME,
        SPECIFIC_RIGHTS_ALL | STANDARD_RIGHTS_ALL, 
        SET_ACCESS,
        NO_INHERITANCE);

    BuildExplicitAccessWithName(&exa[1], EVERYONE_GROUPNAME,
        SERVICE_START | SERVICE_STOP | READ_CONTROL,
        ((action==RESETDACL)?REVOKE_ACCESS:SET_ACCESS),
        NO_INHERITANCE);

    code = SetEntriesInAcl(2, exa, pacl, &pnewacl);
    if(code != ERROR_SUCCESS) {
        show_last_error(code);
    }

    if(!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION))
        show_last_error(0);

    if(!SetSecurityDescriptorDacl(&sd, TRUE, pnewacl, FALSE))
        show_last_error(0);

    if (!SetServiceObjectSecurity(s_afs, DACL_SECURITY_INFORMATION, &sd))
        show_last_error(0);

exit0:
    if(pnewacl)
        LocalFree(pnewacl);
    if(psdesc)
        HeapFree(GetProcessHeap(), 0, psdesc);
    if(s_afs)
        CloseServiceHandle(s_afs);
    if(scm)
        CloseServiceHandle(scm);

    return rv;
}

int show_dacl(void) {
    int             rv = 1;
    BOOL            bDaclPresent = FALSE;
    BOOL            bDaclDefaulted = FALSE;
    SC_HANDLE       scm = NULL;
    SC_HANDLE       s_afs = NULL;
    PSECURITY_DESCRIPTOR psdesc = NULL;
    DWORD           dwSize = 0;
    DWORD           code = ERROR_SUCCESS;
    SECURITY_DESCRIPTOR sd;
    LPSTR           pstr = NULL;

    scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if(!scm) {
        show_last_error(0);
        goto exit0;
    }

    s_afs = OpenService(scm, AFSSERVICE, READ_CONTROL);
    if(!s_afs) {
        show_last_error(0);
        goto exit0;
    }

    if (!QueryServiceObjectSecurity(s_afs, DACL_SECURITY_INFORMATION,
        &sd, 0, &dwSize))
    {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            psdesc = (PSECURITY_DESCRIPTOR)HeapAlloc(GetProcessHeap(),
                HEAP_ZERO_MEMORY, dwSize);
            if (psdesc == NULL)
            {
                show_last_error(0);
                goto exit0;
            }

            if (!QueryServiceObjectSecurity(s_afs,
                DACL_SECURITY_INFORMATION, psdesc, dwSize, &dwSize)) {
                show_last_error(0);
                goto exit0;
                }
        }
        else {
            show_last_error(0);
            goto exit0;
        }
    }
    /* else : shouldn't happen. */

    if(!ConvertSecurityDescriptorToStringSecurityDescriptor(
        psdesc,
        SDDL_REVISION_1,
        DACL_SECURITY_INFORMATION,
        &pstr,
        NULL)) 
    {
        show_last_error(0);
        goto exit0;
    }

    printf("DACL for AFSD service is : [%s]\n",pstr);

exit0:
    if(pstr)
        LocalFree(pstr);
    if(psdesc)
        HeapFree(GetProcessHeap(), 0, psdesc);
    if(s_afs)
        CloseServiceHandle(s_afs);
    if(scm)
        CloseServiceHandle(scm);

    return rv;
}

int main(int argc, char ** argv) {
    int showdacl = FALSE;
    int action = 0;
    int i;
    int rv;

    progname = argv[0];

    for(i=1; i<argc; i++) {
        if(!strcmp(argv[i],"-set") && !action)
            action = SETDACL;
        else if(!strcmp(argv[i], "-reset") && !action)
            action = RESETDACL;
        else if(!strcmp(argv[i], "-show"))
            showdacl = TRUE;
        else {
            show_usage();
            return 1;
        }
    }

    if(!showdacl && action == 0) {
        show_usage();
        return 1;
    }

    if(action) {
        rv = set_dacl(action);
    }
    
    if(showdacl) {
        rv = show_dacl();
    }

    return rv;
}
