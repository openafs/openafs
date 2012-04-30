/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <errno.h>
#include <windows.h>
#include <shlwapi.h>
#include <winsock2.h>
#include <string.h>
#include <malloc.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>

#include "afsd.h"
#include <osi.h>
#include <rx/rx.h>

#include <afs/unified_afs.h>

static osi_once_t cm_utilsOnce;

osi_rwlock_t cm_utilsLock;

cm_space_t *cm_spaceListp;

static int et2sys[512];

static DWORD cm_TlsRequestSlot = -1;

void cm_utilsInit(void)
{
    if (osi_Once(&cm_utilsOnce)) {
        lock_InitializeRWLock(&cm_utilsLock, "cm_utilsLock", LOCK_HIERARCHY_UTILS_GLOBAL);

        cm_TlsRequestSlot = TlsAlloc();

        osi_EndOnce(&cm_utilsOnce);
    }
}

void cm_utilsCleanup(void)
{
    TlsFree(cm_TlsRequestSlot);
}

void
init_et_to_sys_error(void)
{
    memset(&et2sys, 0, sizeof(et2sys));
    et2sys[(UAEPERM - ERROR_TABLE_BASE_uae)] = EPERM;
    et2sys[(UAENOENT - ERROR_TABLE_BASE_uae)] = ENOENT;
    et2sys[(UAESRCH - ERROR_TABLE_BASE_uae)] = ESRCH;
    et2sys[(UAEINTR - ERROR_TABLE_BASE_uae)] = EINTR;
    et2sys[(UAEIO - ERROR_TABLE_BASE_uae)] = EIO;
    et2sys[(UAENXIO - ERROR_TABLE_BASE_uae)] = ENXIO;
    et2sys[(UAE2BIG - ERROR_TABLE_BASE_uae)] = E2BIG;
    et2sys[(UAENOEXEC - ERROR_TABLE_BASE_uae)] = ENOEXEC;
    et2sys[(UAEBADF - ERROR_TABLE_BASE_uae)] = EBADF;
    et2sys[(UAECHILD - ERROR_TABLE_BASE_uae)] = ECHILD;
    et2sys[(UAEAGAIN - ERROR_TABLE_BASE_uae)] = EAGAIN;
    et2sys[(UAENOMEM - ERROR_TABLE_BASE_uae)] = ENOMEM;
    et2sys[(UAEACCES - ERROR_TABLE_BASE_uae)] = EACCES;
    et2sys[(UAEFAULT - ERROR_TABLE_BASE_uae)] = EFAULT;
    et2sys[(UAENOTBLK - ERROR_TABLE_BASE_uae)] = ENOTBLK;
    et2sys[(UAEBUSY - ERROR_TABLE_BASE_uae)] = EBUSY;
    et2sys[(UAEEXIST - ERROR_TABLE_BASE_uae)] = EEXIST;
    et2sys[(UAEXDEV - ERROR_TABLE_BASE_uae)] = EXDEV;
    et2sys[(UAENODEV - ERROR_TABLE_BASE_uae)] = ENODEV;
    et2sys[(UAENOTDIR - ERROR_TABLE_BASE_uae)] = ENOTDIR;
    et2sys[(UAEISDIR - ERROR_TABLE_BASE_uae)] = EISDIR;
    et2sys[(UAEINVAL - ERROR_TABLE_BASE_uae)] = EINVAL;
    et2sys[(UAENFILE - ERROR_TABLE_BASE_uae)] = ENFILE;
    et2sys[(UAEMFILE - ERROR_TABLE_BASE_uae)] = EMFILE;
    et2sys[(UAENOTTY - ERROR_TABLE_BASE_uae)] = ENOTTY;
    et2sys[(UAETXTBSY - ERROR_TABLE_BASE_uae)] = ETXTBSY;
    et2sys[(UAEFBIG - ERROR_TABLE_BASE_uae)] = EFBIG;
    et2sys[(UAENOSPC - ERROR_TABLE_BASE_uae)] = ENOSPC;
    et2sys[(UAESPIPE - ERROR_TABLE_BASE_uae)] = ESPIPE;
    et2sys[(UAEROFS - ERROR_TABLE_BASE_uae)] = EROFS;
    et2sys[(UAEMLINK - ERROR_TABLE_BASE_uae)] = EMLINK;
    et2sys[(UAEPIPE - ERROR_TABLE_BASE_uae)] = EPIPE;
    et2sys[(UAEDOM - ERROR_TABLE_BASE_uae)] = EDOM;
    et2sys[(UAERANGE - ERROR_TABLE_BASE_uae)] = ERANGE;
    et2sys[(UAEDEADLK - ERROR_TABLE_BASE_uae)] = EDEADLK;
    et2sys[(UAENAMETOOLONG - ERROR_TABLE_BASE_uae)] = ENAMETOOLONG;
    et2sys[(UAENOLCK - ERROR_TABLE_BASE_uae)] = ENOLCK;
    et2sys[(UAENOSYS - ERROR_TABLE_BASE_uae)] = ENOSYS;
    et2sys[(UAENOTEMPTY - ERROR_TABLE_BASE_uae)] = ENOTEMPTY;
    et2sys[(UAELOOP - ERROR_TABLE_BASE_uae)] = ELOOP;
    et2sys[(UAEWOULDBLOCK - ERROR_TABLE_BASE_uae)] = EWOULDBLOCK;
    et2sys[(UAENOMSG - ERROR_TABLE_BASE_uae)] = ENOMSG;
    et2sys[(UAEIDRM - ERROR_TABLE_BASE_uae)] = EIDRM;
    et2sys[(UAECHRNG - ERROR_TABLE_BASE_uae)] = ECHRNG;
    et2sys[(UAEL2NSYNC - ERROR_TABLE_BASE_uae)] = EL2NSYNC;
    et2sys[(UAEL3HLT - ERROR_TABLE_BASE_uae)] = EL3HLT;
    et2sys[(UAEL3RST - ERROR_TABLE_BASE_uae)] = EL3RST;
    et2sys[(UAELNRNG - ERROR_TABLE_BASE_uae)] = ELNRNG;
    et2sys[(UAEUNATCH - ERROR_TABLE_BASE_uae)] = EUNATCH;
    et2sys[(UAENOCSI - ERROR_TABLE_BASE_uae)] = ENOCSI;
    et2sys[(UAEL2HLT - ERROR_TABLE_BASE_uae)] = EL2HLT;
    et2sys[(UAEBADE - ERROR_TABLE_BASE_uae)] = EBADE;
    et2sys[(UAEBADR - ERROR_TABLE_BASE_uae)] = EBADR;
    et2sys[(UAEXFULL - ERROR_TABLE_BASE_uae)] = EXFULL;
    et2sys[(UAENOANO - ERROR_TABLE_BASE_uae)] = ENOANO;
    et2sys[(UAEBADRQC - ERROR_TABLE_BASE_uae)] = EBADRQC;
    et2sys[(UAEBADSLT - ERROR_TABLE_BASE_uae)] = EBADSLT;
    et2sys[(UAEBFONT - ERROR_TABLE_BASE_uae)] = EBFONT;
    et2sys[(UAENOSTR - ERROR_TABLE_BASE_uae)] = ENOSTR;
    et2sys[(UAENODATA - ERROR_TABLE_BASE_uae)] = ENODATA;
    et2sys[(UAETIME - ERROR_TABLE_BASE_uae)] = ETIME;
    et2sys[(UAENOSR - ERROR_TABLE_BASE_uae)] = ENOSR;
    et2sys[(UAENONET - ERROR_TABLE_BASE_uae)] = ENONET;
    et2sys[(UAENOPKG - ERROR_TABLE_BASE_uae)] = ENOPKG;
    et2sys[(UAEREMOTE - ERROR_TABLE_BASE_uae)] = EREMOTE;
    et2sys[(UAENOLINK - ERROR_TABLE_BASE_uae)] = ENOLINK;
    et2sys[(UAEADV - ERROR_TABLE_BASE_uae)] = EADV;
    et2sys[(UAESRMNT - ERROR_TABLE_BASE_uae)] = ESRMNT;
    et2sys[(UAECOMM - ERROR_TABLE_BASE_uae)] = ECOMM;
    et2sys[(UAEPROTO - ERROR_TABLE_BASE_uae)] = EPROTO;
    et2sys[(UAEMULTIHOP - ERROR_TABLE_BASE_uae)] = EMULTIHOP;
    et2sys[(UAEDOTDOT - ERROR_TABLE_BASE_uae)] = EDOTDOT;
    et2sys[(UAEBADMSG - ERROR_TABLE_BASE_uae)] = EBADMSG;
    et2sys[(UAEOVERFLOW - ERROR_TABLE_BASE_uae)] = EOVERFLOW;
    et2sys[(UAENOTUNIQ - ERROR_TABLE_BASE_uae)] = ENOTUNIQ;
    et2sys[(UAEBADFD - ERROR_TABLE_BASE_uae)] = EBADFD;
    et2sys[(UAEREMCHG - ERROR_TABLE_BASE_uae)] = EREMCHG;
    et2sys[(UAELIBACC - ERROR_TABLE_BASE_uae)] = ELIBACC;
    et2sys[(UAELIBBAD - ERROR_TABLE_BASE_uae)] = ELIBBAD;
    et2sys[(UAELIBSCN - ERROR_TABLE_BASE_uae)] = ELIBSCN;
    et2sys[(UAELIBMAX - ERROR_TABLE_BASE_uae)] = ELIBMAX;
    et2sys[(UAELIBEXEC - ERROR_TABLE_BASE_uae)] = ELIBEXEC;
    et2sys[(UAEILSEQ - ERROR_TABLE_BASE_uae)] = EILSEQ;
    et2sys[(UAERESTART - ERROR_TABLE_BASE_uae)] = ERESTART;
    et2sys[(UAESTRPIPE - ERROR_TABLE_BASE_uae)] = ESTRPIPE;
    et2sys[(UAEUSERS - ERROR_TABLE_BASE_uae)] = EUSERS;
    et2sys[(UAENOTSOCK - ERROR_TABLE_BASE_uae)] = ENOTSOCK;
    et2sys[(UAEDESTADDRREQ - ERROR_TABLE_BASE_uae)] = EDESTADDRREQ;
    et2sys[(UAEMSGSIZE - ERROR_TABLE_BASE_uae)] = EMSGSIZE;
    et2sys[(UAEPROTOTYPE - ERROR_TABLE_BASE_uae)] = EPROTOTYPE;
    et2sys[(UAENOPROTOOPT - ERROR_TABLE_BASE_uae)] = ENOPROTOOPT;
    et2sys[(UAEPROTONOSUPPORT - ERROR_TABLE_BASE_uae)] = EPROTONOSUPPORT;
    et2sys[(UAESOCKTNOSUPPORT - ERROR_TABLE_BASE_uae)] = ESOCKTNOSUPPORT;
    et2sys[(UAEOPNOTSUPP - ERROR_TABLE_BASE_uae)] = EOPNOTSUPP;
    et2sys[(UAEPFNOSUPPORT - ERROR_TABLE_BASE_uae)] = EPFNOSUPPORT;
    et2sys[(UAEAFNOSUPPORT - ERROR_TABLE_BASE_uae)] = EAFNOSUPPORT;
    et2sys[(UAEADDRINUSE - ERROR_TABLE_BASE_uae)] = EADDRINUSE;
    et2sys[(UAEADDRNOTAVAIL - ERROR_TABLE_BASE_uae)] = EADDRNOTAVAIL;
    et2sys[(UAENETDOWN - ERROR_TABLE_BASE_uae)] = ENETDOWN;
    et2sys[(UAENETUNREACH - ERROR_TABLE_BASE_uae)] = ENETUNREACH;
    et2sys[(UAENETRESET - ERROR_TABLE_BASE_uae)] = ENETRESET;
    et2sys[(UAECONNABORTED - ERROR_TABLE_BASE_uae)] = ECONNABORTED;
    et2sys[(UAECONNRESET - ERROR_TABLE_BASE_uae)] = ECONNRESET;
    et2sys[(UAENOBUFS - ERROR_TABLE_BASE_uae)] = ENOBUFS;
    et2sys[(UAEISCONN - ERROR_TABLE_BASE_uae)] = EISCONN;
    et2sys[(UAENOTCONN - ERROR_TABLE_BASE_uae)] = ENOTCONN;
    et2sys[(UAESHUTDOWN - ERROR_TABLE_BASE_uae)] = ESHUTDOWN;
    et2sys[(UAETOOMANYREFS - ERROR_TABLE_BASE_uae)] = ETOOMANYREFS;
    et2sys[(UAETIMEDOUT - ERROR_TABLE_BASE_uae)] = ETIMEDOUT;
    et2sys[(UAECONNREFUSED - ERROR_TABLE_BASE_uae)] = ECONNREFUSED;
    et2sys[(UAEHOSTDOWN - ERROR_TABLE_BASE_uae)] = EHOSTDOWN;
    et2sys[(UAEHOSTUNREACH - ERROR_TABLE_BASE_uae)] = EHOSTUNREACH;
    et2sys[(UAEALREADY - ERROR_TABLE_BASE_uae)] = EALREADY;
    et2sys[(UAEINPROGRESS - ERROR_TABLE_BASE_uae)] = EINPROGRESS;
    et2sys[(UAESTALE - ERROR_TABLE_BASE_uae)] = ESTALE;
    et2sys[(UAEUCLEAN - ERROR_TABLE_BASE_uae)] = EUCLEAN;
    et2sys[(UAENOTNAM - ERROR_TABLE_BASE_uae)] = ENOTNAM;
    et2sys[(UAENAVAIL - ERROR_TABLE_BASE_uae)] = ENAVAIL;
    et2sys[(UAEISNAM - ERROR_TABLE_BASE_uae)] = EISNAM;
    et2sys[(UAEREMOTEIO - ERROR_TABLE_BASE_uae)] = EREMOTEIO;
    et2sys[(UAEDQUOT - ERROR_TABLE_BASE_uae)] = EDQUOT;
    et2sys[(UAENOMEDIUM - ERROR_TABLE_BASE_uae)] = ENOMEDIUM;
    et2sys[(UAEMEDIUMTYPE - ERROR_TABLE_BASE_uae)] = EMEDIUMTYPE;
}

static afs_int32
et_to_sys_error(afs_int32 in)
{
    if (in < ERROR_TABLE_BASE_uae || in >= ERROR_TABLE_BASE_uae + 512)
	return in;
    if (et2sys[in - ERROR_TABLE_BASE_uae] != 0)
	return et2sys[in - ERROR_TABLE_BASE_uae];
    return in;
}

long cm_MapRPCError(long error, cm_req_t *reqp)
{
    if (error == 0)
        return 0;

    /* If we had to stop retrying, report our saved error code. */
    if (reqp && error == CM_ERROR_TIMEDOUT) {
        if (reqp->accessError)
            return reqp->accessError;
        if (reqp->volumeError)
            return reqp->volumeError;
        if (reqp->rpcError)
            return reqp->rpcError;
        return error;
    }

    error = et_to_sys_error(error);

    if (error == RX_CALL_DEAD ||
        error == RX_CALL_TIMEOUT ||
        error == RX_CALL_BUSY ||
        error == RX_MSGSIZE ||
        error == VNOSERVICE)
        error = CM_ERROR_RETRY;
    else if (error == RX_CALL_IDLE)
        error = EIO;
    else if (error < 0)
        error = CM_ERROR_UNKNOWN;
    else if (error == EINVAL)
        error = CM_ERROR_INVAL;
    else if (error == EROFS)
        error = CM_ERROR_READONLY;
    else if (error == EACCES)
        error = CM_ERROR_NOACCESS;
    else if (error == EXDEV)
        error = CM_ERROR_CROSSDEVLINK;
    else if (error == EEXIST)
        error = CM_ERROR_EXISTS;
    else if (error == ENOTDIR)
        error = CM_ERROR_NOTDIR;
    else if (error == ENOENT)
        error = CM_ERROR_NOSUCHFILE;
    else if (error == EAGAIN
             || error == 35 	   /* EAGAIN, Digital UNIX */
             || error == WSAEWOULDBLOCK)
        error = CM_ERROR_WOULDBLOCK;
    else if (error == VDISKFULL
              || error == ENOSPC)
        error = CM_ERROR_SPACE;
    else if (error == VOVERQUOTA
              || error == EDQUOT
              || error == 49    /* EDQUOT on Solaris */
              || error == 88    /* EDQUOT on AIX */
              || error == 69    /* EDQUOT on Digital UNIX and HPUX */
              || error == 122   /* EDQUOT on Linux */
              || error == 1133) /* EDQUOT on Irix  */
        error = CM_ERROR_QUOTA;
    else if (error == VNOVNODE)
        error = CM_ERROR_BADFD;
    else if (error == VSALVAGE || error == VOFFLINE)
        error = CM_ERROR_ALLOFFLINE;
    else if (error == VBUSY || error == VRESTARTING)
        error = CM_ERROR_ALLBUSY;
    else if (error == EISDIR)
        return CM_ERROR_ISDIR;
    return error;
}

long cm_MapRPCErrorRmdir(long error, cm_req_t *reqp)
{
    if (error == 0)
        return 0;

    /* If we had to stop retrying, report our saved error code. */
    if (reqp && error == CM_ERROR_TIMEDOUT) {
        if (reqp->accessError)
            return reqp->accessError;
        if (reqp->volumeError)
            return reqp->volumeError;
        if (reqp->rpcError)
            return reqp->rpcError;
        return error;
    }

    error = et_to_sys_error(error);

    if (error == RX_CALL_DEAD ||
        error == RX_CALL_TIMEOUT ||
        error == RX_CALL_BUSY ||
        error == RX_CALL_IDLE ||
        error == RX_MSGSIZE ||
        error == VNOSERVICE)
        error = CM_ERROR_RETRY;
    else if (error == VNOVNODE)
        error = CM_ERROR_BADFD;
    else if (error == VSALVAGE || error == VOFFLINE)
        error = CM_ERROR_ALLOFFLINE;
    else if (error == VBUSY || error == VRESTARTING)
        error = CM_ERROR_ALLBUSY;
    else if (error < 0)
        error = CM_ERROR_UNKNOWN;
    else if (error == EROFS)
        error = CM_ERROR_READONLY;
    else if (error == ENOTDIR)
        error = CM_ERROR_NOTDIR;
    else if (error == EACCES)
        error = CM_ERROR_NOACCESS;
    else if (error == ENOENT)
        error = CM_ERROR_NOSUCHFILE;
    else if (error == EINVAL)
        error = CM_ERROR_INVAL;
    else if (error == ENOTEMPTY
              || error == 17		/* AIX */
              || error == 66		/* SunOS 4, Digital UNIX */
              || error == 93		/* Solaris 2, IRIX */
              || error == 247)	/* HP/UX */
        error = CM_ERROR_NOTEMPTY;
    return error;
}

long cm_MapVLRPCError(long error, cm_req_t *reqp)
{
    if (error == 0) return 0;

    /* If we had to stop retrying, report our saved error code. */
    if (reqp && error == CM_ERROR_TIMEDOUT) {
	if (reqp->accessError)
	    return reqp->accessError;
	if (reqp->volumeError)
	    return reqp->volumeError;
	if (reqp->rpcError)
	    return reqp->rpcError;
	return error;
    }

    error = et_to_sys_error(error);

    if (error == RX_CALL_DEAD ||
        error == RX_CALL_TIMEOUT ||
        error == RX_CALL_BUSY ||
        error == RX_CALL_IDLE ||
        error == RX_MSGSIZE ||
        error == VNOSERVICE)
        error = CM_ERROR_RETRY;
    else if (error == RX_RESTARTING)
        error = CM_ERROR_ALLBUSY;
    else if (error < 0)
        error = CM_ERROR_UNKNOWN;
    else if (error == EINVAL)
        error = CM_ERROR_INVAL;
    else if (error == VL_NOENT || error == VL_BADNAME)
	error = CM_ERROR_NOSUCHVOLUME;
    return error;
}

cm_space_t *cm_GetSpace(void)
{
	cm_space_t *tsp;

        cm_utilsInit();

        lock_ObtainWrite(&cm_utilsLock);
	if (tsp = cm_spaceListp) {
		cm_spaceListp = tsp->nextp;
        }
        else tsp = (cm_space_t *) malloc(sizeof(cm_space_t));
	(void) memset(tsp, 0, sizeof(cm_space_t));
        lock_ReleaseWrite(&cm_utilsLock);

        return tsp;
}

void cm_FreeSpace(cm_space_t *tsp)
{
        lock_ObtainWrite(&cm_utilsLock);
	tsp->nextp = cm_spaceListp;
	cm_spaceListp = tsp;
        lock_ReleaseWrite(&cm_utilsLock);
}

/* characters that are legal in an 8.3 name */
/*
 * We used to have 1's for all characters from 128 to 254.  But
 * the NT client behaves better if we create an 8.3 name for any
 * name that has a character with the high bit on, and if we
 * delete those characters from 8.3 names.  In particular, see
 * Sybase defect 10859.
 */
char cm_LegalChars[256] = {
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#define ISLEGALCHAR(c) ((c) < 256 && (c) > 0 && cm_LegalChars[(c)] != 0)

/* return true iff component is a valid 8.3 name */
int cm_Is8Dot3(clientchar_t *namep)
{
    int sawDot = 0;
    clientchar_t tc;
    int charCount = 0;

    if (namep == NULL || !namep[0])
        return 0;

    /*
     * can't have a leading dot;
     * special case for . and ..
     */
    if (namep[0] == '.') {
        if (namep[1] == 0)
            return 1;
        if (namep[1] == '.' && namep[2] == 0)
            return 1;
        return 0;
    }
    while (tc = *namep++) {
        if (tc == '.') {
            /* saw another dot */
            if (sawDot) return 0;	/* second dot */
            sawDot = 1;
            charCount = 0;
            continue;
        }
        if (!ISLEGALCHAR(tc))
            return 0;
        charCount++;
        if (!sawDot && charCount > 8)
            /* more than 8 chars in name */
            return 0;
        if (sawDot && charCount > 3)
            /* more than 3 chars in extension */
            return 0;
    }
    return 1;
}

/*
 * Number unparsing map for generating 8.3 names;
 * The version taken from DFS was on drugs.
 * You can't include '&' and '@' in a file name.
 */
char cm_8Dot3Mapping[42] =
{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J', 'K',
 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U',
 'V', 'W', 'X', 'Y', 'Z', '_', '-', '$', '#', '!', '{', '}'
};
int cm_8Dot3MapSize = sizeof(cm_8Dot3Mapping);

void cm_Gen8Dot3NameInt(const fschar_t * longname, cm_dirFid_t * pfid,
                        clientchar_t *shortName, clientchar_t **shortNameEndp)
{
    char number[12];
    int i, nsize = 0;
    int vnode = ntohl(pfid->vnode);
    char *lastDot;
    int validExtension = 0;
    char tc, *temp;
    const char *name;

    /* Unparse the file's vnode number to get a "uniquifier" */
    do {
        number[nsize] = cm_8Dot3Mapping[vnode % cm_8Dot3MapSize];
        nsize++;
        vnode /= cm_8Dot3MapSize;
    } while (vnode);

    /*
     * Look for valid extension.  There has to be a dot, and
     * at least one of the characters following has to be legal.
     */
    lastDot = strrchr(longname, '.');
    if (lastDot) {
        temp = lastDot; temp++;
        while (tc = *temp++)
            if (ISLEGALCHAR(tc))
                break;
        if (tc)
            validExtension = 1;
    }

    /* Copy name characters */
    for (i = 0, name = longname;
          i < (7 - nsize) && name != lastDot; ) {
        tc = *name++;

        if (tc == 0)
            break;
        if (!ISLEGALCHAR(tc))
            continue;
        i++;
        *shortName++ = toupper(tc);
    }

    /* tilde */
    *shortName++ = '~';

    /* Copy uniquifier characters */
    for (i=0; i < nsize; i++) {
        *shortName++ = number[i];
    }

    if (validExtension) {
        /* Copy extension characters */
        *shortName++ = *lastDot++;	/* copy dot */
        for (i = 0, tc = *lastDot++;
             i < 3 && tc;
             tc = *lastDot++) {
            if (ISLEGALCHAR(tc)) {
                i++;
                *shortName++ = toupper(tc);
            }
        }
    }

    /* Trailing null */
    *shortName = 0;

    if (shortNameEndp)
        *shortNameEndp = shortName;
}

void cm_Gen8Dot3NameIntW(const clientchar_t * longname, cm_dirFid_t * pfid,
                         clientchar_t *shortName, clientchar_t **shortNameEndp)
{
    clientchar_t number[12];
    int i, nsize = 0;
    int vnode = ntohl(pfid->vnode);
    clientchar_t *lastDot;
    int validExtension = 0;
    clientchar_t tc, *temp;
    const clientchar_t *name;

    /* Unparse the file's vnode number to get a "uniquifier" */
    do {
        number[nsize] = cm_8Dot3Mapping[vnode % cm_8Dot3MapSize];
        nsize++;
        vnode /= cm_8Dot3MapSize;
    } while (vnode);

    /*
     * Look for valid extension.  There has to be a dot, and
     * at least one of the characters following has to be legal.
     */
    lastDot = cm_ClientStrRChr(longname, '.');
    if (lastDot) {
        temp = lastDot; temp++;
        while (tc = *temp++)
            if (ISLEGALCHAR(tc))
                break;
        if (tc)
            validExtension = 1;
    }

    /* Copy name characters */
    for (i = 0, name = longname;
          i < (7 - nsize) && name != lastDot; ) {
        tc = *name++;

        if (tc == 0)
            break;
        if (!ISLEGALCHAR(tc))
            continue;
        i++;
        *shortName++ = toupper((char) tc);
    }

    /* tilde */
    *shortName++ = '~';

    /* Copy uniquifier characters */
    for (i=0; i < nsize; i++) {
        *shortName++ = number[i];
    }

    if (validExtension) {
        /* Copy extension characters */
        *shortName++ = *lastDot++;	/* copy dot */
        for (i = 0, tc = *lastDot++;
             i < 3 && tc;
             tc = *lastDot++) {
            if (ISLEGALCHAR(tc)) {
                i++;
                *shortName++ = toupper(tc);
            }
        }
    }

    /* Trailing null */
    *shortName = 0;

    if (shortNameEndp)
        *shortNameEndp = shortName;
}

/*! \brief Compare 'pattern' (containing metacharacters '*' and '?') with the file name 'name'.

  \note This procedure works recursively calling itself.

  \param[in] pattern string containing metacharacters.
  \param[in] name File name to be compared with 'pattern'.

  \return BOOL : TRUE/FALSE (match/mistmatch)
*/
static BOOL
szWildCardMatchFileName(clientchar_t * pattern, clientchar_t * name, int casefold)
{
    clientchar_t upattern[MAX_PATH];
    clientchar_t uname[MAX_PATH];

    clientchar_t * pename;         // points to the last 'name' character
    clientchar_t * p;
    clientchar_t * pattern_next;

    if (casefold) {
        cm_ClientStrCpy(upattern, lengthof(upattern), pattern);
        cm_ClientStrUpr(upattern);
        pattern = upattern;

        cm_ClientStrCpy(uname, lengthof(uname), name);
        cm_ClientStrUpr(uname);
        name = uname;

        /* The following translations all work on single byte
           characters */
        for (p=upattern; *p; p++) {
            if (*p == '"') *p = '.'; continue;
            if (*p == '<') *p = '*'; continue;
            if (*p == '>') *p = '?'; continue;
        }

        for (p=uname; *p; p++) {
            if (*p == '"') *p = '.'; continue;
            if (*p == '<') *p = '*'; continue;
            if (*p == '>') *p = '?'; continue;
        }
    }

    pename = cm_ClientCharThis(name + cm_ClientStrLen(name));

    while (*name) {
        switch (*pattern) {
        case '?':
	    pattern = cm_ClientCharNext(pattern);
            if (*name == '.')
		continue;
            name = cm_ClientCharNext(name);
            break;

         case '*':
            pattern = cm_ClientCharNext(pattern);
            if (*pattern == '\0')
                return TRUE;

            pattern_next = cm_ClientCharNext(pattern);

            for (p = pename; p >= name; p = cm_ClientCharPrev(p)) {
                if (*p == *pattern &&
                    szWildCardMatchFileName(pattern_next,
                                            cm_ClientCharNext(p), FALSE))
                    return TRUE;
            } /* endfor */
            if (*pattern == '.' && *pattern_next == '\0') {
                for (p = name; p && *p; p = cm_ClientCharNext(p))
                    if (*p == '.')
                        break;
                if (p && *p)
                    return FALSE;
                return TRUE;
            }
            return FALSE;

        default:
            if (*name != *pattern)
                return FALSE;
            pattern = cm_ClientCharNext(pattern);
            name = cm_ClientCharNext(name);
            break;
        } /* endswitch */
    } /* endwhile */

    /* if all we have left are wildcards, then we match */
    for (;*pattern; pattern = cm_ClientCharNext(pattern)) {
	if (*pattern != '*' && *pattern != '?')
	    return FALSE;
    }
    return TRUE;
}

/* do a case-folding search of the star name mask with the name in namep.
 * Return 1 if we match, otherwise 0.
 */
int cm_MatchMask(clientchar_t *namep, clientchar_t *maskp, int flags)
{
    clientchar_t *newmask, lastchar = _C('\0');
    int    i, j, casefold, retval;
    int  star = 0, qmark = 0, dot = 0;

    /* make sure we only match 8.3 names, if requested */
    if ((flags & CM_FLAG_8DOT3) && !cm_Is8Dot3(namep))
        return 0;

    casefold = (flags & CM_FLAG_CASEFOLD) ? 1 : 0;

    /* optimize the pattern:
     * if there is a mixture of '?' and '*',
     * for example  the sequence "*?*?*?*"
     * must be turned into the form "*"
     */
    newmask = (clientchar_t *)malloc((cm_ClientStrLen(maskp)+2)*sizeof(clientchar_t));
    for ( i=0, j=0, star=0, qmark=0; maskp[i]; i++) {
        lastchar = maskp[i];
        switch ( maskp[i] ) {
        case '?':
        case '>':
            qmark++;
            break;
        case '<':
        case '*':
            star++;
            break;
        case '.':
            dot++;
            /* fallthrough */
        default:
            if ( star ) {
                newmask[j++] = '*';
            } else if ( qmark ) {
                while ( qmark-- )
                    newmask[j++] = '?';
            }
            newmask[j++] = maskp[i];
            star = 0;
            qmark = 0;
        }
    }
    if ( star ) {
        newmask[j++] = '*';
    } else if ( qmark ) {
        while ( qmark-- )
            newmask[j++] = '?';
    }
    if (dot == 0 && lastchar == '<')
        newmask[j++] = '.';
    newmask[j++] = '\0';

    retval = szWildCardMatchFileName(newmask, namep, casefold) ? 1:0;

    free(newmask);
    return retval;
}

BOOL
cm_TargetPerceivedAsDirectory(const fschar_t *target)
{
    char        * ext;

    ext = PathFindExtension(target);
    if (!ext[0])
        return TRUE;

    return FALSE;
}

HANDLE
cm_LoadAfsdHookLib(void)
{
    char dllname[260];
    char *p;
    HANDLE hLib;

    if (!GetModuleFileName(NULL, dllname, sizeof(dllname)))
        return NULL;

    p = strrchr(dllname, '\\');
    if (p) {
        p++;
        strcpy(p, AFSD_HOOK_DLL);
        hLib = LoadLibrary(dllname);
    } else {
        hLib = LoadLibrary(AFSD_HOOK_DLL);
    }

    return hLib;
}

/*
 * Obtain the file info structure for the specified file.
 * If a full path is not specified, the search order is the
 * same as that used by LoadLibrary().
 */
BOOL
cm_GetOSFileVersion (char *filename, LARGE_INTEGER *liVer)
{
    DWORD dwHandle;
    DWORD dwSize;
    char* pInfo = NULL;
    BOOL  rc;
    UINT uLen;
    void *pbuf;
    VS_FIXEDFILEINFO vsf;

    dwSize = GetFileVersionInfoSizeA(filename,&dwHandle);
    if (dwSize == 0) {
        rc = FALSE;
        goto done;
    }
    pInfo = (char*)malloc(dwSize);
    if (!pInfo) {
        rc = FALSE;
        goto done;
    }
    rc = GetFileVersionInfoA(filename, dwHandle, dwSize, pInfo);
    if (!rc)
        goto done;
    rc = VerQueryValueA(pInfo,"\\",&pbuf, &uLen);
    if (!rc)
        goto done;
    memcpy(&vsf, pbuf, sizeof(VS_FIXEDFILEINFO));

    liVer->LowPart = vsf.dwFileVersionLS;
    liVer->HighPart = vsf.dwFileVersionMS;
    rc = TRUE;

  done:
    if (pInfo)
        free(pInfo);
    return rc;
}

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
typedef BOOL (WINAPI *LPFN_DISABLEWOW64FSREDIRECTION) (PVOID *);
typedef BOOL (WINAPI *LPFN_REVERTWOW64FSREDIRECTION) (PVOID);

BOOL msftSMBRedirectorSupportsExtendedTimeouts(void)
{
    static BOOL fChecked = FALSE;
    static BOOL fSupportsExtendedTimeouts = FALSE;

    if (!fChecked)
    {
        BOOL isWow64 = FALSE;
        OSVERSIONINFOEX Version;
        HANDLE h1 = NULL;
        LPFN_ISWOW64PROCESS fnIsWow64Process = NULL;
        LPFN_DISABLEWOW64FSREDIRECTION fnDisableWow64FsRedirection = NULL;
        LPFN_REVERTWOW64FSREDIRECTION fnRevertWow64FsRedirection = NULL;
        PVOID Wow64RedirectionState;
        LARGE_INTEGER fvFile, fvHotFixMin;

        h1 = GetModuleHandle("kernel32.dll"); /* no refcount increase */
        /*
         * If we don't find the fnIsWow64Process function then we
         * are not running in a Wow64 environment
         */
        fnIsWow64Process =
            (LPFN_ISWOW64PROCESS)GetProcAddress(h1, "IsWow64Process");

        memset (&Version, 0x00, sizeof(Version));
        Version.dwOSVersionInfoSize = sizeof(Version);
        GetVersionEx((OSVERSIONINFO *) &Version);

        /*
         * Support is available as hot fixes / service packs on:
         *   XP SP2
         *   XP SP3
         *   2003 and XP64 SP2
         *   Vista and 2008 SP2
         *   Win7 and 2008 R2
         */
        if (Version.dwPlatformId == VER_PLATFORM_WIN32_NT &&
            Version.dwMajorVersion >= 5) {

            /* 32-bit XP */
            if (Version.dwMajorVersion == 5 &&
                Version.dwMinorVersion == 1) {

                fvHotFixMin.HighPart = (5 << 16) | 1;

                switch (Version.wServicePackMajor) {
                case 3:
                    fvHotFixMin.LowPart = (2600 << 16) | 5815;
                    break;
                case 2:
                    fvHotFixMin.LowPart = (2600 << 16) | 3572;
                    break;
                default:
                    fSupportsExtendedTimeouts = (Version.wServicePackMajor > 3);
                    goto checked;
                }
            }

            /* 64-bit XP and Server 2003 */
            else if (Version.dwMajorVersion == 5 &&
                     Version.dwMinorVersion == 2) {

                fvHotFixMin.HighPart = (5 << 16) | 2;

                switch (Version.wServicePackMajor) {
                case 2:
                    fvHotFixMin.LowPart = (3790 << 16) | 4479;
                    break;
                case 1:
                    fvHotFixMin.LowPart = (3790 << 16) | 3310;
                    break;
                default:
                    fSupportsExtendedTimeouts = (Version.wServicePackMajor > 2);
                    goto checked;
                }
            }

            /* Vista and Server 2008 */
            else if (Version.dwMajorVersion == 6 &&
                     Version.dwMinorVersion == 0) {

                fvHotFixMin.HighPart = (6 << 16) | 0;

                switch (Version.wServicePackMajor) {
                case 2:
                    fvHotFixMin.LowPart = (6002 << 16) | 18005;
                    break;
                default:
                    fSupportsExtendedTimeouts = (Version.wServicePackMajor > 2);
                    goto checked;
                }
            }

            /* Windows 7 and Server 2008 R2 and beyond */
            else if (Version.dwMajorVersion > 6 ||
                     Version.dwMajorVersion == 6 &&
                     Version.dwMinorVersion >= 1) {
                fSupportsExtendedTimeouts = TRUE;
                goto checked;
            }

            /* If wow64, disable wow64 redirection and preserve the existing state */
            if (fnIsWow64Process &&
                 fnIsWow64Process(GetCurrentProcess(), &isWow64) &&
                 isWow64) {
                fnDisableWow64FsRedirection =
                    (LPFN_DISABLEWOW64FSREDIRECTION)GetProcAddress(h1, "Wow64DisableWow64FsRedirection");
                fnRevertWow64FsRedirection =
                    (LPFN_REVERTWOW64FSREDIRECTION)GetProcAddress(h1, "Wow64RevertWow64FsRedirection");
                fnDisableWow64FsRedirection(&Wow64RedirectionState);
            }

            if (cm_GetOSFileVersion("drivers\\mrxsmb.sys", &fvFile) ||
                (fvFile.QuadPart >= fvHotFixMin.QuadPart))
                fSupportsExtendedTimeouts = TRUE;

            /* If wow64, restore the previous redirection state */
            if (fnIsWow64Process && isWow64) {
                fnRevertWow64FsRedirection(Wow64RedirectionState);
            }
        }
      checked:
        fChecked = TRUE;
    }

    return fSupportsExtendedTimeouts;
}

void cm_ResetServerPriority()
{
    void * p = TlsGetValue(cm_TlsRequestSlot);
    if (p) {
	free(p);
	TlsSetValue(cm_TlsRequestSlot, NULL);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    }
}

void cm_SetRequestStartTime()
{
    time_t * tp = TlsGetValue(cm_TlsRequestSlot);
    if (!tp)
	tp = malloc(sizeof(time_t));
    if (tp) {
	*tp = osi_Time();

	if (!TlsSetValue(cm_TlsRequestSlot, tp))
	    free(tp);
    }
}

void cm_UpdateServerPriority()
{
    time_t *tp = TlsGetValue(cm_TlsRequestSlot);

    if (tp) {
	time_t now = osi_Time();

	/* Give one priority boost for each 15 seconds */
	SetThreadPriority(GetCurrentThread(), (int)((now - *tp) / 15));
    }
}


void cm_LargeSearchTimeFromUnixTime(FILETIME *largeTimep, time_t unixTime)
{
    // Note that LONGLONG is a 64-bit value
    LONGLONG ll;

    ll = Int32x32To64(unixTime, 10000000) + 116444736000000000;
    largeTimep->dwLowDateTime = (DWORD)(ll & 0xFFFFFFFF);
    largeTimep->dwHighDateTime = (DWORD)(ll >> 32);
}

void cm_UnixTimeFromLargeSearchTime(time_t *unixTimep, FILETIME *largeTimep)
{
    // Note that LONGLONG is a 64-bit value
    LONGLONG ll;

    ll = largeTimep->dwHighDateTime;
    ll <<= 32;
    ll += largeTimep->dwLowDateTime;

    ll -= 116444736000000000;
    ll /= 10000000;

    *unixTimep = (DWORD)ll;
}

void cm_SearchTimeFromUnixTime(afs_uint32 *searchTimep, time_t unixTime)
{
    struct tm *ltp;
    int dosDate;
    int dosTime;
    struct tm localJunk;
    time_t t = unixTime;

    ltp = localtime(&t);

    /* if we fail, make up something */
    if (!ltp) {
        ltp = &localJunk;
        localJunk.tm_year = 89 - 20;
        localJunk.tm_mon = 4;
        localJunk.tm_mday = 12;
        localJunk.tm_hour = 0;
        localJunk.tm_min = 0;
        localJunk.tm_sec = 0;
    }

    dosDate = ((ltp->tm_year-80)<<9) | ((ltp->tm_mon+1) << 5) | (ltp->tm_mday);
    dosTime = (ltp->tm_hour<<11) | (ltp->tm_min << 5) | (ltp->tm_sec / 2);
    *searchTimep = (dosDate<<16) | dosTime;
}

void cm_UnixTimeFromSearchTime(time_t *unixTimep, afs_uint32 searchTime)
{
    unsigned short dosDate;
    unsigned short dosTime;
    struct tm localTm;

    dosDate = (unsigned short) (searchTime & 0xffff);
    dosTime = (unsigned short) ((searchTime >> 16) & 0xffff);

    localTm.tm_year = 80 + ((dosDate>>9) & 0x3f);
    localTm.tm_mon = ((dosDate >> 5) & 0xf) - 1;	/* January is 0 in localTm */
    localTm.tm_mday = (dosDate) & 0x1f;
    localTm.tm_hour = (dosTime>>11) & 0x1f;
    localTm.tm_min = (dosTime >> 5) & 0x3f;
    localTm.tm_sec = (dosTime & 0x1f) * 2;
    localTm.tm_isdst = -1;				/* compute whether DST in effect */

    *unixTimep = mktime(&localTm);
}
