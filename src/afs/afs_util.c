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

afs_int32 afs_new_inum = 0;

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

#ifdef AFS_LINUX20_ENV

struct afs_md5 {
    unsigned int sz[2];
    afs_int32 counter[4];
    unsigned char save[64];
};

static void AFS_MD5_Init (struct afs_md5 *m);
static void AFS_MD5_Update (struct afs_md5 *m, const void *p, size_t len);
static void AFS_MD5_Final (void *res, struct afs_md5 *m); /* u_int32 res[4] */

#define A m->counter[0]
#define B m->counter[1]
#define C m->counter[2]
#define D m->counter[3]
#define X data

static void
AFS_MD5_Init (struct afs_md5 *m)
{
    m->sz[0] = 0;
    m->sz[1] = 0;
    D = 0x10325476;
    C = 0x98badcfe;
    B = 0xefcdab89;
    A = 0x67452301;
}

#define F(x,y,z) ((x & y) | (~x & z))
#define G(x,y,z) ((x & z) | (y & ~z))
#define H(x,y,z) (x ^ y ^ z)
#define I(x,y,z) (y ^ (x | ~z))

static inline afs_uint32
cshift (afs_uint32 x, unsigned int n)
{
    return ((x << n) | (x >> (32 - n)));
}

#define DOIT(a,b,c,d,k,s,i,OP) \
a = b + cshift(a + OP(b,c,d) + X[k] + (i), s)

#define DO1(a,b,c,d,k,s,i) DOIT(a,b,c,d,k,s,i,F)
#define DO2(a,b,c,d,k,s,i) DOIT(a,b,c,d,k,s,i,G)
#define DO3(a,b,c,d,k,s,i) DOIT(a,b,c,d,k,s,i,H)
#define DO4(a,b,c,d,k,s,i) DOIT(a,b,c,d,k,s,i,I)

static inline void
calc (struct afs_md5 *m, afs_uint32 *data)
{
    afs_uint32 AA, BB, CC, DD;
    
    AA = A;
    BB = B;
    CC = C;
    DD = D;
    
    /* Round 1 */
    
    DO1(A,B,C,D,0,7,0xd76aa478);
    DO1(D,A,B,C,1,12,0xe8c7b756);
    DO1(C,D,A,B,2,17,0x242070db);
    DO1(B,C,D,A,3,22,0xc1bdceee);
    
    DO1(A,B,C,D,4,7,0xf57c0faf);
    DO1(D,A,B,C,5,12,0x4787c62a);
    DO1(C,D,A,B,6,17,0xa8304613);
    DO1(B,C,D,A,7,22,0xfd469501);
    
    DO1(A,B,C,D,8,7,0x698098d8);
    DO1(D,A,B,C,9,12,0x8b44f7af);
    DO1(C,D,A,B,10,17,0xffff5bb1);
    DO1(B,C,D,A,11,22,0x895cd7be);
    
    DO1(A,B,C,D,12,7,0x6b901122);
    DO1(D,A,B,C,13,12,0xfd987193);
    DO1(C,D,A,B,14,17,0xa679438e);
    DO1(B,C,D,A,15,22,0x49b40821);
    
    /* Round 2 */
    
    DO2(A,B,C,D,1,5,0xf61e2562);
    DO2(D,A,B,C,6,9,0xc040b340);
    DO2(C,D,A,B,11,14,0x265e5a51);
    DO2(B,C,D,A,0,20,0xe9b6c7aa);
    
    DO2(A,B,C,D,5,5,0xd62f105d);
    DO2(D,A,B,C,10,9,0x2441453);
    DO2(C,D,A,B,15,14,0xd8a1e681);
    DO2(B,C,D,A,4,20,0xe7d3fbc8);
    
    DO2(A,B,C,D,9,5,0x21e1cde6);
    DO2(D,A,B,C,14,9,0xc33707d6);
    DO2(C,D,A,B,3,14,0xf4d50d87);
    DO2(B,C,D,A,8,20,0x455a14ed);
    
    DO2(A,B,C,D,13,5,0xa9e3e905);
    DO2(D,A,B,C,2,9,0xfcefa3f8);
    DO2(C,D,A,B,7,14,0x676f02d9);
    DO2(B,C,D,A,12,20,0x8d2a4c8a);
    
    /* Round 3 */
    
    DO3(A,B,C,D,5,4,0xfffa3942);
    DO3(D,A,B,C,8,11,0x8771f681);
    DO3(C,D,A,B,11,16,0x6d9d6122);
    DO3(B,C,D,A,14,23,0xfde5380c);
    
    DO3(A,B,C,D,1,4,0xa4beea44);
    DO3(D,A,B,C,4,11,0x4bdecfa9);
    DO3(C,D,A,B,7,16,0xf6bb4b60);
    DO3(B,C,D,A,10,23,0xbebfbc70);
    
    DO3(A,B,C,D,13,4,0x289b7ec6);
    DO3(D,A,B,C,0,11,0xeaa127fa);
    DO3(C,D,A,B,3,16,0xd4ef3085);
    DO3(B,C,D,A,6,23,0x4881d05);
    
    DO3(A,B,C,D,9,4,0xd9d4d039);
    DO3(D,A,B,C,12,11,0xe6db99e5);
    DO3(C,D,A,B,15,16,0x1fa27cf8);
    DO3(B,C,D,A,2,23,0xc4ac5665);
    
    /* Round 4 */
    
    DO4(A,B,C,D,0,6,0xf4292244);
    DO4(D,A,B,C,7,10,0x432aff97);
    DO4(C,D,A,B,14,15,0xab9423a7);
    DO4(B,C,D,A,5,21,0xfc93a039);
    
    DO4(A,B,C,D,12,6,0x655b59c3);
    DO4(D,A,B,C,3,10,0x8f0ccc92);
    DO4(C,D,A,B,10,15,0xffeff47d);
    DO4(B,C,D,A,1,21,0x85845dd1);
    
    DO4(A,B,C,D,8,6,0x6fa87e4f);
    DO4(D,A,B,C,15,10,0xfe2ce6e0);
    DO4(C,D,A,B,6,15,0xa3014314);
    DO4(B,C,D,A,13,21,0x4e0811a1);
    
    DO4(A,B,C,D,4,6,0xf7537e82);
    DO4(D,A,B,C,11,10,0xbd3af235);
    DO4(C,D,A,B,2,15,0x2ad7d2bb);
    DO4(B,C,D,A,9,21,0xeb86d391);
    
    A += AA;
    B += BB;
    C += CC;
    D += DD;
}

/*
 * From `Performance analysis of MD5' by Joseph D. Touch <touch@isi.edu>
 */

#if defined(WORDS_BIGENDIAN)
static inline afs_uint32
swap_u_int32_t (afs_uint32 t)
{
    afs_uint32 temp1, temp2;
    
    temp1   = cshift(t, 16);
    temp2   = temp1 >> 8;
    temp1  &= 0x00ff00ff;
    temp2  &= 0x00ff00ff;
    temp1 <<= 8;
    return temp1 | temp2;
}
#endif

struct x32{
    unsigned int a:32;
    unsigned int b:32;
};

static void
AFS_MD5_Update (struct afs_md5 *m, const void *v, size_t len)
{
    const unsigned char *p = v;
    size_t old_sz = m->sz[0];
    size_t offset;
    
    m->sz[0] += len * 8;
    if (m->sz[0] < old_sz)
	++m->sz[1];
    offset = (old_sz / 8)  % 64;
    while(len > 0){
	size_t l = MIN(len, 64 - offset);
	memcpy(m->save + offset, p, l);
	offset += l;
	p += l;
	len -= l;
	if(offset == 64){
#if defined(WORDS_BIGENDIAN)
	    int i;
	    afs_uint32 temp[16];
	    struct x32 *us = (struct x32*)m->save;
	    for(i = 0; i < 8; i++){
		temp[2*i+0] = swap_u_int32_t(us[i].a);
		temp[2*i+1] = swap_u_int32_t(us[i].b);
	    }
	    calc(m, temp);
#else
	    calc(m, (afs_uint32*)m->save);
#endif
	    offset = 0;
	}
    }
}

static void
AFS_MD5_Final (void *res, struct afs_md5 *m)
{
    unsigned char zeros[72];
    unsigned offset = (m->sz[0] / 8) % 64;
    unsigned int dstart = (120 - offset - 1) % 64 + 1;
    
    *zeros = 0x80;
    memset (zeros + 1, 0, sizeof(zeros) - 1);
    zeros[dstart+0] = (m->sz[0] >> 0) & 0xff;
    zeros[dstart+1] = (m->sz[0] >> 8) & 0xff;
    zeros[dstart+2] = (m->sz[0] >> 16) & 0xff;
    zeros[dstart+3] = (m->sz[0] >> 24) & 0xff;
    zeros[dstart+4] = (m->sz[1] >> 0) & 0xff;
    zeros[dstart+5] = (m->sz[1] >> 8) & 0xff;
    zeros[dstart+6] = (m->sz[1] >> 16) & 0xff;
    zeros[dstart+7] = (m->sz[1] >> 24) & 0xff;
    AFS_MD5_Update (m, zeros, dstart + 8);
    {
	int i;
	unsigned char *r = (unsigned char *)res;
	
	for (i = 0; i < 4; ++i) {
	    r[4*i]   = m->counter[i] & 0xFF;
	    r[4*i+1] = (m->counter[i] >> 8) & 0xFF;
	    r[4*i+2] = (m->counter[i] >> 16) & 0xFF;
	    r[4*i+3] = (m->counter[i] >> 24) & 0xFF;
	}
    }
}

afs_int32 afs_calc_inum (afs_int32 volume, afs_int32 vnode)
{ 
    afs_int32 ino;
    char digest[16];
    struct afs_md5 ct;
    
    if (afs_new_inum) {
	AFS_MD5_Init(&ct);
	AFS_MD5_Update(&ct, &volume, 4);
	AFS_MD5_Update(&ct, &vnode, 4);
	AFS_MD5_Final(digest, &ct);
	memcpy(&ino, digest, sizeof(ino_t));
	ino ^= (ino ^ vnode) & 1;
    } else {
	ino = (volume << 16) + vnode;
	ino &= 0x7fffffff;      /* Assumes 32 bit ino_t ..... */
    }
    return ino;
}

#else

afs_int32 afs_calc_inum (afs_int32 volume, afs_int32 vnode)
{
    return (volume << 16) + vnode;
}

#endif
