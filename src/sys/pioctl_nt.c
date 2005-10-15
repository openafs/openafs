/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */
/* AFSIFS portions copyright (c) 2005
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any purpose,
 * so long as the name of the university of michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.  if
 * the above copyright notice or any other identification of the
 * university of michigan is included in any copy of any portion of
 * this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the
 * university of michigan as to its fitness for any purpose, and without
 * warranty by the university of michigan of any kind, either express
 * or implied, including without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  the regents
 * of the university of michigan shall not be liable for any damages,
 * including special, indirect, incidental, or consequential damages, 
 * with respect to any claim arising out or in connection with the use
 * of the software, even if it has been or is hereafter advised of the
 * possibility of such damages.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <winioctl.h>
#include <winsock2.h>
#define SECURITY_WIN32
#include <security.h>
#include <nb30.h>

#include <osi.h>

#include <cm.h>
#include <cm_dir.h>
#include <cm_cell.h>
#include <cm_user.h>
#include <cm_conn.h>
#include <cm_scache.h>
#include <cm_buf.h>
#include <cm_utils.h>
#include <cm_ioctl.h>

#include <smb.h>
#include <pioctl_nt.h>
#include <WINNT/afsreg.h>
#include <lanahelper.h>
#include <../WINNT/afsrdr/kif.h>

#include <loadfuncs-krb5.h>
#include <krb5.h>

static char AFSConfigKeyName[] = AFSREG_CLT_SVC_PARAM_SUBKEY;

#define FS_IOCTLREQUEST_MAXSIZE	8192
/* big structure for representing and storing an IOCTL request */
typedef struct fs_ioctlRequest {
    char *mp;			/* marshalling/unmarshalling ptr */
    long nbytes;		/* bytes received (when unmarshalling) */
    char data[FS_IOCTLREQUEST_MAXSIZE];	/* data we're marshalling */
} fs_ioctlRequest_t;

static int
CMtoUNIXerror(int cm_code)
{
    switch (cm_code) {
    case CM_ERROR_TIMEDOUT:
	return ETIMEDOUT;
    case CM_ERROR_NOACCESS:
	return EACCES;
    case CM_ERROR_NOSUCHFILE:
	return ENOENT;
    case CM_ERROR_INVAL:
	return EINVAL;
    case CM_ERROR_BADFD:
	return EBADF;
    case CM_ERROR_EXISTS:
	return EEXIST;
    case CM_ERROR_CROSSDEVLINK:
	return EXDEV;
    case CM_ERROR_NOTDIR:
	return ENOTDIR;
    case CM_ERROR_ISDIR:
	return EISDIR;
    case CM_ERROR_READONLY:
	return EROFS;
    case CM_ERROR_WOULDBLOCK:
	return EWOULDBLOCK;
    case CM_ERROR_NOSUCHCELL:
	return ESRCH;		/* hack */
    case CM_ERROR_NOSUCHVOLUME:
	return EPIPE;		/* hack */
    case CM_ERROR_NOMORETOKENS:
	return EDOM;		/* hack */
    case CM_ERROR_TOOMANYBUFS:
	return EFBIG;		/* hack */
    default:
	if (cm_code > 0 && cm_code < EILSEQ)
	    return cm_code;
	else
	    return ENOTTY;
    }
}

static void
InitFSRequest(fs_ioctlRequest_t * rp)
{
    rp->mp = rp->data;
    rp->nbytes = 0;
}

static BOOL
IoctlDebug(void)
{
    static int init = 0;
    static BOOL debug = 0;

    if ( !init ) {
        HKEY hk;

        if (RegOpenKey (HKEY_LOCAL_MACHINE, 
                         TEXT("Software\\OpenAFS\\Client"), &hk) == 0)
        {
            DWORD dwSize = sizeof(BOOL);
            DWORD dwType = REG_DWORD;
            RegQueryValueEx (hk, TEXT("IoctlDebug"), NULL, &dwType, (PBYTE)&debug, &dwSize);
            RegCloseKey (hk);
        }

        init = 1;
    }

    return debug;
}


// krb5 functions
DECL_FUNC_PTR(krb5_cc_default_name);
DECL_FUNC_PTR(krb5_cc_set_default_name);
DECL_FUNC_PTR(krb5_get_default_config_files);
DECL_FUNC_PTR(krb5_free_config_files);
DECL_FUNC_PTR(krb5_free_context);
DECL_FUNC_PTR(krb5_get_default_realm);
DECL_FUNC_PTR(krb5_free_default_realm);
DECL_FUNC_PTR(krb5_init_context);
DECL_FUNC_PTR(krb5_cc_default);
DECL_FUNC_PTR(krb5_parse_name);
DECL_FUNC_PTR(krb5_free_principal);
DECL_FUNC_PTR(krb5_cc_close);
DECL_FUNC_PTR(krb5_cc_get_principal);
DECL_FUNC_PTR(krb5_build_principal);
DECL_FUNC_PTR(krb5_c_random_make_octets);
DECL_FUNC_PTR(krb5_get_init_creds_password);
DECL_FUNC_PTR(krb5_free_cred_contents);
DECL_FUNC_PTR(krb5_cc_resolve);
DECL_FUNC_PTR(krb5_unparse_name);
DECL_FUNC_PTR(krb5_free_unparsed_name);

FUNC_INFO krb5_fi[] = {
    MAKE_FUNC_INFO(krb5_cc_default_name),
    MAKE_FUNC_INFO(krb5_cc_set_default_name),
    MAKE_FUNC_INFO(krb5_get_default_config_files),
    MAKE_FUNC_INFO(krb5_free_config_files),
    MAKE_FUNC_INFO(krb5_free_context),
    MAKE_FUNC_INFO(krb5_get_default_realm),
    MAKE_FUNC_INFO(krb5_free_default_realm),
    MAKE_FUNC_INFO(krb5_init_context),
    MAKE_FUNC_INFO(krb5_cc_default),
    MAKE_FUNC_INFO(krb5_parse_name),
    MAKE_FUNC_INFO(krb5_free_principal),
    MAKE_FUNC_INFO(krb5_cc_close),
    MAKE_FUNC_INFO(krb5_cc_get_principal),
    MAKE_FUNC_INFO(krb5_build_principal),
    MAKE_FUNC_INFO(krb5_c_random_make_octets),
    MAKE_FUNC_INFO(krb5_get_init_creds_password),
    MAKE_FUNC_INFO(krb5_free_cred_contents),
    MAKE_FUNC_INFO(krb5_cc_resolve),
    MAKE_FUNC_INFO(krb5_unparse_name),
    MAKE_FUNC_INFO(krb5_free_unparsed_name),
    END_FUNC_INFO
};

static int
LoadFuncs(
    const char* dll_name,
    FUNC_INFO fi[],
    HINSTANCE* ph,  // [out, optional] - DLL handle
    int* pindex,    // [out, optional] - index of last func loaded (-1 if none)
    int cleanup,    // cleanup function pointers and unload on error
    int go_on,      // continue loading even if some functions cannot be loaded
    int silent      // do not pop-up a system dialog if DLL cannot be loaded
    )
{
    HINSTANCE h;
    int i, n, last_i;
    int error = 0;
    UINT em;

    if (ph) *ph = 0;
    if (pindex) *pindex = -1;

    for (n = 0; fi[n].func_ptr_var; n++)
        *(fi[n].func_ptr_var) = 0;

    if (silent)
        em = SetErrorMode(SEM_FAILCRITICALERRORS);
    h = LoadLibrary(dll_name);
    if (silent)
        SetErrorMode(em);

    if (!h)
        return 0;

    last_i = -1;
    for (i = 0; (go_on || !error) && (i < n); i++)
    {
        void* p = (void*)GetProcAddress(h, fi[i].func_name);
        if (!p)
            error = 1;
        else
        {
            last_i = i;
            *(fi[i].func_ptr_var) = p;
        }
    }
    if (pindex) *pindex = last_i;
    if (error && cleanup && !go_on) {
        for (i = 0; i < n; i++) {
            *(fi[i].func_ptr_var) = 0;
        }
        FreeLibrary(h);
        return 0;
    }
    if (ph) *ph = h;
    if (error) return 0;
    return 1;
}

#define KERB5DLL "krb5_32.dll"
static BOOL
IsKrb5Available()
{
    static HINSTANCE hKrb5DLL = 0;

    if ( hKrb5DLL )
        return TRUE;

    hKrb5DLL = LoadLibrary(KERB5DLL);
    if (hKrb5DLL) {
        if (!LoadFuncs(KERB5DLL, krb5_fi, 0, 0, 1, 0, 0))
        {
            FreeLibrary(hKrb5DLL);
            hKrb5DLL = 0;
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

static BOOL
GetLSAPrincipalName(char * szUser, DWORD *dwSize)
{
    krb5_context   ctx = 0;
    krb5_error_code code;
    krb5_ccache mslsa_ccache=0;
    krb5_principal princ = 0;
    char * pname = 0;
    BOOL success = 0;

    if (!IsKrb5Available())
        return FALSE;

    if (code = pkrb5_init_context(&ctx))
        goto cleanup;

    if (code = pkrb5_cc_resolve(ctx, "MSLSA:", &mslsa_ccache))
        goto cleanup;

    if (code = pkrb5_cc_get_principal(ctx, mslsa_ccache, &princ))
        goto cleanup;

    if (code = pkrb5_unparse_name(ctx, princ, &pname))
        goto cleanup;

    if ( strlen(pname) < *dwSize ) {
        strncpy(szUser, pname, *dwSize);
        szUser[*dwSize-1] = '\0';
        success = 1;
    }
    *dwSize = strlen(pname);

  cleanup:
    if (pname)
        pkrb5_free_unparsed_name(ctx, pname);

    if (princ)
        pkrb5_free_principal(ctx, princ);

    if (mslsa_ccache)
        pkrb5_cc_close(ctx, mslsa_ccache);

    if (ctx)
        pkrb5_free_context(ctx);
    return success;
}

static long
GetIoctlHandle(char *fileNamep, HANDLE * handlep)
{
    char *drivep;
    char netbiosName[MAX_NB_NAME_LENGTH];
    char tbuffer[256]="";
    HANDLE fh;
    HKEY hk;
    char szUser[128] = "";
    char szClient[MAX_PATH] = "";
    char szPath[MAX_PATH] = "";
    NETRESOURCE nr;
    DWORD res;
    DWORD ioctlDebug = IoctlDebug();
    DWORD gle;
    DWORD dwSize = sizeof(szUser);

#ifndef AFSIFS
    if (fileNamep) {
        drivep = strchr(fileNamep, ':');
        if (drivep && (drivep - fileNamep) >= 1) {
            tbuffer[0] = *(drivep - 1);
            tbuffer[1] = ':';
            strcpy(tbuffer + 2, SMB_IOCTL_FILENAME);
        } else if (fileNamep[0] == fileNamep[1] && 
		   (fileNamep[0] == '\\' || fileNamep[0] == '/'))
        {
            int count = 0, i = 0;

            while (count < 4 && fileNamep[i]) {
                tbuffer[i] = fileNamep[i];
                if ( tbuffer[i] == '\\' ||
		     tbuffer[i] == '/')
                    count++;
		i++;
            }
            if (fileNamep[i] == 0)
                tbuffer[i++] = '\\';
            tbuffer[i] = 0;
            strcat(tbuffer, SMB_IOCTL_FILENAME);
        } else {
            char curdir[256]="";

            GetCurrentDirectory(sizeof(curdir), curdir);
            if ( curdir[1] == ':' ) {
                tbuffer[0] = curdir[0];
                tbuffer[1] = ':';
                strcpy(tbuffer + 2, SMB_IOCTL_FILENAME);
            } else if (curdir[0] == curdir[1] &&
                       (curdir[0] == '\\' || curdir[0] == '/')) 
            {
                int count = 0, i = 0;

                while (count < 4 && curdir[i]) {
                    tbuffer[i] = curdir[i];
		    if ( tbuffer[i] == '\\' ||
			 tbuffer[i] == '/')
			count++;
		    i++;
                }
                if (tbuffer[i] == 0)
                    tbuffer[i++] = '\\';
                tbuffer[i] = 0;
                strcat(tbuffer, SMB_IOCTL_FILENAME_NOSLASH);
            }
        }
    }
    if (!tbuffer[0]) {
        /* No file name starting with drive colon specified, use UNC name */
        lana_GetNetbiosName(netbiosName,LANA_NETBIOS_NAME_FULL);
        sprintf(tbuffer,"\\\\%s\\all%s",netbiosName,SMB_IOCTL_FILENAME);
    }
#else   
    sprintf(tbuffer,"\\\\.\\afscom\\ioctl");
#endif 

    fflush(stdout);
    /* now open the file */
    fh = CreateFile(tbuffer, GENERIC_READ | GENERIC_WRITE,
		    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		    FILE_FLAG_WRITE_THROUGH, NULL);

	fflush(stdout);

#ifdef AFSIFS
    if (fh == INVALID_HANDLE_VALUE) {
        return -1;
    }
#endif
	
	if (fh == INVALID_HANDLE_VALUE) {
        int  gonext = 0;

        gle = GetLastError();
        if (gle && ioctlDebug ) {
            char buf[4096];

            if ( FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                               NULL,
                               gle,
                               MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),
                               buf,
                               4096,
                               (va_list *) NULL
                               ) )
            {
                fprintf(stderr,"pioctl CreateFile(%s) failed: 0x%X\r\n\t[%s]\r\n",
                        tbuffer,gle,buf);
            }
        }

        lana_GetNetbiosName(szClient, LANA_NETBIOS_NAME_FULL);

        if (RegOpenKey (HKEY_CURRENT_USER, 
                         TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"), &hk) == 0)
        {
            DWORD dwType = REG_SZ;
            RegQueryValueEx (hk, TEXT("Logon User Name"), NULL, &dwType, (PBYTE)szUser, &dwSize);
            RegCloseKey (hk);
        }

        if ( szUser[0] ) {
            if ( ioctlDebug )
                fprintf(stderr, "pioctl Explorer logon user: [%s]\r\n",szUser);

            sprintf(szPath, "\\\\%s", szClient);
            memset (&nr, 0x00, sizeof(NETRESOURCE));
            nr.dwType=RESOURCETYPE_DISK;
            nr.lpLocalName=0;
            nr.lpRemoteName=szPath;
            res = WNetAddConnection2(&nr,NULL,szUser,0);
            if (res) {
                if ( ioctlDebug ) {
                    fprintf(stderr, "pioctl WNetAddConnection2(%s,%s) failed: 0x%X\r\n",
                             szPath,szUser,res);
                }
                gonext = 1;
            }

            sprintf(szPath, "\\\\%s\\all", szClient);
            res = WNetAddConnection2(&nr,NULL,szUser,0);
            if (res) {
                if ( ioctlDebug ) {
                    fprintf(stderr, "pioctl WNetAddConnection2(%s,%s) failed: 0x%X\r\n",
                             szPath,szUser,res);
                }
                gonext = 1;
            }

            if (gonext)
                goto try_lsa_principal;

            fh = CreateFile(tbuffer, GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                             FILE_FLAG_WRITE_THROUGH, NULL);
            fflush(stdout);
            if (fh == INVALID_HANDLE_VALUE) {
                gle = GetLastError();
                if (gle && ioctlDebug ) {
                    char buf[4096];

                    if ( FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                                        NULL,
                                        gle,
                                        MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),
                                        buf,
                                        4096,
                                        (va_list *) NULL
                                        ) )
                    {
                        fprintf(stderr,"pioctl CreateFile(%s) failed: 0x%X\r\n\t[%s]\r\n",
                                 tbuffer,gle,buf);
                    }
                }
            }
        }
    }

  try_lsa_principal:
    if (fh == INVALID_HANDLE_VALUE) {
        int  gonext = 0;

        dwSize = sizeof(szUser);
        if (GetLSAPrincipalName(szUser, &dwSize)) {
            if ( ioctlDebug )
                fprintf(stderr, "pioctl LSA Principal logon user: [%s]\r\n",szUser);

            sprintf(szPath, "\\\\%s", szClient);
            memset (&nr, 0x00, sizeof(NETRESOURCE));
            nr.dwType=RESOURCETYPE_DISK;
            nr.lpLocalName=0;
            nr.lpRemoteName=szPath;
            res = WNetAddConnection2(&nr,NULL,szUser,0);
            if (res) {
                if ( ioctlDebug ) {
                    fprintf(stderr, "pioctl WNetAddConnection2(%s,%s) failed: 0x%X\r\n",
                             szPath,szUser,res);
                }
                gonext = 1;
            }

            sprintf(szPath, "\\\\%s\\all", szClient);
            res = WNetAddConnection2(&nr,NULL,szUser,0);
            if (res) {
                if ( ioctlDebug ) {
                    fprintf(stderr, "pioctl WNetAddConnection2(%s,%s) failed: 0x%X\r\n",
                             szPath,szUser,res);
                }
                gonext = 1;
            }

            if (gonext)
                goto try_sam_compat;

            fh = CreateFile(tbuffer, GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                             FILE_FLAG_WRITE_THROUGH, NULL);
            fflush(stdout);
            if (fh == INVALID_HANDLE_VALUE) {
                gle = GetLastError();
                if (gle && ioctlDebug ) {
                    char buf[4096];

                    if ( FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                                        NULL,
                                        gle,
                                        MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),
                                        buf,
                                        4096,
                                        (va_list *) NULL
                                        ) )
                    {
                        fprintf(stderr,"pioctl CreateFile(%s) failed: 0x%X\r\n\t[%s]\r\n",
                                 tbuffer,gle,buf);
                    }
                }
            }
        }
    }

  try_sam_compat:
    if ( fh == INVALID_HANDLE_VALUE ) {
        dwSize = sizeof(szUser);
        if (GetUserNameEx(NameSamCompatible, szUser, &dwSize)) {
            if ( ioctlDebug )
                fprintf(stderr, "pioctl SamCompatible logon user: [%s]\r\n",szUser);

            sprintf(szPath, "\\\\%s", szClient);
            memset (&nr, 0x00, sizeof(NETRESOURCE));
            nr.dwType=RESOURCETYPE_DISK;
            nr.lpLocalName=0;
            nr.lpRemoteName=szPath;
            res = WNetAddConnection2(&nr,NULL,szUser,0);
            if (res) {
                if ( ioctlDebug ) {
                    fprintf(stderr, "pioctl WNetAddConnection2(%s,%s) failed: 0x%X\r\n",
                             szPath,szUser,res);
                }
            }

            sprintf(szPath, "\\\\%s\\all", szClient);
            res = WNetAddConnection2(&nr,NULL,szUser,0);
            if (res) {
                if ( ioctlDebug ) {
                    fprintf(stderr, "pioctl WNetAddConnection2(%s,%s) failed: 0x%X\r\n",
                             szPath,szUser,res);
                }
                return -1;
            }

            fh = CreateFile(tbuffer, GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                             FILE_FLAG_WRITE_THROUGH, NULL);
            fflush(stdout);
            if (fh == INVALID_HANDLE_VALUE) {
                gle = GetLastError();
                if (gle && ioctlDebug ) {
                    char buf[4096];

                    if ( FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                                        NULL,
                                        gle,
                                        MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),
                                        buf,
                                        4096,
                                        (va_list *) NULL
                                        ) )
                    {
                        fprintf(stderr,"pioctl CreateFile(%s) failed: 0x%X\r\n\t[%s]\r\n",
                                 tbuffer,gle,buf);
                    }
                }
                return -1;
            }
        } else {
            fprintf(stderr, "GetUserNameEx(NameSamCompatible) failed: 0x%X\r\n", GetLastError());
            return -1;
        }
    }

    /* return fh and success code */
    *handlep = fh;
    return 0;
}

static long
Transceive(HANDLE handle, fs_ioctlRequest_t * reqp)
{
    long rcount;
    long ioCount;
    DWORD gle;
	char *data;

    rcount = reqp->mp - reqp->data;
    if (rcount <= 0) {
        if ( IoctlDebug() )
            fprintf(stderr, "pioctl Transceive rcount <= 0: %d\r\n",rcount);
	return EINVAL;		/* not supposed to happen */
    }

#ifndef AFSIFS
    if (!WriteFile(handle, reqp->data, rcount, &ioCount, NULL)) {
	/* failed to write */
	gle = GetLastError();

        if ( IoctlDebug() )
            fprintf(stderr, "pioctl Transceive WriteFile failed: 0x%X\r\n",gle);
        return gle;
    }

    if (!ReadFile(handle, reqp->data, sizeof(reqp->data), &ioCount, NULL)) {
	/* failed to read */
	gle = GetLastError();

        if ( IoctlDebug() )
            fprintf(stderr, "pioctl Transceive ReadFile failed: 0x%X\r\n",gle);
        return gle;
    }
#else
    /* ioctl completes as one operation, so copy input to a new buffer, and use as output buffer */
    data = malloc(rcount);
    memcpy(data, reqp->data, rcount);
    if (!DeviceIoControl(handle, IOCTL_AFSRDR_IOCTL, data, rcount, reqp->data, sizeof(reqp->data), &ioCount, NULL))
    {
        free(data);
        return GetLastError();
    }
    free(data);
#endif

    reqp->nbytes = ioCount;	/* set # of bytes available */
    reqp->mp = reqp->data;	/* restart marshalling */

    /* return success */
    return 0;
}

static long
MarshallLong(fs_ioctlRequest_t * reqp, long val)
{
    memcpy(reqp->mp, &val, 4);
    reqp->mp += 4;
    return 0;
}

static long
UnmarshallLong(fs_ioctlRequest_t * reqp, long *valp)
{
    /* not enough data left */
    if (reqp->nbytes < 4) {
        if ( IoctlDebug() )
            fprintf(stderr, "pioctl UnmarshallLong reqp->nbytes < 4: %d\r\n",
                     reqp->nbytes);
	return -1;
    }

    memcpy(valp, reqp->mp, 4);
    reqp->mp += 4;
    reqp->nbytes -= 4;
    return 0;
}

/* includes marshalling NULL pointer as a null (0 length) string */
static long
MarshallString(fs_ioctlRequest_t * reqp, char *stringp)
{
    int count;

    if (stringp)
	count = strlen(stringp) + 1;	/* space required including null */
    else
	count = 1;

    /* watch for buffer overflow */
    if ((reqp->mp - reqp->data) + count > sizeof(reqp->data)) {
        if ( IoctlDebug() )
            fprintf(stderr, "pioctl MarshallString buffer overflow\r\n");
	return -1;
    }

    if (stringp)
	memcpy(reqp->mp, stringp, count);
    else
	*(reqp->mp) = 0;
    reqp->mp += count;
    return 0;
}

/* take a path with a drive letter, possibly relative, and return a full path
 * without the drive letter.  This is the full path relative to the working
 * dir for that drive letter.  The input and output paths can be the same.
 */
static long
fs_GetFullPath(char *pathp, char *outPathp, long outSize)
{
    char tpath[1000];
    char origPath[1000];
    char *firstp;
    long code;
    int pathHasDrive;
    int doSwitch;
    char newPath[3];
    HANDLE rootDir;
    wchar_t *wpath;
    unsigned long length;
    char * p;

#ifdef AFSIFS
    if (!pathp)
        return CM_ERROR_NOSUCHPATH;

    //sprintf(tpath, "%c:\\", pathp[0]);
    rootDir = CreateFile(pathp, 0, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (rootDir == INVALID_HANDLE_VALUE)
        return CM_ERROR_NOSUCHPATH;

    wpath = tpath;
    length = 0;
    if (!DeviceIoControl(rootDir, IOCTL_AFSRDR_GET_PATH, NULL, 0, wpath, 1000, &length, NULL))
    {
        CloseHandle(rootDir);
        return CM_ERROR_NOSUCHPATH;
    }
    CloseHandle(rootDir);

    code = WideCharToMultiByte(CP_UTF8, 0/*WC_NO_BEST_FIT_CHARS*/, wpath, length/sizeof(wchar_t), outPathp, outSize/sizeof(wchar_t), NULL, NULL);

//    strcpy(outPathp, tpath);
    return 0;
#endif

    if (pathp[0] != 0 && pathp[1] == ':') {
	/* there's a drive letter there */
	firstp = pathp + 2;
	pathHasDrive = 1;
    } else {
	firstp = pathp;
	pathHasDrive = 0;
    }

    if ( firstp[0] == '\\' && firstp[1] == '\\' || 
	 firstp[0] == '/' && firstp[1] == '/') {
        /* UNC path - strip off the server and sharename */
        int i, count;
        for ( i=2,count=2; count < 4 && firstp[i]; i++ ) {
            if ( firstp[i] == '\\' || firstp[i] == '/' ) {
                count++;
            }
        }
        if ( firstp[i] == 0 ) {
            strcpy(outPathp,"\\");
        } else {
            strcpy(outPathp,&firstp[--i]);
        }
	for (p=outPathp ;*p; p++) {
	    if (*p == '/')
		*p = '\\';
	}
        return 0;
    } else if (firstp[0] == '\\' || firstp[0] == '/') {
        /* already an absolute pathname, just copy it back */
        strcpy(outPathp, firstp);
	for (p=outPathp ;*p; p++) {
	    if (*p == '/')
		*p = '\\';
	}
        return 0;
    }

    GetCurrentDirectory(sizeof(origPath), origPath);

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
	if (!SetCurrentDirectory(newPath)) {
	    code = GetLastError();

            if ( IoctlDebug() )
                fprintf(stderr, "pioctl fs_GetFullPath SetCurrentDirectory(%s) failed: 0x%X\r\n",
                         newPath, code);
	    return code;
	}
    }

    /* now get the absolute path to the current wdir in this drive */
    GetCurrentDirectory(sizeof(tpath), tpath);
    if (tpath[1] == ':')
        strcpy(outPathp, tpath + 2);	/* skip drive letter */
    else if ( tpath[0] == '\\' && tpath[1] == '\\'||
	      tpath[0] == '/' && tpath[1] == '/') {
        /* UNC path - strip off the server and sharename */
        int i, count;
        for ( i=2,count=2; count < 4 && tpath[i]; i++ ) {
            if ( tpath[i] == '\\' || tpath[i] == '/' ) {
                count++;
            }
        }
        if ( tpath[i] == 0 ) {
            strcpy(outPathp,"\\");
        } else {
            strcpy(outPathp,&tpath[--i]);
        }
    } else {
        /* this should never happen */
        strcpy(outPathp, tpath);
    }

    /* if there is a non-null name after the drive, append it */
    if (*firstp != 0) {
        int len = strlen(outPathp);
        if (outPathp[len-1] != '\\' && outPathp[len-1] != '/') 
            strcat(outPathp, "\\");
        strcat(outPathp, firstp);
    }

    /* finally, if necessary, switch back to our home drive letter */
    if (doSwitch) {
	SetCurrentDirectory(origPath);
    }

    for (p=outPathp ;*p; p++) {
	if (*p == '/')
	    *p = '\\';
    }
    return 0;
}

long
pioctl(char *pathp, long opcode, struct ViceIoctl *blobp, int follow)
{
    fs_ioctlRequest_t preq;
    long code;
    long temp;
    char fullPath[1000];
    HANDLE reqHandle;

    code = GetIoctlHandle(pathp, &reqHandle);
    if (code) {
	if (pathp)
	    errno = EINVAL;
	else
	    errno = ENODEV;
	return code;
    }

    /* init the request structure */
    InitFSRequest(&preq);

    /* marshall the opcode, the path name and the input parameters */
    MarshallLong(&preq, opcode);
    /* when marshalling the path, remove the drive letter, since we already
     * used the drive letter to find the AFS daemon; we don't need it any more.
     * Eventually we'll expand relative path names here, too, since again, only
     * we understand those.
     */
    if (pathp) {
	code = fs_GetFullPath(pathp, fullPath, sizeof(fullPath));
	if (code) {
	    CloseHandle(reqHandle);
	    errno = EINVAL;
	    return code;
	}
    } else {
	strcpy(fullPath, "");
    }

    MarshallString(&preq, fullPath);
    if (blobp->in_size) {
	memcpy(preq.mp, blobp->in, blobp->in_size);
	preq.mp += blobp->in_size;
    }

    /* now make the call */
    code = Transceive(reqHandle, &preq);
    if (code) {
	CloseHandle(reqHandle);
	return code;
    }

    /* now unmarshall the return value */
    if (UnmarshallLong(&preq, &temp) != 0) {
        CloseHandle(reqHandle);
        return -1;
    }

    if (temp != 0) {
	CloseHandle(reqHandle);
	errno = CMtoUNIXerror(temp);
        if ( IoctlDebug() )
            fprintf(stderr, "pioctl temp != 0: 0x%X\r\n",temp);
	return -1;
    }

    /* otherwise, unmarshall the output parameters */
    if (blobp->out_size) {
	temp = blobp->out_size;
	if (preq.nbytes < temp)
	    temp = preq.nbytes;
	memcpy(blobp->out, preq.mp, temp);
	blobp->out_size = temp;
    }

    /* and return success */
    CloseHandle(reqHandle);
    return 0;
}
