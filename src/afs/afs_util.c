/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * afs_util.c - miscellaneous AFS client utility functions
 *
 * Implements:
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

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

#ifndef afs_cv2string
char *
afs_cv2string(char *ttp, afs_uint32 aval)
{
    register char *tp = ttp;
    register int i;
    int any;

    AFS_STATCNT(afs_cv2string);
    any = 0;
    *(--tp) = 0;
    while (aval != 0) {
	i = aval % 10;
	*(--tp) = '0' + i;
	aval /= 10;
	any = 1;
    }
    if (!any)
	*(--tp) = '0';
    return tp;

}				/*afs_cv2string */
#endif

#ifndef afs_strcasecmp
int
afs_strcasecmp(char *s1, char *s2)
{
    while (*s1 && *s2) {
	char c1, c2;

	c1 = *s1++;
	c2 = *s2++;
	if (c1 >= 'A' && c1 <= 'Z')
	    c1 += 0x20;
	if (c2 >= 'A' && c2 <= 'Z')
	    c2 += 0x20;
	if (c1 != c2)
	    return c1 - c2;
    }

    return *s1 - *s2;
}
#endif

#ifndef afs_strcat
char *
afs_strcat(char *s1, char *s2)
{
    char *os1;

    os1 = s1;
    while (*s1++);
    --s1;
    while ((*s1++ = *s2++));
    return (os1);
}
#endif

#ifdef AFS_OBSD34_ENV
char *
afs_strcpy(char *s1, char *s2)
{
    char *os1;

    os1 = s1;
    while ((*s1++ = *s2++) != '\0');
    return os1;
}
#endif

#ifndef afs_strchr
char *
afs_strchr(char *s, int c)
{
    char *p;

    for (p = s; *p; p++)
	if (*p == c)
	    return p;
    return NULL;
}
#endif

char *
afs_strdup(char *s)
{
    char *n;
    int cc;

    cc = strlen(s) + 1;
    n = (char *)afs_osi_Alloc(cc);
    if (n)
	memcpy(n, s, cc);

    return n;
}

void
print_internet_address(char *preamble, struct srvAddr *sa, char *postamble,
		       int flag)
{
    register struct server *aserver = sa->server;
    char *ptr = "\n";
    afs_uint32 address;

    AFS_STATCNT(print_internet_address);
    address = ntohl(sa->sa_ip);
    if (aserver->flags & SRVR_MULTIHOMED) {
	if (flag == 1) {	/* server down mesg */
	    if (!(aserver->flags & SRVR_ISDOWN))
		ptr =
		    " (multi-homed address; other same-host interfaces maybe up)\n";
	    else
		ptr = " (all multi-homed ip addresses down for the server)\n";
	} else if (flag == 2) {	/* server up mesg */
	    ptr =
		" (multi-homed address; other same-host interfaces may still be down)\n";
	}
    }
    afs_warn("%s%d.%d.%d.%d in cell %s%s%s", preamble, (address >> 24),
	     (address >> 16) & 0xff, (address >> 8) & 0xff, (address) & 0xff,
	     aserver->cell->cellName, postamble, ptr);
    afs_warnuser("%s%d.%d.%d.%d in cell %s%s%s", preamble, (address >> 24),
		 (address >> 16) & 0xff, (address >> 8) & 0xff,
		 (address) & 0xff, aserver->cell->cellName, postamble, ptr);

}				/*print_internet_address */



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


/* run everywhere, checking locks */
void
afs_CheckLocks(void)
{
    register int i;

    afs_warn("Looking for locked data structures.\n");
    afs_warn("conn %lx, volume %lx, user %lx, cell %lx, server %lx\n", &afs_xconn,
	     &afs_xvolume, &afs_xuser, &afs_xcell, &afs_xserver);
    {
	register struct vcache *tvc;
	AFS_STATCNT(afs_CheckLocks);

	for (i = 0; i < VCSIZE; i++) {
	    for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
                if (tvc->states & CVInit) continue;
#ifdef	AFS_OSF_ENV
		if (VREFCOUNT(tvc) > 1)
#else /* AFS_OSF_ENV */
#ifdef AFS_DARWIN80_ENV
		if (vnode_isinuse(AFSTOV(tvc), 0))
#else
		if (VREFCOUNT(tvc))
#endif
#endif
		    afs_warn("Stat cache entry at %x is held\n", tvc);
		if (CheckLock(&tvc->lock))
		    afs_warn("Stat entry at %x is locked\n", tvc);
	    }
	}
    }
    {
	register struct dcache *tdc;
	for (i = 0; i < afs_cacheFiles; i++) {
	    tdc = afs_indexTable[i];
	    if (tdc) {
		if (tdc->refCount)
		    afs_warn("Disk entry %d at %x is held\n", i, tdc);
	    }
	    if (afs_indexFlags[i] & IFDataMod)
		afs_warn("Disk entry %d at %x has IFDataMod flag set.\n", i,
			 tdc);
	}
    }
    {
	struct srvAddr *sa;
	struct server *ts;
	struct conn *tc;
	for (i = 0; i < NSERVERS; i++) {
	    for (ts = afs_servers[i]; ts; ts = ts->next) {
		if (ts->flags & SRVR_ISDOWN)
		    printf("Server entry %lx is marked down\n", (unsigned long)ts);
		for (sa = ts->addr; sa; sa = sa->next_sa) {
		    for (tc = sa->conns; tc; tc = tc->next) {
			if (tc->refCount)
			    afs_warn("conn at %x (server %x) is held\n", tc,
				     sa->sa_ip);
		    }
		}
	    }
	}
    }
    {
	struct volume *tv;
	for (i = 0; i < NVOLS; i++) {
	    for (tv = afs_volumes[i]; tv; tv = tv->next) {
		if (CheckLock(&tv->lock))
		    afs_warn("volume at %x is locked\n", tv);
		if (tv->refCount)
		    afs_warn("volume at %x is held\n", tv);
	    }
	}
    }
    {
	struct unixuser *tu;

	for (i = 0; i < NUSERS; i++) {
	    for (tu = afs_users[i]; tu; tu = tu->next) {
		if (tu->refCount)
		    printf("user at %lx is held\n", (unsigned long)tu);
	    }
	}
    }
    afs_warn("Done.\n");
}


int
afs_noop(void)
{
    AFS_STATCNT(afs_noop);
#ifdef	AFS_OSF30_ENV
    return (EOPNOTSUPP);
#else
    return EINVAL;
#endif
}

int
afs_badop(void)
{
    AFS_STATCNT(afs_badop);
    osi_Panic("afs bad vnode op");
    return 0;
}

/*
 * afs_data_pointer_to_int32() - returns least significant afs_int32 of the
 * given data pointer, without triggering "cast truncates pointer"
 * warnings.  We use this where we explicitly don't care whether a
 * pointer is truncated -- it loses information where a pointer is
 * larger than an afs_int32.
 */

afs_int32
afs_data_pointer_to_int32(const void *p)
{
    union {
	afs_int32 i32[sizeof(void *) / sizeof(afs_int32)];
	const void *p;
    } ip;

    int i32_sub;		/* subscript of least significant afs_int32 in ip.i32[] */

    /* set i32_sub */

    {
	/* used to determine the byte order of the system */

	union {
	    char c[sizeof(int) / sizeof(char)];
	    int i;
	} ci;

	ci.i = 1;
	if (ci.c[0] == 1) {
	    /* little-endian system */
	    i32_sub = 0;
	} else {
	    /* big-endian system */
	    i32_sub = (sizeof ip.i32 / sizeof ip.i32[0]) - 1;
	}
    }

    ip.p = p;
    return ip.i32[i32_sub];
}
