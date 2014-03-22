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
# elif defined(AFS_FBSD_ENV)
#  include <machine/stdarg.h>
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
{
    AFS_STATCNT(afs_warn);

    if (afs_showflags & GAGCONSOLE) {
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
    }
}
#else /* AFS_AIX_ENV */
static_inline void
afs_vwarn(char *fmt, va_list ap)
{
    afs_vprintf(fmt, ap);
}


void
afs_warn(char *fmt, ...)
{
    AFS_STATCNT(afs_warn);

    if (afs_showflags & GAGCONSOLE) {
	va_list ap;

	va_start(ap, fmt);
	afs_vwarn(fmt, ap);
	va_end(ap);
    }
}
#endif /* AFS_AIX_ENV */


#ifdef AFS_AIX_ENV
void
afs_warnuser(fmt, a, b, c, d, e, f, g, h, i)
    char *fmt;
    void *a, *b, *c, *d, *e, *f, *g, *h, *i;
{
    AFS_STATCNT(afs_warnuser);
    if (afs_showflags & GAGUSER) {
# ifdef AFS_GLOBAL_SUNLOCK
	int haveGlock = ISAFS_GLOCK();
	/* drop GLOCK for uprintf */
	if (haveGlock)
	    AFS_GUNLOCK();
# endif /* AFS_GLOBAL_SUNLOCK */
	uprintf(fmt, a, b, c, d, e, f, g, h, i);
# ifdef AFS_GLOBAL_SUNLOCK
	/* regain GLOCK we dropped for uprintf */
	if (haveGlock)
	    AFS_GLOCK();
# endif /* AFS_GLOBAL_SUNLOCK */
    }
}
#else  /* AFS_AIX_ENV */
# ifdef AFS_WARNUSER_MARINER_ENV
static void
afs_vwarnuser (char *fmt, va_list ap)
{
#  ifdef AFS_GLOBAL_SUNLOCK
    int haveGlock = ISAFS_GLOCK();
    /* gain GLOCK for mariner */
    if (!haveGlock)
	AFS_GLOCK();
#  endif /* AFS_GLOBAL_SUNLOCK */

    if (afs_mariner) {
	char buf[256];
	va_list aq;
	va_copy(aq, ap);
	/* mariner log the warning */
	snprintf(buf, sizeof(buf), "warn$");
	vsnprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), fmt, aq);
	afs_MarinerLog(buf, NULL);
    }
    vprintf(fmt, ap);

#  ifdef AFS_GLOBAL_SUNLOCK
    /* drop GLOCK we got for mariner */
    if (!haveGlock)
	AFS_GUNLOCK();
#  endif /* AFS_GLOBAL_SUNLOCK */
}
# else /* AFS_WARNUSER_MARINER_ENV */
static void
afs_vwarnuser (char *fmt, va_list ap)
{
#  ifdef AFS_GLOBAL_SUNLOCK
    int haveGlock = ISAFS_GLOCK();
    /* drop GLOCK */
    if (haveGlock)
	AFS_GUNLOCK();
#  endif /* AFS_GLOBAL_SUNLOCK */

    afs_vprintf(fmt, ap);

#  ifdef AFS_GLOBAL_SUNLOCK
    /* regain GLOCK we dropped */
    if (haveGlock)
	AFS_GLOCK();
#  endif /* AFS_GLOBAL_SUNLOCK */
}
# endif /* AFS_WARNUSER_MARINER_ENV */

void
afs_warnuser(char *fmt, ...)
{
    AFS_STATCNT(afs_warnuser);
    if (afs_showflags & GAGUSER) {
	va_list ap;

	va_start(ap, fmt);
	afs_vwarnuser(fmt, ap);
	va_end(ap);
    }
}

#endif /* AFS_AIX_ENV */


#ifdef AFS_AIX_ENV
void
afs_warnall(fmt, a, b, c, d, e, f, g, h, i)
    char *fmt;
    void *a, *b, *c, *d, *e, *f, *g, *h, *i;
{
    afs_warn(fmt, a, b, c, d, e, f, g, h, i);
    afs_warnuser(fmt, a, b, c, d, e, f, g, h, i);

}
#else /* AFS_AIX_ENV */
/*  On Linux both afs_warn and afs_warnuser go to the same
 *  place.  Suppress one of them if we're running on Linux.
 */
void
afs_warnall(char *fmt, ...)
{
    va_list ap;

# ifdef AFS_LINUX20_ENV
    AFS_STATCNT(afs_warn);
    if ((afs_showflags & GAGCONSOLE) || (afs_showflags & GAGUSER)) {
	va_start(ap, fmt);
	afs_vwarn(fmt, ap);
	va_end(ap);
    }
# else /* AFS_LINUX20_ENV */
    AFS_STATCNT(afs_warn);
    if (afs_showflags & GAGCONSOLE) {
	va_start(ap, fmt);
	afs_vwarn(fmt, ap);
	va_end(ap);
    }

    AFS_STATCNT(afs_warnuser);
    if (afs_showflags & GAGUSER) {
	va_start(ap, fmt);
	afs_vwarnuser(fmt, ap);
	va_end(ap);
    }
# endif /* AFS_LINUX20_ENV */
}
#endif /* AFS_AIX_ENV */
