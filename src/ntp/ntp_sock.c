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

RCSID("$Header: /tmp/cvstemp/openafs/src/ntp/ntp_sock.c,v 1.1.1.4 2001/07/14 22:23:06 hartmans Exp $");

#include <sys/types.h>
#include <sys/param.h>
#ifdef	AFS_SUN5_ENV
#define BSD_COMP
#endif
#include <sys/ioctl.h>
#ifdef	AFS_SUN5_ENV
#include <fcntl.h>
#endif
#include <sys/file.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <errno.h>
#include <syslog.h>
#include <stdio.h>
#include "ntp.h"

#define	MAX_INTF	10
struct intf addrs[MAX_INTF];
int nintf;

#ifdef	TEST
extern int errno;

#include "AFS_component_version_number.c"

main() {
	int i, cc, val;
	char foo[10];

	syslog(LOG_ERR, "ifconfig test");
	create_sockets(htons(43242));
	for (i = 0; i < nintf; i++) {
		printf("%d: %s fd %d  addr %s  mask %x ",
		       i, addrs[i].name, addrs[i].fd,
		       inet_ntoa(addrs[i].sin.sin_addr.s_addr),
		       ntohl(addrs[i].mask.sin_addr.s_addr));
		cc = sizeof(val);
		if (getsockopt(addrs[0].fd, SOL_SOCKET, SO_BROADCAST,
			       (char*)&val, &cc)) {
			perror("getsockopt");
			exit(1);
		}
		printf("BCAST opt %d", val);
		cc = sizeof(val);
		if (getsockopt(addrs[0].fd, SOL_SOCKET, SO_RCVBUF,
			       (char*)&val, &cc)) {
			perror("getsockopt");
			exit(1);
		}
		printf("sockbuf size = %d ", val);
		putchar('\n');
	}

	for (i=0; i < nintf; i++) {
		fprintf(stderr, "Read fd %d.. ", addrs[i].fd);
		cc = read(addrs[i].fd, foo, 10);
		fprintf(stderr, " returns %d ", cc);
		perror("read errno");
	}
}
#endif

#ifndef	SIOCGIFCONF
/*
 *  If we can't determine the interface configuration, just listen with one
 *  socket at the INADDR_ANY address.
 */
create_sockets(port)
	unsigned int port;
{
	addrs[0].sin.sin_family = AF_INET;
	addrs[0].sin.sin_port = 0;
	addrs[0].sin.sin_addr.s_addr = INADDR_ANY;
	addrs[0].sin.sin_mask.s_addr = htonl(~0);
	addrs[0].name = "wildcard";

	if ((addrs[0].fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "socket() failed: %m");
#ifdef	TEST
		perror( "socket() failed");
#endif
		exit(1);
		/*NOTREACHED*/
	}

	if (fcntl(addrs[i].fd, F_SETFL, FNDELAY) < 0) {
		syslog(LOG_ERR, "fcntl(FNDELAY) fails: %m");
#ifdef	TEST
		perror("fcntl(FNDELAY) fails");
#endif
		exit(1);
		/*NOTREACHED*/
	}
	addrs[0].sin.sin_family = AF_INET;
	addrs[0].sin.sin_port = port;
	addrs[0].if_flags = 0;
	if (bind(addrs[0].fd, (struct sockaddr *)&addrs[0].sin,
		 sizeof(addrs[0].sin)) < 0) {
		syslog(LOG_ERR, "bind() fails: %m");
#ifdef	TEST
		perror("bind fails\n");
#endif
		exit(1);
	}
	nintf = 1;
	return nintf;
}
#else
/*
 *  Grab interface configuration, and create a socket for each interface
 *  address.
 */
create_sockets(port)
	unsigned int port;
{
	char	buf[1024];
	struct	ifconf	ifc;
	struct	ifreq	ifreq, *ifr;
	int on = 1, off = 0;
	int n, i, vs;
	extern char *malloc();

	/*
	 * create pseudo-interface with wildcard address
	 */
	addrs[nintf].sin.sin_family = AF_INET;
	addrs[nintf].sin.sin_port = 0;
	addrs[nintf].sin.sin_addr.s_addr = INADDR_ANY;
	addrs[nintf].name = "wild";
	addrs[nintf].mask.sin_addr.s_addr = htonl(~0);

	nintf = 1;

	if ((vs = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "vs=socket(AF_INET, SOCK_DGRAM) %m");
#ifdef	TEST
		perror("vs=socket(AF_INET, SOCK_DGRAM)");
#endif
		exit(1);
	}
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(vs, SIOCGIFCONF, (char *)&ifc) < 0) {
		syslog(LOG_ERR, "get interface configuration: %m");
#ifdef	TEST
		perror("ioctl(SIOCGIFCONF) fails");
#endif
		exit(1);
	}
	n = ifc.ifc_len/sizeof(struct ifreq);

	for (ifr = ifc.ifc_req; n > 0; n--, ifr++) {
		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;
		ifreq = *ifr;
		if (ioctl(vs, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			syslog(LOG_ERR, "get interface flags: %m");
#ifdef	TEST
			perror("SIOCGIFFFLAGS fails");
#endif
			continue;
		}
		if ((ifreq.ifr_flags & IFF_UP) == 0)
			continue;
		addrs[nintf].if_flags = ifreq.ifr_flags;

		if (ioctl(vs, SIOCGIFADDR, (char *)&ifreq) < 0) {
			syslog(LOG_ERR, "get interface addr: %m");
#ifdef	TEST
			perror("SIOCGIFADDR fails");
#endif
			continue;
		}
		if ((addrs[nintf].name = malloc(strlen(ifreq.ifr_name)+1))
				== NULL) {
			syslog(LOG_ERR, "malloc failed");
			exit(1);
		}
		strcpy(addrs[nintf].name, ifreq.ifr_name);
		addrs[nintf].sin = *(struct sockaddr_in *)&ifreq.ifr_addr;

#ifdef SIOCGIFBRDADDR
		if (addrs[nintf].if_flags & IFF_BROADCAST) {
			if (ioctl(vs, SIOCGIFBRDADDR, (char *)&ifreq) < 0) {
				syslog(LOG_ERR, "SIOCGIFBRDADDR fails");
#ifdef	TEST
				perror("SIOCGIFBRDADDR fails");
#endif
				exit(1);
			}
#ifdef ifr_broadaddr
			addrs[nintf].bcast =
				*(struct sockaddr_in *)&ifreq.ifr_broadaddr;
#else
			addrs[nintf].bcast =
				*(struct sockaddr_in *)&ifreq.ifr_addr;
#endif
		}
#endif /* SIOCGIFBRDADDR */
#ifdef SIOCGIFNETMASK
		if (ioctl(vs, SIOCGIFNETMASK, (char *)&ifreq) < 0) {
			syslog(LOG_ERR, "SIOCGIFNETMASK fails");
#ifdef	TEST
			perror("SIOCGIFNETMASK fails");
#endif
			exit(1);
		}
		addrs[nintf].mask = *(struct sockaddr_in *)&ifreq.ifr_addr;
#endif /* SIOCGIFNETMASK */

		/* 
		 * look for an already existing source interface address.  If
		 * the machine has multiple point to point interfaces, then 
		 * the local address may appear more than once.
		 */		   
		for (i=0; i < nintf; i++)
			if (addrs[i].sin.sin_addr.s_addr == 
			    addrs[nintf].sin.sin_addr.s_addr) {
#ifdef TEST
				printf("dup interface address %s on %s\n",
					inet_ntoa(addrs[nintf].sin.sin_addr.s_addr),
					ifreq.ifr_name);
#endif		      
				goto next;
			}
		nintf++;
	   next:;
	}
	close(vs);

	for (i = 0; i < nintf; i++) {
		/* create a datagram (UDP) socket */
		if ((addrs[i].fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			syslog(LOG_ERR, "socket() failed: %m");
#ifdef	TEST
			perror("socket(AF_INET, SOCK_DGRAM) fails");
#endif
			exit(1);
			/*NOTREACHED*/
		}

		/* set SO_REUSEADDR since we will be binding the same port
		   number on each interface */
		if (setsockopt(addrs[i].fd, SOL_SOCKET, SO_REUSEADDR,
				  (char *)&on, sizeof(on))) {
#ifdef	TEST
			perror("setsockopt SO_REUSEADDR on");
#endif
			syslog(LOG_ERR, "setsockopt SO_REUSEADDR on fails: %m");
		}

		/*
		 * set non-blocking I/O on the descriptor
		 */
		if (fcntl(addrs[i].fd, F_SETFL, FNDELAY) < 0) {
			syslog(LOG_ERR, "fcntl(FNDELAY) fails: %m");
#ifdef	TEST
			perror("fcntl(F_SETFL, FNDELAY) fails");
#endif
			exit(1);
			/*NOTREACHED*/
		}

		/*
		 * finally, bind the local address address.
		 */
		addrs[i].sin.sin_family = AF_INET;
		addrs[i].sin.sin_port = port;
		if (bind(addrs[i].fd, (struct sockaddr *)&addrs[i].sin,
			 sizeof(addrs[i].sin)) < 0) {
			syslog(LOG_ERR, "bind() fails: %m");
#ifdef	TEST
			perror("bind fails");
#endif
			exit(1);
		}

		/*
		 *  Turn off the SO_REUSEADDR socket option.  It apparently
		 *  causes heartburn on systems with multicast IP installed.
		 *  On normal systems it only gets looked at when the address
		 *  is being bound anyway..
		 */
		if (setsockopt(addrs[i].fd, SOL_SOCKET, SO_REUSEADDR,
			       (char *)&off, sizeof(off))) {
			syslog(LOG_ERR, "setsockopt SO_REUSEADDR off fails: %m");
#ifdef	TEST
			perror("setsockopt SO_REUSEADDR off");
#endif
		}

#ifdef SO_BROADCAST
		/* if this interface can support broadcast, set SO_BROADCAST */
		if (addrs[i].if_flags & IFF_BROADCAST) {
			if (setsockopt(addrs[i].fd, SOL_SOCKET, SO_BROADCAST,
				       (char *)&on, sizeof(on))) {
				syslog(LOG_ERR, "setsockopt(SO_BROADCAST): %m");
#ifdef	TEST
				perror("setsockopt(SO_BROADCAST) on");
#endif
			}
		}
#endif /* SO_BROADCAST */
	}
	return nintf;
}

#endif

