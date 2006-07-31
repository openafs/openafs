/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */

#ifndef UKERNEL
#if !defined(AFS_LINUX20_ENV) && !defined(AFS_FBSD_ENV)
#include <net/if.h>
#include <netinet/in.h>
#endif

#ifdef AFS_SGI62_ENV
#include "h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_FBSD_ENV) && !defined(AFS_DARWIN60_ENV)
#include <netinet/in_var.h>
#endif
#endif /* !UKERNEL */

#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */
#include "afs/afs_util.h"
#include "afs/unified_afs.h"

#if	defined(AFS_SUN56_ENV)
#include <inet/led.h>
#include <inet/common.h>
#if     defined(AFS_SUN58_ENV)
#include <netinet/ip6.h>
#endif
#include <inet/ip.h>
#endif


/* shouldn't do it this way, but for now will do */
#ifndef ERROR_TABLE_BASE_U
#define ERROR_TABLE_BASE_U	(5376L)
#endif /* ubik error base define */

/* shouldn't do it this way, but for now will do */
#ifndef ERROR_TABLE_BASE_uae
#define ERROR_TABLE_BASE_uae	(49733376L)
#endif /* unified afs error base define */

/* same hack for vlserver error base as for ubik error base */
#ifndef ERROR_TABLE_BASE_VL
#define ERROR_TABLE_BASE_VL	(363520L)
#define VL_NOENT		(363524L)
#endif /* vlserver error base define */


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

afs_int32
et_to_sys_error(afs_int32 in)
{
    if (in < ERROR_TABLE_BASE_uae || in >= ERROR_TABLE_BASE_uae + 512)
	return in;
    if (et2sys[in - ERROR_TABLE_BASE_uae] != 0)
	return et2sys[in - ERROR_TABLE_BASE_uae];
    return in;
}

void
afs_CopyError(register struct vrequest *afrom, register struct vrequest *ato)
{
    AFS_STATCNT(afs_CopyError);
    if (!afrom->initd)
	return;
    afs_FinalizeReq(ato);
    if (afrom->accessError)
	ato->accessError = 1;
    if (afrom->volumeError)
	ato->volumeError = 1;
    if (afrom->networkError)
	ato->networkError = 1;
    if (afrom->permWriteError)
	ato->permWriteError = 1;

}

void
afs_FinalizeReq(register struct vrequest *areq)
{
    AFS_STATCNT(afs_FinalizeReq);
    if (areq->initd)
	return;
    areq->busyCount = 0;
    areq->accessError = 0;
    areq->volumeError = 0;
    areq->networkError = 0;
    areq->permWriteError = 0;
    areq->initd = 1;

}

int
afs_CheckCode(afs_int32 acode, struct vrequest *areq, int where)
{
    AFS_STATCNT(afs_CheckCode);
    if (acode) {
	afs_Trace2(afs_iclSetp, CM_TRACE_CHECKCODE, ICL_TYPE_INT32, acode,
		   ICL_TYPE_INT32, where);
    }
    if ((acode & ~0xff) == ERROR_TABLE_BASE_uae)
	acode = et_to_sys_error(acode);
    if (!areq || !areq->initd)
	return acode;
    if (areq->networkError)
	return ETIMEDOUT;
    if (acode == 0)
	return 0;
    if (areq->accessError)
	return EACCES;
    if (areq->volumeError == VOLMISSING)
	return ENODEV;
    if (areq->volumeError == VOLBUSY)
	return EWOULDBLOCK;
    if (acode == VNOVNODE)
	return ENOENT;
    if (acode == VDISKFULL)
	return ENOSPC;
    if (acode == VOVERQUOTA)
	return
#ifdef EDQUOT
	    EDQUOT
#else
	    ENOSPC
#endif
	    ;

    return acode;

}				/*afs_CheckCode */
