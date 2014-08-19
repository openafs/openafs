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
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN_ENV)
#include <netinet/in_var.h>
#endif /* ! AFS_HPUX110_ENV */
#endif /* !defined(UKERNEL) */

#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

#ifdef AFS_LINUX20_ENV
#include "afs/afs_md5.h"	/* For MD5 inodes - Linux only */
#endif

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

afs_int32 afs_new_inum = 0;

#ifndef afs_cv2string
char *
afs_cv2string(char *ttp, afs_uint32 aval)
{
    char *tp = ttp;
    int i;
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

/* not a generic strtoul replacement. for vol/vno/uniq, portable */

afs_int32
afs_strtoi_r(const char *str, char **endptr, afs_uint32 *ret)
{
    char *x;

    *ret = 0;
    *endptr = (char *)str;

    if (!str)
	return ERANGE;

    for (x = (char *)str; *x >= '0' && *x <= '9'; x++) {
	/* Check for impending overflow */
	if (*ret > 429496729) { /* ULONG_MAX/10 */
	    *ret = 0;
	    *endptr = (char *)str;
	    return EINVAL;
	}

	*ret = (*ret * 10) + (*x - '0');
    }

    *endptr = x;
    return 0;
}

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
#ifndef afs_strrchr
char *
afs_strrchr(char *s, int c)
{
    char *p = NULL;

    do {
	if (*s == c)
	    p = (char*) s;
    } while (*s++);
    return p;
}
#endif

char *
afs_strdup(char *s)
{
    char *n;
    int cc;

    cc = strlen(s) + 1;
    n = afs_osi_Alloc(cc);
    if (n)
	memcpy(n, s, cc);

    return n;
}

void
print_internet_address(char *preamble, struct srvAddr *sa, char *postamble,
		       int flag, int code)
{
    struct server *aserver = sa->server;
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
    afs_warnall("%s%d.%d.%d.%d in cell %s%s (code %d)%s", preamble, (address >> 24),
	     (address >> 16) & 0xff, (address >> 8) & 0xff, (address) & 0xff,
	     aserver->cell->cellName, postamble, code, ptr);

}				/*print_internet_address */



/* run everywhere, checking locks */
void
afs_CheckLocks(void)
{
    int i;

    afs_warn("Looking for locked data structures.\n");
    afs_warn("conn %p, volume %p, user %p, cell %p, server %p\n", &afs_xconn,
	     &afs_xvolume, &afs_xuser, &afs_xcell, &afs_xserver);
    {
	struct vcache *tvc;
	AFS_STATCNT(afs_CheckLocks);

	for (i = 0; i < VCSIZE; i++) {
	    for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
                if (tvc->f.states & CVInit) continue;
#ifdef AFS_DARWIN80_ENV
		if (vnode_isinuse(AFSTOV(tvc), 0))
#else
		if (VREFCOUNT(tvc))
#endif
		    afs_warn("Stat cache entry at %p is held\n", tvc);
		if (CheckLock(&tvc->lock))
		    afs_warn("Stat entry at %p is locked\n", tvc);
	    }
	}
    }
    {
	struct dcache *tdc;
	for (i = 0; i < afs_cacheFiles; i++) {
	    tdc = afs_indexTable[i];
	    if (tdc) {
		if (tdc->refCount)
		    afs_warn("Disk entry %d at %p is held\n", i, tdc);
	    }
	    if (afs_indexFlags[i] & IFDataMod)
		afs_warn("Disk entry %d at %p has IFDataMod flag set.\n", i,
			 tdc);
	}
    }
    {
	struct srvAddr *sa;
	struct server *ts;
	struct afs_conn *tc;
	for (i = 0; i < NSERVERS; i++) {
	    for (ts = afs_servers[i]; ts; ts = ts->next) {
		if (ts->flags & SRVR_ISDOWN)
		    afs_warn("Server entry %p is marked down\n", ts);
		for (sa = ts->addr; sa; sa = sa->next_sa) {
		    for (tc = sa->conns; tc; tc = tc->next) {
			if (tc->refCount)
			    afs_warn("conn at %p (server %x) is held\n", tc,
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
		    afs_warn("volume at %p is locked\n", tv);
		if (tv->refCount)
		    afs_warn("volume at %p is held\n", tv);
	    }
	}
    }
    {
	struct unixuser *tu;

	for (i = 0; i < NUSERS; i++) {
	    for (tu = afs_users[i]; tu; tu = tu->next) {
		if (tu->refCount)
		    afs_warn("user at %lx is held\n", (unsigned long)tu);
	    }
	}
    }
    afs_warn("Done.\n");
}


int
afs_noop(void)
{
    AFS_STATCNT(afs_noop);
    return EINVAL;
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

#ifdef AFS_LINUX20_ENV

afs_int32
afs_calc_inum(afs_int32 cell, afs_int32 volume, afs_int32 vnode)
{
    afs_int32 ino = 0, vno = vnode;
    char digest[16];
    struct afs_md5 ct;

    if (afs_new_inum) {
	int offset;
	AFS_MD5_Init(&ct);
	AFS_MD5_Update(&ct, &cell, 4);
	AFS_MD5_Update(&ct, &volume, 4);
	AFS_MD5_Update(&ct, &vnode, 4);
	AFS_MD5_Final(digest, &ct);

	/* Userspace may react oddly to an inode number of 0 or 1, so keep
	 * reading more of the md5 digest if we get back one of those.
	 * Make sure not to read beyond the end of the digest; if we somehow
	 * still have a 0, we will fall through to the non-md5 calculation. */
	for (offset = 0;
	     (ino == 0 || ino == 1) &&
	      offset + sizeof(ino) <= sizeof(digest);
	     offset++) {

	    memcpy(&ino, &digest[offset], sizeof(ino));
	    ino ^= (ino ^ vno) & 1;
	    ino &= 0x7fffffff;      /* Assumes 32 bit ino_t ..... */
	}
    }
    if (ino == 0 || ino == 1) {
	ino = (volume << 16) + vnode;
    }
    ino &= 0x7fffffff;      /* Assumes 32 bit ino_t ..... */
    return ino;
}

#else

afs_int32
afs_calc_inum(afs_int32 cell, afs_int32 volume, afs_int32 vnode)
{
    return (volume << 16) + vnode;
}

#endif
