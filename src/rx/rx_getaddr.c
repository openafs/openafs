
#ifndef lint
#endif

/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/* getaddr -- get our internet address. July, 1986 */

#include <afs/param.h>
#ifndef KERNEL
#ifndef AFS_NT40_ENV
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
/*
 * By including this, we get any system dependencies. In particular,
 * the pthreads for solaris requires the socket call to be mapped.
 */
#include "rx.h"
#endif
#else	/* KERNEL */
#ifdef UKERNEL
#include "../rx/rx_kcommon.h"
#else /* UKERNEL */
/* nothing here required yet */
#endif /* UKERNEL */
#endif	/* KERNEL */

#define NIFS		512

#ifdef KERNEL
/* only used for generating random noise */

afs_int32 rxi_tempAddr=0;	/* default attempt */

/* set the advisory noise */
afs_int32 rxi_setaddr(x)
afs_int32 x;{
    rxi_tempAddr = x;
}

/* get approx to net addr */
afs_int32 rxi_getaddr() {
    return rxi_tempAddr;
}

#endif /* KERNEL */

#ifndef KERNEL

/* to satisfy those who call setaddr */
rxi_setaddr(x)
afs_int32 x; {
    return 0;
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
afs_int32 rxi_getaddr ()
{
	afs_int32	buffer[1024];
	int     count;

	count = rx_getAllAddr(buffer, 1024); 
	if ( count > 0 )
		return buffer[0]; /* returns the first address */
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

/* this function returns the total number of interface addresses 
** the buffer has to be passed in by the caller
*/
int rx_getAllAddr (buffer,maxSize)
afs_int32	buffer[];
int 	maxSize;	/* sizeof of buffer in afs_int32 units */
{
    int     s;
    int     i, len, count=0;
    struct ifconf   ifc;
    struct ifreq    ifs[NIFS], *ifr;
    struct sockaddr_in *a;
    char	*cp, *cplim;	/* used only for AIX 41 */

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
#if	defined(AFS_AIX41_ENV)
    if ( ifc.ifc_len > sizeof(ifs) ) 	/* safety check */
	ifc.ifc_len = sizeof(ifs); 
    for ( cp = (char *)ifc.ifc_buf, 
		cplim= ifc.ifc_buf+ifc.ifc_len;
		cp < cplim;
		cp += sizeof(ifr->ifr_name) + MAX(a->sin_len, sizeof(*a))){
	ifr = (struct ifreq *)cp;
#else
    for (i = 0; i < len; ++i) {
	ifr = &ifs[i];
#endif
	a = (struct sockaddr_in *) &ifr->ifr_addr;
	if (a->sin_addr.s_addr != 0 && a->sin_family == AF_INET) {
	    if ( ioctl(s, SIOCGIFFLAGS, ifr) < 0 ) {
		perror("SIOCGIFFLAGS");
		continue; /* ignore this address */
	    }
	    if (ifr->ifr_flags & IFF_LOOPBACK) {
		continue;	 /* skip aliased loopbacks as well. */
	    }
	    if ( count >= maxSize )  /* no more space */
		printf("Too many interfaces..ignoring 0x%x\n",
		       a->sin_addr.s_addr);
	    else
		buffer[count++] = a->sin_addr.s_addr;
	}
    }
    close(s);
    return count;
}

/* this function returns the total number of interface addresses
 * the buffer has to be passed in by the caller. It also returns
 * the interface mask. If AFS_USERSPACE_IP_ADDR is defined, it
 * gets the mask which is then passed into the kernel and is used
 * by afsi_SetServerIPRank().
 */
int rxi_getAllAddrMaskMtu (addrBuffer, maskBuffer, mtuBuffer, maxSize)
   afs_int32   addrBuffer[];   /* the network addrs in net byte order */
   afs_int32   maskBuffer[];   /* the subnet masks */
   afs_int32   mtuBuffer[];    /* the MTU sizes */
   int     maxSize;        /* sizeof of buffer in afs_int32 units */
{
   int     s;
   int     i, len, count=0;
   struct ifconf   ifc;
   struct ifreq    ifs[NIFS], *ifr, tempIfr;
   struct sockaddr_in *a;
   char        *cp, *cplim;    /* used only for AIX 41 */

#if !defined(AFS_USERSPACE_IP_ADDR)
   count = rx_getAllAddr(addrBuffer, 1024);
   for (i=0; i<count; i++) {
      maskBuffer[i] = htonl(0xffffffff);
      mtuBuffer[i]  = htonl(1500);
   }
   return count;
#else  /* AFS_USERSPACE_IP_ADDR */
   s = socket(AF_INET, SOCK_DGRAM, 0);
   if (s < 0) return 0;

   ifc.ifc_len = sizeof(ifs);
   ifc.ifc_buf = (caddr_t) ifs;
   i = ioctl(s, SIOCGIFCONF, &ifc);
   if (i < 0) {
      close(s);
      return 0;
   }
   len = ifc.ifc_len / sizeof(struct ifreq);
   if (len > NIFS) len = NIFS;

#if     defined(AFS_AIX41_ENV) || defined(AFS_USR_AIX_ENV)
   if ( ifc.ifc_len > sizeof(ifs) )    /* safety check */
     ifc.ifc_len = sizeof(ifs);
   for ( cp = (char *)ifc.ifc_buf,
	cplim= ifc.ifc_buf+ifc.ifc_len;
	cp < cplim;
	cp += sizeof(ifr->ifr_name) + MAX(a->sin_len, sizeof(*a))) {
      ifr = (struct ifreq *)cp;
#else
   for (i = 0; i < len; ++i) {
      ifr = &ifs[i];
#endif
      a = (struct sockaddr_in *) &ifr->ifr_addr;
      if (a->sin_addr.s_addr != 0 && a->sin_family == AF_INET) {

	 if ( ioctl(s, SIOCGIFFLAGS, ifr) < 0 ) {
	    perror("SIOCGIFFLAGS");
	    continue; /* ignore this address */
	 }
	 if (ifr->ifr_flags & IFF_LOOPBACK) {
	    continue;	 /* skip aliased loopbacks as well. */
	 }

	 if ( count >= maxSize ) { /* no more space */
	    printf("Too many interfaces..ignoring 0x%x\n", a->sin_addr.s_addr);
	    continue;
	 }

	 addrBuffer[count] = a->sin_addr.s_addr;

	 if ( ioctl(s, SIOCGIFNETMASK, (caddr_t)ifr) < 0 ) {
	    perror("SIOCGIFNETMASK");
	    maskBuffer[count] = htonl(0xffffffff);
	 } else {
	    maskBuffer[count] = (((struct sockaddr_in *)
				  (&ifr->ifr_addr))->sin_addr).s_addr;
	 }

	 mtuBuffer[count] = htonl(1500);
#ifdef SIOCGIFMTU
	 if ( ioctl(s, SIOCGIFMTU, (caddr_t)ifr) < 0) {
	    perror("SIOCGIFMTU");
	 } else {
	    mtuBuffer[count] = ifr->ifr_metric;
	 }
#endif /* SIOCGIFMTU */
#ifdef SIOCRIPMTU
	 if ( ioctl(s, SIOCRIPMTU, (caddr_t)ifr) < 0) {
	    perror("SIOCRIPMTU");
	 } else {
	    mtuBuffer[count] = ifr->ifr_metric;
	 }
#endif /* SIOCRIPMTU */

	 count++;
      }
   }
   close(s);
   return count;
#endif /* AFS_USERSPACE_IP_ADDR */
}

#endif /* ! AFS_NT40_ENV */
#endif /* !KERNEL || UKERNEL */
