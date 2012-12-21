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


#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */

#if !defined(UKERNEL)

#if !defined(AFS_LINUX20_ENV)
# include <net/if.h>
# if defined(AFS_SUN58_ENV)
#  include <sys/varargs.h>
# else
#  include <stdarg.h>
# endif
#endif
#include <netinet/in.h>

#ifdef AFS_SGI62_ENV
#include "h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN_ENV)
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

#if defined(AFS_LINUX26_ENV)
# define afs_vprintf(fmt, ap) vprintk(fmt, ap)
#elif defined(AFS_SGI_ENV)
# define afs_vprintf(fmt, ap) icmn_err(CE_WARN, fmt, ap)
#elif (defined(AFS_DARWIN80_ENV) && !defined(AFS_DARWIN90_ENV)) || (defined(AFS_LINUX22_ENV))
static_inline void
afs_vprintf(const char *fmt, va_list ap)
{
    char buf[256];

    vsnprintf(buf, sizeof(buf), fmt, ap);
    printf(buf);
}
#else
# define afs_vprintf(fmt, ap) vprintf(fmt, ap)
#endif

#ifdef AFS_AIX_ENV
void
afs_warn(fmt, a, b, c, d, e, f, g, h, i)
    char *fmt;
    void *a, *b, *c, *d, *e, *f, *g, *h, *i;
#else
void
afs_warn(char *fmt, ...)
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

	    sprintf(buf, fmt, a, b, c, d, e, f, g, h, i);
	    len = strlen(buf);
	    fp_write(fd, buf, len, 0, UIO_SYSSPACE, &count);
	    fp_close(fd);
	}
#else
	va_list ap;

	va_start(ap, fmt);
	afs_vprintf(fmt, ap);
	va_end(ap);
#endif
    }
}

#ifdef AFS_AIX_ENV
void
afs_warnuser(fmt, a, b, c, d, e, f, g, h, i)
    char *fmt;
    void *a, *b, *c, *d, *e, *f, *g, *h, *i;
#else
void
afs_warnuser(char *fmt, ...)
#endif
{
#if defined(AFS_WARNUSER_MARINER_ENV)
    char buf[256];
#endif
    AFS_STATCNT(afs_warnuser);
    if (afs_showflags & GAGUSER) {
#if !defined(AFS_AIX_ENV)
	va_list ap;
#endif
#ifdef AFS_GLOBAL_SUNLOCK
	int haveGlock = ISAFS_GLOCK();
#if defined(AFS_WARNUSER_MARINER_ENV)
	/* gain GLOCK for mariner */
	if (!haveGlock)
	    AFS_GLOCK();
#else
	/* drop GLOCK for uprintf */
	if (haveGlock)
	    AFS_GUNLOCK();
#endif
#endif /* AFS_GLOBAL_SUNLOCK */

#if defined(AFS_AIX_ENV)
	uprintf(fmt, a, b, c, d, e, f, g, h, i);
#else
	va_start(ap, fmt);
#if defined(AFS_WARNUSER_MARINER_ENV)
	/* mariner log the warning */
	snprintf(buf, sizeof(buf), "warn$");
	vsnprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), fmt, ap);
	if (afs_mariner)
	    afs_MarinerLog(buf, NULL);
	va_end(ap);
	va_start(ap, fmt);
	vprintf(fmt, ap);
#else
	afs_vprintf(fmt, ap);
#endif
	va_end(ap);
#endif

#ifdef AFS_GLOBAL_SUNLOCK
#if defined(AFS_WARNUSER_MARINER_ENV)
	/* drop GLOCK we got for mariner */
	if (!haveGlock)
	    AFS_GUNLOCK();
#else
	/* regain GLOCK we dropped for uprintf */
	if (haveGlock)
	    AFS_GLOCK();
#endif
#endif /* AFS_GLOBAL_SUNLOCK */
    }
}
