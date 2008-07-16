/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * afs_warn.c - afs_warn
 *
 * Implements: afs_warn, afs_warnuser
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/afs_warn.c,v 1.1.2.2 2008/07/01 03:35:23 shadow Exp $");

#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */

#if !defined(UKERNEL)
#if !defined(AFS_LINUX20_ENV)
#include <net/if.h>
#endif
#include <netinet/in.h>

#ifdef AFS_SGI62_ENV
#include "h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN60_ENV)
#include <netinet/in_var.h>
#endif /* ! AFS_HPUX110_ENV */
#endif /* !defined(UKERNEL) */

#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

#if	defined(AFS_SUN56_ENV)
#include <inet/led.h>
#include <inet/common.h>
#if     defined(AFS_SUN58_ENV)
#include <netinet/ip6.h>
#endif
#include <inet/ip.h>
#endif

#if	defined(AFS_AIX_ENV)
#include <sys/fp_io.h>
#endif



/* * * * * * *
 * this code badly needs to be cleaned up...  too many ugly ifdefs.
 * XXX
 */
#if 0
void
afs_warn(char *a, long b, long c, long d, long e, long f, long g, long h,
	 long i, long j)
#else
void
afs_warn(a, b, c, d, e, f, g, h, i, j)
     char *a;
#if defined( AFS_USE_VOID_PTR)
     void *b, *c, *d, *e, *f, *g, *h, *i, *j;
#else
     long b, c, d, e, f, g, h, i, j;
#endif
#endif
{
    AFS_STATCNT(afs_warn);

    if (afs_showflags & GAGCONSOLE) {
#if defined(AFS_AIX_ENV)
	struct file *fd;

	/* cf. console_printf() in oncplus/kernext/nfs/serv/shared.c */
	if (fp_open
	    ("/dev/console", O_WRONLY | O_NOCTTY | O_NDELAY, 0666, 0, FP_SYS,
	     &fd) == 0) {
	    char buf[1024];
	    ssize_t len;
	    ssize_t count;

	    sprintf(buf, a, b, c, d, e, f, g, h, i, j);
	    len = strlen(buf);
	    fp_write(fd, buf, len, 0, UIO_SYSSPACE, &count);
	    fp_close(fd);
	}
#else
	printf(a, b, c, d, e, f, g, h, i, j);
#endif
    }
}

#if 0
void
afs_warnuser(char *a, long b, long c, long d, long e, long f, long g, long h,
	     long i, long j)
#else
void
afs_warnuser(a, b, c, d, e, f, g, h, i, j)
     char *a;
     long b, c, d, e, f, g, h, i, j;
#endif
{
    AFS_STATCNT(afs_warnuser);
    if (afs_showflags & GAGUSER) {
#ifdef AFS_GLOBAL_SUNLOCK
	int haveGlock = ISAFS_GLOCK();
	if (haveGlock)
	    AFS_GUNLOCK();
#endif /* AFS_GLOBAL_SUNLOCK */

	uprintf(a, b, c, d, e, f, g, h, i, j);

#ifdef AFS_GLOBAL_SUNLOCK
	if (haveGlock)
	    AFS_GLOCK();
#endif /* AFS_GLOBAL_SUNLOCK */
    }
}
