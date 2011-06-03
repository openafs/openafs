/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>

#include <afs/stds.h>
#include <afs/afs_consts.h>

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <winioctl.h>
#include <winsock2.h>
#include <nb30.h>

#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <strsafe.h>

#include <osi.h>
#include <afsd.h>
#include <smb.h>
#include <afs/cmd.h>
#include <fs_utils.h>
#include <WINNT\afsreg.h>

long
fs_ExtractDriveLetter(char *inPathp, char *outPathp)
{
    if (inPathp[0] != 0 && inPathp[1] == ':') {
        /* there is a drive letter */
        *outPathp++ = *inPathp++;
        *outPathp++ = *inPathp++;
        *outPathp++ = 0;
    }
    else
        *outPathp = 0;

    return 0;
}

/* strip the drive letter from a component */
long
fs_StripDriveLetter(char *inPathp, char *outPathp, size_t outSize)
{
    char tempBuffer[1000];

    if( FAILED(StringCbCopy(tempBuffer, sizeof(tempBuffer), inPathp))) {
        fprintf (stderr, "fs_StripDriveLetter - not enough space on input");
        exit(1);
    }
    if (tempBuffer[0] != 0 && tempBuffer[1] == ':') {
        /* drive letter present */
        if( FAILED(StringCbCopy(outPathp, outSize, tempBuffer+2))) {
            fprintf (stderr, "fs_StripDriveLetter - not enough space on output");
            exit(1);
        }
    }
    else {
        /* no drive letter present */
        if( FAILED(StringCbCopy(outPathp, outSize, tempBuffer))) {
            fprintf (stderr, "fs_StripDriveLetter - not enough space on output");
            exit(1);
        }
    }
    return 0;
}

/* take a path with a drive letter, possibly relative, and return a full path
 * without the drive letter.  This is the full path relative to the working
 * dir for that drive letter.  The input and output paths can be the same.
 */
long
fs_GetFullPath(char *pathp, char *outPathp, size_t outSize)
{
    char tpath[1000];
    char origPath[1000];
    char *firstp;
    long code;
    int pathHasDrive;
    int doSwitch;
    char newPath[3];

    if (pathp[0] != 0 && pathp[1] == ':') {
        /* there's a drive letter there */
        firstp = pathp+2;
        pathHasDrive = 1;
    } else {
        firstp = pathp;
        pathHasDrive = 0;
    }

    if (*firstp == '\\' || *firstp == '/') {
        /* already an absolute pathname, just copy it back */
        if( FAILED(StringCbCopy(outPathp, outSize, firstp))) {
            fprintf (stderr, "fs_GetFullPath - not enough space on output");
            exit(1);
        }
        return 0;
    }

    GetCurrentDirectoryA(sizeof(origPath), origPath);

    doSwitch = 0;
    if (pathHasDrive && (*pathp & ~0x20) != (origPath[0] & ~0x20)) {
        /* a drive has been specified and it isn't our current drive.
         * to get path, switch to it first.  Must case-fold drive letters
         * for user convenience.
         */
        doSwitch = 1;
        newPath[0] = *pathp;
        newPath[1] = ':';
        newPath[2] = 0;
        if (!SetCurrentDirectoryA(newPath)) {
            code = GetLastError();
            return code;
        }
    }

    /* now get the absolute path to the current wdir in this drive */
    GetCurrentDirectoryA(sizeof(tpath), tpath);
    if( FAILED(StringCbCopy(outPathp, outSize, tpath+2))) {
        fprintf (stderr, "fs_GetFullPath - not enough space on output");
        exit(1);
    }
    /* if there is a non-null name after the drive, append it */
    if (*firstp != 0) {
        if( FAILED(StringCbCat(outPathp, outSize, "\\"))) {
            fprintf (stderr, "fs_GetFullPath - not enough space on output");
            exit(1);
        }
        if( FAILED(StringCbCat(outPathp, outSize, firstp))) {
            fprintf (stderr, "fs_GetFullPath - not enough space on output");
            exit(1);
        }
    }

    /* finally, if necessary, switch back to our home drive letter */
    if (doSwitch) {
        SetCurrentDirectoryA(origPath);
    }

    return 0;
}

/* is this a digit or a digit-like thing? */
static int ismeta(int abase, int ac) {
/*    if (ac == '-' || ac == 'x' || ac == 'X') return 1; */
    if (ac >= '0' && ac <= '7') return 1;
    if (abase <= 8) return 0;
    if (ac >= '8' && ac <= '9') return 1;
    if (abase <= 10) return 0;
    if (ac >= 'a' && ac <= 'f') return 1;
    if (ac >= 'A' && ac <= 'F') return 1;
    return 0;
}

/* given that this is a digit or a digit-like thing, compute its value */
static int getmeta(int ac) {
    if (ac >= '0' && ac <= '9') return ac - '0';
    if (ac >= 'a' && ac <= 'f') return ac - 'a' + 10;
    if (ac >= 'A' && ac <= 'F') return ac - 'A' + 10;
    return 0;
}

char *cm_mount_root="afs";
char *cm_slash_mount_root="/afs";
char *cm_back_slash_mount_root="\\afs";

void
fs_utils_InitMountRoot()
{
    HKEY parmKey;
    char mountRoot[MAX_PATH+1];
    char *pmount=mountRoot;
    DWORD len=sizeof(mountRoot)-1;

    if ((RegOpenKeyExA(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY, 0,
                      (IsWow64()?KEY_WOW64_64KEY:0)|KEY_QUERY_VALUE, &parmKey)!= ERROR_SUCCESS)
        || (RegQueryValueExA(parmKey, "Mountroot", NULL, NULL,(LPBYTE)(mountRoot), &len)!= ERROR_SUCCESS)
         || (len==sizeof(mountRoot)-1)
         )
    {
        if( FAILED(StringCbCopy(mountRoot, sizeof(mountRoot), "\\afs"))) {
            fprintf (stderr, "fs_InitMountRoot - not enough space on output");
            exit(1);
        }
    }
    RegCloseKey(parmKey);
    mountRoot[len]=0;       /*safety see ms-help://MS.MSDNQTR.2002OCT.1033/sysinfo/base/regqueryvalueex.htm*/

    cm_mount_root=malloc(len+1);
    cm_slash_mount_root=malloc(len+2);
    cm_back_slash_mount_root=malloc(len+2);
    if ((*pmount=='/') || (*pmount='\\'))
        pmount++;

    if( FAILED(StringCbCopy(cm_mount_root, len+1, pmount))) {
        fprintf (stderr, "fs_InitMountRoot - not enough space on output");
        exit(1);
    }
    cm_slash_mount_root[0]='/';
    if( FAILED(StringCbCopy(cm_slash_mount_root+1, len+1, pmount))) {
        fprintf (stderr, "fs_InitMountRoot - not enough space on output");
        exit(1);
    }
    cm_back_slash_mount_root[0]='\\';
    if( FAILED(StringCbCopy(cm_back_slash_mount_root+1, len+1, pmount))) {
        fprintf (stderr, "fs_InitMountRoot - not enough space on output");
        exit(1);
    }
}

/* return a static pointer to a buffer */
char *
fs_GetParent(char *apath)
{
    static char tspace[1024];
    char *tp;

    if( FAILED(StringCbCopy(tspace, sizeof(tspace), apath))) {
        fprintf (stderr, "tspace - not enough space");
        exit(1);
    }
    tp = strrchr(tspace, '\\');
    if (tp) {
        if (tp - tspace > 2 &&
            tspace[1] == ':' &&
            &tspace[2] == tp)
	    *(tp+1) = 0;	/* lv trailing slash so Parent("k:\foo") is "k:\" not "k:" */
        else
            *tp = 0;
    }
    else {
	fs_ExtractDriveLetter(apath, tspace);
	if( FAILED(StringCbCat(tspace, sizeof(tspace), "."))) {
	    fprintf (stderr, "tspace - not enough space");
	    exit(1);
	}
    }
    return tspace;
}

/* this function returns TRUE (1) if the file is in AFS, otherwise false (0) */
int
fs_InAFS(char *apath)
{
    struct ViceIoctl blob;
    cm_ioctlQueryOptions_t options;
    cm_fid_t fid;
    afs_int32 code;

    memset(&options, 0, sizeof(options));
    options.size = sizeof(options);
    options.field_flags |= CM_IOCTL_QOPTS_FIELD_LITERAL;
    options.literal = 1;
    blob.in_size = options.size;    /* no variable length data */
    blob.in = &options;
    blob.out_size = sizeof(cm_fid_t);
    blob.out = (char *) &fid;

    code = pioctl_utf8(apath, VIOCGETFID, &blob, 1);
    if (code) {
	if ((errno == EINVAL) || (errno == ENOENT))
            return 0;
    }
    return 1;
}

int
fs_IsFreelanceRoot(char *apath)
{
    struct ViceIoctl blob;
    afs_int32 code;
    char space[AFS_PIOCTL_MAXSIZE];

    blob.in_size = 0;
    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.out = space;

    code = pioctl_utf8(apath, VIOC_FILE_CELL_NAME, &blob, 1);
    if (code == 0) {
        return !cm_strnicmp_utf8N("Freelance.Local.Root",space, blob.out_size);
    }
    return 1;   /* assume it is because it is more restrictive that way */
}

const char *
fs_NetbiosName(void)
{
    static char buffer[NETBIOSNAMESZ] = "AFS";
    HKEY  parmKey;
    DWORD code;
    DWORD dummyLen;
    DWORD enabled = 0;

    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                         0, (IsWow64()?KEY_WOW64_64KEY:0)|KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        dummyLen = sizeof(buffer);
        code = RegQueryValueEx(parmKey, "NetbiosName", NULL, NULL,
			       buffer, &dummyLen);
        RegCloseKey (parmKey);
    } else {
	if( FAILED(StringCbCopy(buffer, sizeof(buffer), "AFS"))) {
	    fprintf (stderr, "buffer - not enough space");
            exit(1);
	}
    }
    return buffer;
}

#define AFSCLIENT_ADMIN_GROUPNAME "AFS Client Admins"

BOOL
fs_IsAdmin (void)
{
    static BOOL fAdmin = FALSE;
    static BOOL fTested = FALSE;

    if (!fTested)
    {
        /* Obtain the SID for the AFS client admin group.  If the group does
         * not exist, then assume we have AFS client admin privileges.
         */
        PSID psidAdmin = NULL;
        DWORD dwSize, dwSize2;
        char pszAdminGroup[ MAX_COMPUTERNAME_LENGTH + sizeof(AFSCLIENT_ADMIN_GROUPNAME) + 2 ];
        char *pszRefDomain = NULL;
        SID_NAME_USE snu = SidTypeGroup;

        dwSize = sizeof(pszAdminGroup);

        if (!GetComputerName(pszAdminGroup, &dwSize)) {
            /* Can't get computer name.  We return false in this case.
               Retain fAdmin and fTested. This shouldn't happen.*/
            return FALSE;
        }

        dwSize = 0;
        dwSize2 = 0;

	if( FAILED(StringCbCat(pszAdminGroup, MAX_COMPUTERNAME_LENGTH + sizeof(AFSCLIENT_ADMIN_GROUPNAME) + 2,"\\"))) {
	    fprintf (stderr, "pszAdminGroup - not enough space");
            exit(1);
	}
	if( FAILED(StringCbCat(pszAdminGroup, MAX_COMPUTERNAME_LENGTH +
	    sizeof(AFSCLIENT_ADMIN_GROUPNAME) + 2, AFSCLIENT_ADMIN_GROUPNAME))) {
	    fprintf (stderr, "pszAdminGroup - not enough space");
            exit(1);
	}

        LookupAccountName(NULL, pszAdminGroup, NULL, &dwSize, NULL, &dwSize2, &snu);
        /* that should always fail. */

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            /* if we can't find the group, then we allow the operation */
            fAdmin = TRUE;
            return TRUE;
        }

        if (dwSize == 0 || dwSize2 == 0) {
            /* Paranoia */
            fAdmin = TRUE;
            return TRUE;
        }

        psidAdmin = (PSID)malloc(dwSize); memset(psidAdmin,0,dwSize);
        assert(psidAdmin);
        pszRefDomain = (char *)malloc(dwSize2);
        assert(pszRefDomain);

        if (!LookupAccountName(NULL, pszAdminGroup, psidAdmin, &dwSize, pszRefDomain, &dwSize2, &snu)) {
            /* We can't lookup the group now even though we looked it up earlier.
               Could this happen? */
            fAdmin = TRUE;
        } else {
            /* Then open our current ProcessToken */
            HANDLE hToken;

            if (OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &hToken))
            {

                if (!CheckTokenMembership(hToken, psidAdmin, &fAdmin)) {
                    /* We'll have to allocate a chunk of memory to store the list of
                     * groups to which this user belongs; find out how much memory
                     * we'll need.
                     */
                    DWORD dwSize = 0;
                    PTOKEN_GROUPS pGroups;

                    GetTokenInformation (hToken, TokenGroups, NULL, dwSize, &dwSize);

                    pGroups = (PTOKEN_GROUPS)malloc(dwSize);
                    assert(pGroups);

                    /* Allocate that buffer, and read in the list of groups. */
                    if (GetTokenInformation (hToken, TokenGroups, pGroups, dwSize, &dwSize))
                    {
                        /* Look through the list of group SIDs and see if any of them
                         * matches the AFS Client Admin group SID.
                         */
                        size_t iGroup = 0;
                        for (; (!fAdmin) && (iGroup < pGroups->GroupCount); ++iGroup)
                        {
                            if (EqualSid (psidAdmin, pGroups->Groups[ iGroup ].Sid)) {
                                fAdmin = TRUE;
                            }
                        }
                    }

                    if (pGroups)
                        free(pGroups);
                }

                /* if do not have permission because we were not explicitly listed
                 * in the Admin Client Group let's see if we are the SYSTEM account
                 */
                if (!fAdmin) {
                    PTOKEN_USER pTokenUser;
                    SID_IDENTIFIER_AUTHORITY SIDAuth = SECURITY_NT_AUTHORITY;
                    PSID pSidLocalSystem = 0;
                    DWORD gle;

                    GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);

                    pTokenUser = (PTOKEN_USER)malloc(dwSize);
                    assert(pTokenUser);

                    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize))
                        gle = GetLastError();

                    if (AllocateAndInitializeSid( &SIDAuth, 1,
                                                  SECURITY_LOCAL_SYSTEM_RID,
                                                  0, 0, 0, 0, 0, 0, 0,
                                                  &pSidLocalSystem))
                    {
                        if (EqualSid(pTokenUser->User.Sid, pSidLocalSystem)) {
                            fAdmin = TRUE;
                        }

                        FreeSid(pSidLocalSystem);
                    }

                    if ( pTokenUser )
                        free(pTokenUser);
                }
            }
        }

        free(psidAdmin);
        free(pszRefDomain);

        fTested = TRUE;
    }

    return fAdmin;
}

void
fs_FreeUtf8CmdLine(int argc, char ** argv)
{
    int i;
    for (i=0; i < argc; i++) {
        if (argv[i])
            free(argv[i]);
    }
    free(argv);
}

char **
fs_MakeUtf8Cmdline(int argc, const wchar_t **wargv)
{
    char ** argv;
    int i;

    argv = calloc(argc, sizeof(argv[0]));
    if (argv == NULL)
        return NULL;

    for (i=0; i < argc; i++) {
        int s;

        s = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, FALSE);
        if (s == 0 ||
            (argv[i] = calloc(s+1, sizeof(char))) == NULL) {
            break;
        }

        s = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i], s+1, NULL, FALSE);
        if (s == 0) {
            break;
        }
    }

    if (i < argc) {
        fs_FreeUtf8CmdLine(argc, argv);
        return NULL;
    }

    return argv;
}

static const char *pn = "<cmd>";
void
fs_SetProcessName(const char *name)
{
    pn = name;
}

void
fs_Die(int code, char *filename)
{
    if (code == EINVAL) {
	if (filename)
	    fprintf(stderr,"%s: Invalid argument; it is possible that %s is not in AFS.\n", pn, filename);
	else
            fprintf(stderr,"%s: Invalid argument.\n", pn);
    }
    else if (code == ENOENT) {
	if (filename)
            fprintf(stderr,"%s: File '%s' doesn't exist\n", pn, filename);
	else
            fprintf(stderr,"%s: no such file returned\n", pn);
    }
    else if (code == EEXIST) {
	if (filename)
            fprintf(stderr,"%s: File '%s' already exists.\n", pn, filename);
	else
            fprintf(stderr,"%s: the specified file already exists.\n", pn);
    }
    else if (code == EROFS)
        fprintf(stderr,"%s: You can not change a backup or readonly volume\n", pn);
    else if (code == EACCES || code == EPERM) {
	if (filename)
            fprintf(stderr,"%s: You don't have the required access rights on '%s'\n", pn, filename);
	else
            fprintf(stderr,"%s: You do not have the required rights to do this operation\n", pn);
    }
    else if (code == ENODEV) {
	fprintf(stderr,"%s: AFS service may not have started.\n", pn);
    }
    else if (code == ESRCH) {   /* hack */
	fprintf(stderr,"%s: Cell name not recognized.\n", pn);
    }
    else if (code == EPIPE) {   /* hack */
	fprintf(stderr,"%s: Volume name or ID not recognized.\n", pn);
    }
    else if (code == EFBIG) {
	fprintf(stderr,"%s: Cache size too large.\n", pn);
    }
    else if (code == ETIMEDOUT) {
	if (filename)
	    fprintf(stderr,"%s:'%s': Connection timed out", pn, filename);
	else
	    fprintf(stderr,"%s: Connection timed out", pn);
    }
    else if (code == EBUSY) {
	if (filename)
            fprintf(stderr,"%s: All servers are busy on which '%s' resides\n", pn, filename);
	else
            fprintf(stderr,"%s: All servers are busy\n", pn);
    }
    else if (code == ENXIO) {
	if (filename)
            fprintf(stderr,"%s: All volume instances are offline on which '%s' resides\n", pn, filename);
	else
            fprintf(stderr,"%s: All volume instances are offline\n", pn);
    }
    else if (code == ENOSYS) {
	if (filename)
            fprintf(stderr,"%s: All servers are down on which '%s' resides\n", pn, filename);
	else
            fprintf(stderr,"%s: All servers are down\n", pn);
    }
    else if (code == ECHILD) {  /* hack */
	if (filename)
            fprintf(stderr,"%s: Invalid owner specified for '%s'\n", pn, filename);
	else
            fprintf(stderr,"%s: Invalid owner specified\n", pn);
    }
    else {
	if (filename)
            fprintf(stderr,"%s:'%s'", pn, filename);
	else
            fprintf(stderr,"%s", pn);

	fprintf(stderr, ": code 0x%x\n", code);
    }
}

/* values match cache manager File Types */
const char *
fs_filetypestr(afs_uint32 type)
{
    char * s = "Object";

    switch (type) {
    case 1:
        s = "File";
        break;
    case 2:
        s = "Directory";
        break;
    case 3:
        s = "Symlink";
        break;
    case 4:
        s = "Mountpoint";
        break;
    case 5:
        s = "DfsLink";
        break;
    }
    return s;
}
