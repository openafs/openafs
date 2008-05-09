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
#ifndef DJGPP
#include <windows.h>
#include <winsock2.h>
#ifndef EWOULDBLOCK
#define EWOULDBLOCK             WSAEWOULDBLOCK
#define EINPROGRESS             WSAEINPROGRESS
#define EALREADY                WSAEALREADY
#define ENOTSOCK                WSAENOTSOCK
#define EDESTADDRREQ            WSAEDESTADDRREQ
#define EMSGSIZE                WSAEMSGSIZE
#define EPROTOTYPE              WSAEPROTOTYPE
#define ENOPROTOOPT             WSAENOPROTOOPT
#define EPROTONOSUPPORT         WSAEPROTONOSUPPORT
#define ESOCKTNOSUPPORT         WSAESOCKTNOSUPPORT
#define EOPNOTSUPP              WSAEOPNOTSUPP
#define EPFNOSUPPORT            WSAEPFNOSUPPORT
#define EAFNOSUPPORT            WSAEAFNOSUPPORT
#define EADDRINUSE              WSAEADDRINUSE
#define EADDRNOTAVAIL           WSAEADDRNOTAVAIL
#define ENETDOWN                WSAENETDOWN
#define ENETUNREACH             WSAENETUNREACH
#define ENETRESET               WSAENETRESET
#define ECONNABORTED            WSAECONNABORTED
#define ECONNRESET              WSAECONNRESET
#define ENOBUFS                 WSAENOBUFS
#define EISCONN                 WSAEISCONN
#define ENOTCONN                WSAENOTCONN
#define ESHUTDOWN               WSAESHUTDOWN
#define ETOOMANYREFS            WSAETOOMANYREFS
#define ETIMEDOUT               WSAETIMEDOUT
#define ECONNREFUSED            WSAECONNREFUSED
#ifdef ELOOP
#undef ELOOP
#endif
#define ELOOP                   WSAELOOP
#ifdef ENAMETOOLONG
#undef ENAMETOOLONG
#endif
#define ENAMETOOLONG            WSAENAMETOOLONG
#define EHOSTDOWN               WSAEHOSTDOWN
#define EHOSTUNREACH            WSAEHOSTUNREACH
#ifdef ENOTEMPTY
#undef ENOTEMPTY
#endif 
#define ENOTEMPTY               WSAENOTEMPTY
#define EPROCLIM                WSAEPROCLIM
#define EUSERS                  WSAEUSERS
#define EDQUOT                  WSAEDQUOT
#define ESTALE                  WSAESTALE
#define EREMOTE                 WSAEREMOTE
#endif /* EWOULDBLOCK */
#endif /* !DJGPP */
#include <afs/unified_afs.h>

#include <string.h>
#include <malloc.h>
#include "afsd.h"
#include <osi.h>
#include <rx/rx.h>

#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>


static osi_once_t cm_utilsOnce;

osi_rwlock_t cm_utilsLock;

cm_space_t *cm_spaceListp;

static int et2sys[512];

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

    if (error < 0) 
        error = CM_ERROR_TIMEDOUT;
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

    if (error < 0) 
        error = CM_ERROR_TIMEDOUT;
    else if (error == EROFS) 
        error = CM_ERROR_READONLY;
    else if (error == ENOTDIR) 
        error = CM_ERROR_NOTDIR;
    else if (error == EACCES) 
        error = CM_ERROR_NOACCESS;
    else if (error == ENOENT) 
        error = CM_ERROR_NOSUCHFILE;
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

    if (error < 0) 
	error = CM_ERROR_TIMEDOUT;
    else if (error == VL_NOENT) 
	error = CM_ERROR_NOSUCHVOLUME;
    return error;
}

cm_space_t *cm_GetSpace(void)
{
	cm_space_t *tsp;

	if (osi_Once(&cm_utilsOnce)) {
		lock_InitializeRWLock(&cm_utilsLock, "cm_utilsLock");
		osi_EndOnce(&cm_utilsOnce);
        }
        
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

/* This is part of the Microsoft Internationalized Domain Name
   Mitigation APIs. */
#include <normalization.h>

int
(WINAPI *pNormalizeString)( __in NORM_FORM NormForm,
                            __in_ecount(cwSrcLength) LPCWSTR lpSrcString,
                            __in int cwSrcLength,
                            __out_ecount(cwDstLength) LPWSTR lpDstString,
                            __in int cwDstLength ) = NULL;

BOOL
(WINAPI *pIsNormalizedString)( __in NORM_FORM NormForm,
                               __in_ecount(cwLength) LPCWSTR lpString,
                               __in int cwLength ) = NULL;


#define NLSDLLNAME "Normaliz.dll"
#define NLSMAXCCH  1024
#define NLSERRCCH  8

#define AFS_NORM_FORM NormalizationC

long cm_InitNormalization(void)
{
    HMODULE h_Nls;

    if (pNormalizeString != NULL)
        return 0;

    h_Nls = LoadLibrary(NLSDLLNAME);
    if (h_Nls == INVALID_HANDLE_VALUE) {
        afsi_log("Can't load " NLSDLLNAME ": LastError=%d", GetLastError());
        return 1;
    }

    pNormalizeString = GetProcAddress(h_Nls, "NormalizeString");
    pIsNormalizedString = GetProcAddress(h_Nls, "IsNormalizedString");

    return (pNormalizeString && pIsNormalizedString);
}

/* \brief Normalize a UTF-16 string.

   If the supplied destination buffer is
   insufficient or NULL, then a new buffer will be allocated to hold
   the normalized string.

   \param[in] src : Source UTF-16 string.  Length is specified in
       cch_src.

   \param[in] cch_src : The character count in cch_src is assumed to
       be tight and include the terminating NULL character if there is
       one.  If the NULL is absent, the resulting string will not be
       NULL terminated.

   \param[out] ext_dest : The destination buffer.  Can be NULL, in
       which case *pcch_dest MUST be NULL.

   \param[in,out] pcch_dest : On entry *pcch_dest contains a count of
       characters in the destination buffer.  On exit, it will contain
       a count of characters that were copied to the destination
       buffer.

   Returns a pointer to the buffer containing the normalized string or
   NULL if the call was unsuccessful.  If the returned destination
   buffer is different fron the supplied buffer and non-NULL, it
   should be freed using free().
*/
static wchar_t * 
NormalizeUtf16String(const wchar_t * src, int cch_src, wchar_t * ext_dest, int *pcch_dest)
{
    if ((pIsNormalizedString && (*pIsNormalizedString)(AFS_NORM_FORM, src, cch_src)) ||
        (!pNormalizeString)) {

        int rv;
        DWORD gle;
        int tries = 10;
        wchar_t * dest;
        int cch_dest = *pcch_dest;

        dest = ext_dest;

        while (tries-- > 0) {

            rv = (*pNormalizeString)(AFS_NORM_FORM, src, cch_src, dest, cch_dest);

            if (rv <= 0 && (gle = GetLastError()) != ERROR_SUCCESS) {
#ifdef DEBUG
                osi_Log1(afsd_logp, "NormalizeUtf16String error = %d", gle);
#endif
                if (gle == ERROR_INSUFFICIENT_BUFFER) {

                    /* The buffer wasn't big enough.  We are going to
                       try allocating one. */

                    cch_dest = (-rv) + NLSERRCCH;
                    goto cont;

                } else {
                    /* Something else is wrong */
                    break;
                }

            } else if (rv < 0) { /* rv < 0 && gle == ERROR_SUCCESS */

                /* Technically not one of the expected outcomes */
                break;

            } else {            /* rv > 0 || (rv == 0 && gle == ERROR_SUCCESS) */

                /* Possibly succeeded */

                if (rv == 0) { /* Succeeded and the return string is empty */
                    *pcch_dest = 0;
                    return dest;
                }

                if (cch_dest == 0) {
                    /* Nope.  We only calculated the required size of the buffer */

                    cch_dest = rv + NLSERRCCH;
                    goto cont;
                }

                *pcch_dest = rv;

                /* Success! */
                return dest;
            }

        cont:
            if (dest != ext_dest && dest)
                free(dest);
            dest = malloc(cch_dest * sizeof(wchar_t));
        }

        /* Failed */

        if (dest != ext_dest && dest)
            free(dest);

        *pcch_dest = 0;
        return NULL;
    } else {

        /* No need to or unable to normalize.  Just copy the string */
        if (SUCCEEDED(StringCchCopyNW(ext_dest, *pcch_dest, src, cch_src))) {
            *pcch_dest = cch_src;
            return ext_dest;
        } else {
            *pcch_dest = 0;
            return NULL;
        }
    }
}

/* \brief Normalize a UTF-16 string into a UTF-8 string.

   \param[in] src : Source string.

   \param[in] cch_src : Count of characters in src. If the count includes the
       NULL terminator, then the resulting string will be NULL
       terminated.  If it is -1, then src is assumed to be NULL
       terminated.

   \param[out] adest : Destination buffer.

   \param[in] cch_adest : Number of characters in the destination buffer.

   Returns the number of characters stored into cch_adest. This will
   include the terminating NULL if cch_src included the terminating
   NULL or was -1.  If this is 0, then the operation was unsuccessful.
 */
long cm_NormalizeUtf16StringToUtf8(const wchar_t * src, int cch_src,
                                   char * adest, int cch_adest)
{
    if (cch_src < 0) {
        size_t cch;

        if (FAILED(StringCchLengthW(src, NLSMAXCCH, &cch)))
            return CM_ERROR_TOOBIG;

        cch_src = cch+1;
    }

    {
        wchar_t nbuf[NLSMAXCCH];
        wchar_t * normalized;
        int cch_norm = NLSMAXCCH;

        normalized = NormalizeUtf16String(src, cch_src, nbuf, &cch_norm);
        if (normalized) {
            cch_adest = WideCharToMultiByte(CP_UTF8, 0, normalized, cch_norm,
                                            adest, cch_adest, NULL, 0);

            if (normalized != nbuf && normalized)
                free(normalized);

            return cch_adest;

        } else {

            return 0;

        }
    }
}


/* \brief Normalize a UTF-8 string.

   \param[in] src String to normalize.

   \param[in] cch_src : Count of characters in src.  If this value is
       -1, then src is assumed to be NULL terminated.  The translated
       string will be NULL terminated only if this is -1 or the count
       includes the terminating NULL.

   \param[out] adest : Destination string.

   \param[in] cch_adest : Number of characters in the destination
       string.

   Returns the number of characters stored into adest or 0 if the call
   was unsuccessful.
 */
long cm_NormalizeUtf8String(const char * src, int cch_src,
                            char * adest, int cch_adest)
{
    wchar_t wsrcbuf[NLSMAXCCH];
    wchar_t *wnorm;
    int cch;
    int cch_norm;

    /* Get some edge cases out first, so we don't have to worry about
       cch_src being 0 etc. */
    if (cch_src == 0) {
        return 0;
    } else if (*src == '\0') {
        *adest = '\0';
        return 1;
    }

    cch = MultiByteToWideChar(CP_UTF8, 0, src, cch_src * sizeof(char),
                             wsrcbuf, NLSMAXCCH);

    if (cch == 0) {
#ifdef DEBUG
        DebugBreak();
#endif
        return 0;
    }

    cch_norm = 0;
    wnorm = NormalizeUtf16String(wsrcbuf, cch, NULL, &cch_norm);
    if (wnorm == NULL) {
#ifdef DEBUG
        DebugBreak();
#endif
        return 0;
    }

    cch = WideCharToMultiByte(CP_UTF8, 0, wnorm, cch_norm,
                              adest, cch_adest * sizeof(char),
                              NULL, FALSE);

    if (wnorm)
        free(wnorm);

    return cch;
}
