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

RCSID
    ("$Header: /cvs/openafs/src/sys/pioctl_nt.c,v 1.18.2.7 2005/01/31 04:09:37 shadow Exp $");

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

#include <lanahelper.h>

static char AFSConfigKeyName[] =
    "SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters";

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

    if (fileNamep) {
        drivep = strchr(fileNamep, ':');
        if (drivep && (drivep - fileNamep) >= 1) {
            tbuffer[0] = *(drivep - 1);
            tbuffer[1] = ':';
            strcpy(tbuffer + 2, SMB_IOCTL_FILENAME);
        } else if (fileNamep[0] == fileNamep[1] && 
			       fileNamep[0] == '\\')
        {
            int count = 0, i = 0;

            while (count < 4 && fileNamep[i]) {
                tbuffer[i] = fileNamep[i];
                if ( tbuffer[i++] == '\\' )
                    count++;
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
                       curdir[0] == '\\') 
            {
                int count = 0, i = 0;

                while (count < 4 && curdir[i]) {
                    tbuffer[i] = curdir[i];
                    if ( tbuffer[i++] == '\\' )
                        count++;
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

    fflush(stdout);
    /* now open the file */
    fh = CreateFile(tbuffer, GENERIC_READ | GENERIC_WRITE,
		    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		    FILE_FLAG_WRITE_THROUGH, NULL);
    fflush(stdout);
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
                fprintf(stderr,"pioctl CreateFile(%s) failed: 0x%8X\r\n\t[%s]\r\n",
                        tbuffer,gle,buf);
            }
        }
#ifdef COMMENT
        if (gle != ERROR_DOWNGRADE_DETECTED)
            return -1;                                   
#endif

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
                fprintf(stderr, "pioctl logon user: [%s]\r\n",szUser);

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
                goto next_attempt;

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
                        fprintf(stderr,"pioctl CreateFile(%s) failed: 0x%8X\r\n\t[%s]\r\n",
                                 tbuffer,gle,buf);
                    }
                }
            }
        }
    }

  next_attempt:
    if ( fh == INVALID_HANDLE_VALUE ) {
        if (GetUserNameEx(NameSamCompatible, szUser, &dwSize)) {
            if ( ioctlDebug )
                fprintf(stderr, "pioctl logon user: [%s]\r\n",szUser);

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
                        fprintf(stderr,"pioctl CreateFile(%s) failed: 0x%8X\r\n\t[%s]\r\n",
                                 tbuffer,gle,buf);
                    }
                }
                return -1;
            }
        } else {
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

    rcount = reqp->mp - reqp->data;
    if (rcount <= 0) {
        if ( IoctlDebug() )
            fprintf(stderr, "pioctl Transceive rcount <= 0: %d\r\n",rcount);
	return EINVAL;		/* not supposed to happen */
    }

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

    if (pathp[0] != 0 && pathp[1] == ':') {
	/* there's a drive letter there */
	firstp = pathp + 2;
	pathHasDrive = 1;
    } else {
	firstp = pathp;
	pathHasDrive = 0;
    }

    if ( firstp[0] == '\\' && firstp[1] == '\\') {
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
        return 0;
    } else if (firstp[0] == '\\' || firstp[0] == '/') {
        /* already an absolute pathname, just copy it back */
        strcpy(outPathp, firstp);
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
    else if ( tpath[0] == '\\' && tpath[1] == '\\') {
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
