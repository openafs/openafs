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

#ifndef KERNEL

# include <roken.h>
# ifndef AFS_NT40_ENV
# include <net/if.h>
#  if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
#   include <sys/sysctl.h>
#   ifndef AFS_ARM_DARWIN_ENV
#    include <net/route.h>
#   endif
#   include <net/if_dl.h>
#  endif

/*
 * By including this, we get any system dependencies. In particular,
 * the pthreads for solaris requires the socket call to be mapped.
 */
#  include "rx.h"
#  include "rx_globals.h"
# endif /* AFS_NT40_ENV */
#else /* KERNEL */
# ifdef UKERNEL
#  include "rx/rx_kcommon.h"
# else /* UKERNEL */
#  include "rx/rx.h"
# endif /* UKERNEL */
#endif /* KERNEL */

#define NIFS		512

#if defined(AFS_USR_DFBSD_ENV)
# include <net/if.h>
# include <sys/sockio.h>
#endif

#ifdef KERNEL
/* only used for generating random noise */

afs_uint32 rxi_tempAddr = 0;	/* default attempt */

/* set the advisory noise */
void
rxi_setaddr(afs_uint32 x)
{
    rxi_tempAddr = x;
}

/* get approx to net addr */
afs_uint32
rxi_getaddr(void)
{
    return rxi_tempAddr;
}

#endif /* KERNEL */

#ifndef KERNEL

/* to satisfy those who call setaddr */
void
rxi_setaddr(afs_uint32 x)
{
}

#endif /* !KERNEL */


#if !defined(AFS_NT40_ENV)
/* For NT, rxi_getaddr has moved to rx_user.c. rxi_GetIfInfo is called by
 * rx_Init which sets up the list of addresses for us.
 */

#ifndef KERNEL

/* Return our internet address as a long in network byte order.  Returns zero
 * if it can't find one.
 */
afs_uint32
rxi_getaddr(void)
{
    afs_uint32 buffer[1024];
    int count;

    count = rx_getAllAddr(buffer, 1024);
    if (count > 0)
	return buffer[0];	/* returns the first address */
    else
	return count;
}

#endif /* !KERNEL */

#if !defined(KERNEL) || defined(UKERNEL)

#ifndef MAX
#define MAX(A,B) (((A)<(B)) ? (B) : (A))
#endif

#ifdef UKERNEL
#undef ioctl
#undef socket
#endif /* UKERNEL */

#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
#define ROUNDUP(a) \
        ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

static void
rt_xaddrs(caddr_t cp, caddr_t cplim, struct rt_addrinfo *rtinfo)
{
    struct sockaddr *sa;
    int i;

    memset(rtinfo->rti_info, 0, sizeof(rtinfo->rti_info));
    for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
	if ((rtinfo->rti_addrs & (1 << i)) == 0)
	    continue;
	rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
	ADVANCE(cp, sa);
    }
}
#endif

static_inline int
rxi_IsLoopbackIface(struct sockaddr_in *a, unsigned long flags)
{
    afs_uint32 addr = ntohl(a->sin_addr.s_addr);
    if (rx_IsLoopbackAddr(addr)) {
	return 1;
    }
    if ((flags & IFF_LOOPBACK) && ((addr & 0xff000000) == 0x7f000000)) {
	return 1;
    }
    return 0;
}

/* this function returns the total number of interface addresses
** the buffer has to be passed in by the caller
*/
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
#if defined(AFS_OBSD42_ENV)
void
ifm_fixversion(char *buffer, size_t *size) {
    struct if_msghdr *ifm;
    char *b = buffer;
    char *s, *t;

    if ((t = malloc(*size)) != NULL) {
	memcpy(t, buffer, *size);

	for (s = t; s < t + *size; s += ifm->ifm_msglen) {
	    ifm = (struct if_msghdr *)s;

	    if (ifm->ifm_version == RTM_VERSION) {
		memcpy(b, ifm, ifm->ifm_msglen);
		b += ifm->ifm_msglen;
	    }
	}

	free(t);

	*size = b - buffer;
    }
}
#endif

int
rx_getAllAddr_internal(afs_uint32 buffer[], int maxSize, int loopbacks)
{
    size_t needed;
    int mib[6];
    struct if_msghdr *ifm, *nextifm;
    struct ifa_msghdr *ifam;
    struct rt_addrinfo info;
    char *buf, *lim, *next;
    int count = 0, addrcount = 0;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_INET;		/* address family */
    mib[4] = NET_RT_IFLIST;
    mib[5] = 0;
    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
	return 0;
    if ((buf = malloc(needed)) == NULL)
	return 0;
    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
	free(buf);
	return 0;
    }
#if defined(AFS_OBSD42_ENV)
    ifm_fixversion(buf, &needed);
#endif
    lim = buf + needed;
    next = buf;
    while (next < lim) {
	ifm = (struct if_msghdr *)next;
	if (ifm->ifm_type != RTM_IFINFO) {
	    dpf(("out of sync parsing NET_RT_IFLIST\n"));
	    free(buf);
	    return 0;
	}
	next += ifm->ifm_msglen;
	ifam = NULL;
	addrcount = 0;
	while (next < lim) {
	    nextifm = (struct if_msghdr *)next;
	    if (nextifm->ifm_type != RTM_NEWADDR)
		break;
	    if (ifam == NULL)
		ifam = (struct ifa_msghdr *)nextifm;
	    addrcount++;
	    next += nextifm->ifm_msglen;
	}
	if ((ifm->ifm_flags & IFF_UP) == 0)
	    continue;		/* not up */
	while (addrcount > 0) {
	    struct sockaddr_in *a;

	    info.rti_addrs = ifam->ifam_addrs;

	    /* Expand the compacted addresses */
	    rt_xaddrs((char *)(ifam + 1), ifam->ifam_msglen + (char *)ifam,
		      &info);
	    if (info.rti_info[RTAX_IFA]->sa_family != AF_INET) {
		addrcount--;
		continue;
	    }
	    a = (struct sockaddr_in *) info.rti_info[RTAX_IFA];

	    if (count >= maxSize)	/* no more space */
		dpf(("Too many interfaces..ignoring 0x%x\n",
		       a->sin_addr.s_addr));
	    else if (!loopbacks && rxi_IsLoopbackIface(a, ifm->ifm_flags)) {
		addrcount--;
		continue;	/* skip loopback address as well. */
	    } else if (loopbacks && ifm->ifm_flags & IFF_LOOPBACK) {
		addrcount--;
		continue;	/* skip aliased loopbacks as well. */
	    } else
		buffer[count++] = a->sin_addr.s_addr;
	    addrcount--;
	    ifam = (struct ifa_msghdr *)((char *)ifam + ifam->ifam_msglen);
	}
    }
    free(buf);
    return count;
}

int
rx_getAllAddrMaskMtu(afs_uint32 addrBuffer[], afs_uint32 maskBuffer[],
		     afs_uint32 mtuBuffer[], int maxSize)
{
    int s;

    size_t needed;
    int mib[6];
    struct if_msghdr *ifm, *nextifm;
    struct ifa_msghdr *ifam;
    struct sockaddr_dl *sdl;
    struct rt_addrinfo info;
    char *buf, *lim, *next;
    int count = 0, addrcount = 0;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_INET;		/* address family */
    mib[4] = NET_RT_IFLIST;
    mib[5] = 0;
    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
	return 0;
    if ((buf = malloc(needed)) == NULL)
	return 0;
    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
	free(buf);
	return 0;
    }
#if defined(AFS_OBSD42_ENV)
    ifm_fixversion(buf, &needed);
#endif
    s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0)
	return 0;
    lim = buf + needed;
    next = buf;
    while (next < lim) {
	ifm = (struct if_msghdr *)next;
	if (ifm->ifm_type != RTM_IFINFO) {
	    dpf(("out of sync parsing NET_RT_IFLIST\n"));
	    free(buf);
	    return 0;
	}
	sdl = (struct sockaddr_dl *)(ifm + 1);
	next += ifm->ifm_msglen;
	ifam = NULL;
	addrcount = 0;
	while (next < lim) {
	    nextifm = (struct if_msghdr *)next;
	    if (nextifm->ifm_type != RTM_NEWADDR)
		break;
	    if (ifam == NULL)
		ifam = (struct ifa_msghdr *)nextifm;
	    addrcount++;
	    next += nextifm->ifm_msglen;
	}
	if ((ifm->ifm_flags & IFF_UP) == 0)
	    continue;		/* not up */
	while (addrcount > 0) {
	    struct sockaddr_in *a;

	    info.rti_addrs = ifam->ifam_addrs;

	    /* Expand the compacted addresses */
	    rt_xaddrs((char *)(ifam + 1), ifam->ifam_msglen + (char *)ifam,
		      &info);
	    if (info.rti_info[RTAX_IFA]->sa_family != AF_INET) {
		addrcount--;
		continue;
	    }
	    a = (struct sockaddr_in *) info.rti_info[RTAX_IFA];

	    if (!rx_IsLoopbackAddr(ntohl(a->sin_addr.s_addr))) {
		if (count >= maxSize) {	/* no more space */
		    dpf(("Too many interfaces..ignoring 0x%x\n",
			   a->sin_addr.s_addr));
		} else {
		    struct ifreq ifr;

		    addrBuffer[count] = a->sin_addr.s_addr;
		    a = (struct sockaddr_in *) info.rti_info[RTAX_NETMASK];
		    if (a)
			maskBuffer[count] = a->sin_addr.s_addr;
		    else
			maskBuffer[count] = htonl(0xffffffff);
		    memset(&ifr, 0, sizeof(ifr));
		    ifr.ifr_addr.sa_family = AF_INET;
		    strncpy(ifr.ifr_name, sdl->sdl_data, sdl->sdl_nlen);
		    if (ioctl(s, SIOCGIFMTU, (caddr_t) & ifr) < 0)
			mtuBuffer[count] = htonl(1500);
		    else
			mtuBuffer[count] = htonl(ifr.ifr_mtu);
		    count++;
		}
	    }
	    addrcount--;
	    ifam = (struct ifa_msghdr *)((char *)ifam + ifam->ifam_msglen);
	}
    }
    free(buf);
    return count;
}


int
rx_getAllAddr(afs_uint32 buffer[], int maxSize)
{
    return rx_getAllAddr_internal(buffer, maxSize, 0);
}
/* this function returns the total number of interface addresses
** the buffer has to be passed in by the caller
*/
#else /* UKERNEL indirectly, on DARWIN or XBSD */
static int
rx_getAllAddr_internal(afs_uint32 buffer[], int maxSize, int loopbacks)
{
    int s;
    int i, len, count = 0;
    struct ifconf ifc;
    struct ifreq ifs[NIFS], *ifr;
    struct sockaddr_in *a;
    /* can't ever be AFS_DARWIN_ENV or AFS_XBSD_ENV, no? */
#if    defined(AFS_AIX41_ENV) || defined (AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    char *cp, *cplim, *cpnext;	/* used only for AIX 41 */
#endif

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
	return 0;
    ifc.ifc_len = sizeof(ifs);
    ifc.ifc_buf = (caddr_t) ifs;
    i = ioctl(s, SIOCGIFCONF, &ifc);
    if (i < 0)
	return 0;
    len = ifc.ifc_len / sizeof(struct ifreq);
    if (len > NIFS)
	len = NIFS;
#if    defined(AFS_AIX41_ENV) || defined (AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    if (ifc.ifc_len > sizeof(ifs))	/* safety check */
	ifc.ifc_len = sizeof(ifs);
    for (cp = (char *)ifc.ifc_buf, cplim = ifc.ifc_buf + ifc.ifc_len;
	 cp < cplim;
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	 cp += _SIZEOF_ADDR_IFREQ(*ifr)
#else
#ifdef AFS_AIX51_ENV
	 cp = cpnext
#else
	 cp += sizeof(ifr->ifr_name) + MAX(a->sin_len, sizeof(*a))
#endif
#endif
	)
#else
    for (i = 0; i < len; ++i)
#endif
    {
#if    defined(AFS_AIX41_ENV) || defined (AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	ifr = (struct ifreq *)cp;
#else
	ifr = &ifs[i];
#endif
	a = (struct sockaddr_in *)&ifr->ifr_addr;
#ifdef AFS_AIX51_ENV
	cpnext = cp + sizeof(ifr->ifr_name) + MAX(a->sin_len, sizeof(*a));
#endif
	if (a->sin_family != AF_INET)
	    continue;
	if (ioctl(s, SIOCGIFFLAGS, ifr) < 0) {
	    perror("SIOCGIFFLAGS");
	    continue;		/* ignore this address */
	}
	if (a->sin_addr.s_addr != 0) {
            if (!loopbacks) {
                if (rxi_IsLoopbackIface(a, ifr->ifr_flags))
		    continue;	/* skip loopback address as well. */
            } else {
                if (ifr->ifr_flags & IFF_LOOPBACK)
		    continue;	/* skip aliased loopbacks as well. */
	    }
	    if (count >= maxSize)	/* no more space */
		dpf(("Too many interfaces..ignoring 0x%x\n",
		       a->sin_addr.s_addr));
	    else
		buffer[count++] = a->sin_addr.s_addr;
	}
    }
    close(s);
    return count;
}

int
rx_getAllAddr(afs_uint32 buffer[], int maxSize)
{
    return rx_getAllAddr_internal(buffer, maxSize, 0);
}

/* this function returns the total number of interface addresses
 * the buffer has to be passed in by the caller. It also returns
 * the interface mask. If AFS_USERSPACE_IP_ADDR is defined, it
 * gets the mask which is then passed into the kernel and is used
 * by afsi_SetServerIPRank().
 */
int
rx_getAllAddrMaskMtu(afs_uint32 addrBuffer[], afs_uint32 maskBuffer[],
                     afs_uint32 mtuBuffer[], int maxSize)
{
    int i, count = 0;
#if defined(AFS_USERSPACE_IP_ADDR)
    int s, len;
    struct ifconf ifc;
    struct ifreq ifs[NIFS], *ifr;
    struct sockaddr_in *a;
#endif

#if     defined(AFS_AIX41_ENV) || defined(AFS_USR_AIX_ENV)
    char *cp, *cplim;		/* used only for AIX 41 */
#endif

#if !defined(AFS_USERSPACE_IP_ADDR)
    count = rx_getAllAddr_internal(addrBuffer, 1024, 0);
    for (i = 0; i < count; i++) {
	maskBuffer[i] = htonl(0xffffffff);
	mtuBuffer[i] = htonl(1500);
    }
    return count;
#else /* AFS_USERSPACE_IP_ADDR */
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
	return 0;

    ifc.ifc_len = sizeof(ifs);
    ifc.ifc_buf = (caddr_t) ifs;
    i = ioctl(s, SIOCGIFCONF, &ifc);
    if (i < 0) {
	close(s);
	return 0;
    }
    len = ifc.ifc_len / sizeof(struct ifreq);
    if (len > NIFS)
	len = NIFS;

#if     defined(AFS_AIX41_ENV) || defined(AFS_USR_AIX_ENV)
    if (ifc.ifc_len > sizeof(ifs))	/* safety check */
	ifc.ifc_len = sizeof(ifs);
    for (cp = (char *)ifc.ifc_buf, cplim = ifc.ifc_buf + ifc.ifc_len;
	 cp < cplim;
	 cp += sizeof(ifr->ifr_name) + MAX(a->sin_len, sizeof(*a))) {
	ifr = (struct ifreq *)cp;
#else
    for (i = 0; i < len; ++i) {
	ifr = &ifs[i];
#endif
	a = (struct sockaddr_in *)&ifr->ifr_addr;
	if (a->sin_addr.s_addr != 0 && a->sin_family == AF_INET) {

	    if (ioctl(s, SIOCGIFFLAGS, ifr) < 0) {
		perror("SIOCGIFFLAGS");
		continue;	/* ignore this address */
	    }

            if (rx_IsLoopbackAddr(ntohl(a->sin_addr.s_addr)))
                continue;   /* skip loopback address as well. */

	    if (count >= maxSize) {	/* no more space */
		dpf(("Too many interfaces..ignoring 0x%x\n",
		       a->sin_addr.s_addr));
		continue;
	    }

	    addrBuffer[count] = a->sin_addr.s_addr;

	    if (ioctl(s, SIOCGIFNETMASK, (caddr_t) ifr) < 0) {
		perror("SIOCGIFNETMASK");
		maskBuffer[count] = htonl(0xffffffff);
	    } else {
		maskBuffer[count] = (((struct sockaddr_in *)
				      (&ifr->ifr_addr))->sin_addr).s_addr;
	    }

	    mtuBuffer[count] = htonl(1500);
#ifdef SIOCGIFMTU
	    if (ioctl(s, SIOCGIFMTU, (caddr_t) ifr) < 0) {
		perror("SIOCGIFMTU");
	    } else {
		mtuBuffer[count] = htonl(ifr->ifr_metric);
	    }
#endif /* SIOCGIFMTU */
#ifdef SIOCRIPMTU
	    if (ioctl(s, SIOCRIPMTU, (caddr_t) ifr) < 0) {
		perror("SIOCRIPMTU");
	    } else {
		mtuBuffer[count] = htonl(ifr->ifr_metric);
	    }
#endif /* SIOCRIPMTU */

	    count++;
	}
    }
    close(s);
    return count;
#endif /* AFS_USERSPACE_IP_ADDR */
}
#endif

#endif /* ! AFS_NT40_ENV */
#endif /* !KERNEL || UKERNEL */
