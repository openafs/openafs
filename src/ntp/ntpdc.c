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

RCSID("$Header: /tmp/cvstemp/openafs/src/ntp/ntpdc.c,v 1.1.1.4 2001/07/14 22:23:06 hartmans Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <signal.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <errno.h>
#include <stdio.h>
#include <netdb.h>
#include <strings.h>
#include <arpa/inet.h>

#ifdef AFS_AIX32_ENV
#include <sys/select.h>
#endif

#include "ntp.h"

#define	WTIME	10		/* Time to wait for all responses */
#define	STIME	500000		/* usec to wait for another response */
#define	MAXPACKETSIZE 1500

extern int errno;
int debug;
int s;
int timedout;
void timeout();
int nflag, vflag;

struct sockaddr_in sin = {AF_INET};

char packet[MAXPACKETSIZE];
#ifndef	MAXHOSTNAMELEN
#define	MAXHOSTNAMELEN	64
#endif
char	LocalHostName[MAXHOSTNAMELEN+1];	/* our hostname */
char	*LocalDomain;		/* our local domain name */

#include "AFS_component_version_number.c"

main(argc, argv)
	int argc;
	char *argv[];
{
	char *p;
	int on = 48*1024;

#ifdef	AFS_AIX32_ENV
	/*
	 * The following signal action for AIX is necessary so that in case of a 
	 * crash (i.e. core is generated) we can include the user's data section 
	 * in the core dump. Unfortunately, by default, only a partial core is
	 * generated which, in many cases, isn't too useful.
	 */
	struct sigaction nsa;
    
	sigemptyset(&nsa.sa_mask);
	nsa.sa_handler = SIG_DFL;
	nsa.sa_flags = SA_FULLDUMP;
	sigaction(SIGSEGV, &nsa, NULL);
#endif
	(void) gethostname(LocalHostName, sizeof LocalHostName);
	if (p = index(LocalHostName, '.')) {
		*p++ = '\0';
		LocalDomain = p;
	}
	else
		LocalDomain = "";

	if (argc < 2) {
usage:
		printf("usage: %s [ -v ][ -n ] hosts...\n", argv[0]);
		exit(1);
	}

	argv++, argc--;
	if (*argv[0] == '-') {
		switch (argv[0][1]) {
		case 'n':
			nflag++;
			break;
		case 'v':
			vflag++;
			break;
		default:
			goto usage;
		}
		argc--, argv++;
	}
	if (argc > 1)
		printf("--- %s ---\n", *argv);

	while (argc > 0) {
		/*
		 * Get a new socket each time - this will cause us to ignore
		 * packets from the previously queried host.
		 */
		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s < 0) {
			perror("socket");
			exit(2);
		}
#ifdef	SO_RCVBUF
		if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
				(char *) &on, sizeof (on)) < 0) {
			fprintf(stderr, "setsockopt SO_RCVBUF\n");
		}
#endif
		if (query(*argv))
			answer(*argv);
		close(s);
		argv++;
		if (argc-- > 1)
			printf("--- %s ---\n", *argv);
	}

}

answer(host)
	char *host;
{
	register struct ntpinfo *msg = (struct ntpinfo *) packet;
	register struct clockinfo *n;
	struct sockaddr_in from;
	int fromlen = sizeof(from);
	int count, cc;
	fd_set bits;
	struct timeval shorttime;
	int first = 1;
	afs_int32 replies = 0;

	/*
	 * Listen for returning packets; may be more than one packet per
	 * host. 
	 */
	FD_ZERO(&bits);
	FD_SET(s, &bits);
	shorttime.tv_sec = 0;
	shorttime.tv_usec = STIME;
	(void) signal(SIGALRM, timeout);
	(void) alarm(WTIME);
	timedout = 0;
	while ((first || replies) && 
	       (!timedout || select(FD_SETSIZE, &bits, (fd_set *) 0,
				    (fd_set *) 0, &shorttime) > 0)) {
		if ((cc = recvfrom(s, packet, sizeof(packet), 0,
			     (struct sockaddr *)&from, &fromlen)) <= 0) {
			if (cc == 0 || errno == EINTR)
				continue;
			fflush(stdout);
			perror(host);
			(void) close(s);
			return;
		}
		FD_SET(s, &bits);

		if (msg->type != INFO_REPLY)
			return;

		if (msg->version != NTPDC_VERSION) {
			printf("ntpd(%d) - ntpdc(%d) version mismatch\n",
			       msg->version, NTPDC_VERSION);
			alarm(0);
			return;
		}

		if (first) {
			first = 0;
			replies = (1L << msg->npkts) - 1;
			if (!vflag) {
				printf("   (rem)  Address   (lcl)      Strat Poll Reach    Delay   Offset    Disp\n");
				printf("==========================================================================\n");
			}
		}
		replies &= ~(1L << msg->seq);
		n = (struct clockinfo *)&msg[1];
		for (count = msg->count; count > 0; count--) {
			if(vflag)
				print_verbose(n);
			else
				print_terse(n);
			n++;
		}
	}
	alarm(0);
	if (replies)
		printf("Timed out waiting for replies\n");
}

int
query(host)
	char *host;
{
	struct sockaddr_in watcher;
	register struct ntpdata *msg = (struct ntpdata *) packet;
	struct hostent *hp;
	static struct servent *sp = NULL;
	afs_int32 HostAddr;

	bzero((char *) &watcher, sizeof(watcher));
	watcher.sin_family = AF_INET;
	HostAddr = inet_addr(host);
	watcher.sin_addr.s_addr = (afs_uint32) HostAddr;
	if (HostAddr == -1) {
		hp = gethostbyname(host);
		if (hp == 0) {
			fprintf(stderr,"%s: unknown\n", host);
			return 0;
		}
		bcopy(hp->h_addr, (char *) &watcher.sin_addr, hp->h_length);
	}
	sp = getservbyname("ntp", "udp");
	if (sp == 0) {
		fprintf(stderr,"udp/ntp: service unknown, using default %d\n",
			NTP_PORT);
		watcher.sin_port = htons(NTP_PORT);
	} else
		watcher.sin_port = sp->s_port;
	msg->status = NTPVERSION_1;
	msg->stratum = INFO_QUERY;
	if (connect(s, (struct sockaddr *) &watcher, sizeof(watcher))) {
		perror("connect");
		return 0;
	}
	if (send(s, packet, sizeof(struct ntpdata), 0) < 0) {
		perror("send");
		return 0;
	}
	return 1;
}

void timeout()
{
	timedout = 1;
}

print_terse (n)
	struct clockinfo *n;
{
	int i;
	double offset[PEER_SHIFT], delay[PEER_SHIFT], dsp,del,off;
	char c;
	char *cvthname();
	int flags;

	sin.sin_addr.s_addr = n->net_address;
	for (i = 0; i < PEER_SHIFT; i++) {
		delay[i] = (double) ((afs_int32) (ntohl(n->info_filter.delay[i])/1000.0));
		offset[i] = (double) ((afs_int32) (ntohl(n->info_filter.offset[i])/1000.0));
	}
	dsp = (afs_int32) ntohl(n->estdisp);		/* leave in milliseconds */
	del = (afs_int32) ntohl(n->estdelay);	/* leave in milliseconds */
	off = (afs_int32) ntohl(n->estoffset);	/* leave in milliseconds */
	c = ' ';
	flags = ntohs(n->flags);
	if (flags & PEER_FL_CONFIG)
		c = '-';		/* mark pre-configured */
	if (flags & PEER_FL_SANE)
		c = '.';		/* passed sanity check */
	if (flags & PEER_FL_CANDIDATE)
		c = '+';		/* made candidate list */
	if (flags & PEER_FL_SELECTED)
		c = '*';		/* mark peer selection */
	sin.sin_addr.s_addr = n->net_address;
	printf("%c%-15.15s ", c, cvthname(&sin));
	sin.sin_addr.s_addr = n->my_address;
	printf("%-16.16s %2d %4d  %03o  %8.1f %8.1f %8.1f\n",
	       sin.sin_addr.s_addr ? inet_ntoa(sin.sin_addr) : "wildcard", 
	       n->stratum, (int)ntohl((afs_uint32)n->timer), 
	       ntohs(n->reach) & SHIFT_MASK, del, off, dsp);
}	

print_verbose(n)
	struct clockinfo *n;
{
	int i;
	struct in_addr clock_host;
	double offset[PEER_SHIFT], delay[PEER_SHIFT], dsp,del,off;
	char *cvthname();

	sin.sin_addr.s_addr = n->net_address;
	for (i = 0; i < PEER_SHIFT; i++) {
		delay[i] = (double) (afs_int32) ntohl(n->info_filter.delay[i]);
		offset[i] = (double) (afs_int32) ntohl(n->info_filter.offset[i]);
	}
	dsp = (double) ((afs_int32) ntohl(n->estdisp));	/* in milliseconds */
	del = (double) ((afs_int32) ntohl(n->estdelay));	/* in milliseconds */
	off = (double) ((afs_int32) ntohl(n->estoffset));	/* in milliseconds */
	printf("Neighbor address %s port:%d",
	       inet_ntoa(sin.sin_addr), (int)ntohs(n->port));
	sin.sin_addr.s_addr = n->my_address;
	printf("  local address %s\n", inet_ntoa(sin.sin_addr));
	printf("Reach: 0%o stratum: %d, precision: %d\n",
	       ntohs(n->reach) & SHIFT_MASK, n->stratum, n->precision);
	printf("dispersion: %f, flags: %x, leap: %x\n",
	       dsp,
	       ntohs(n->flags),
	       n->leap);
	if (n->stratum == 1 || n->stratum == 0) {
		printf("Reference clock ID: %.4s", (char *)&n->refid);
	} else {
		clock_host.s_addr = (afs_uint32) n->refid;
		printf("Reference clock ID: [%s]", inet_ntoa(clock_host));
	}
	printf(" timestamp: %08lx.%08lx\n", ntohl(n->reftime.int_part),
	       ntohl(n->reftime.fraction));

	printf("hpoll: %d, ppoll: %d, timer: %d, sent: %d received: %d dropped: %d\n",
	       n->hpoll, n->ppoll,
	       (int)ntohl((afs_uint32)n->timer),
	       (int)ntohl(n->pkt_sent),
	       (int)ntohl(n->pkt_rcvd),
	       (int)ntohl(n->pkt_dropped));
	printf("Delay(ms)  ");
	for (i = 0; i < PEER_SHIFT; i++)
		printf("%7.2f ", delay[i]);
	printf("\n");
	printf("Offset(ms) ");
	for (i = 0; i < PEER_SHIFT; i++)
		printf("%7.2f ", offset[i]);
	printf("\n");
	printf("\n\tdelay: %f offset: %f dsp %f\n", del, off, dsp);
	printf("\n");
}
/*
 * Return a printable representation of a host address.
 */
char *
cvthname(f)
	struct sockaddr_in *f;
{
	struct hostent *hp;
	register char *p;
	extern char *inet_ntoa();

	if (f->sin_family != AF_INET) {
		printf("Malformed from address\n");
		return ("???");
	}
	if (!nflag)
		hp = gethostbyaddr((char *) &f->sin_addr,
				   sizeof(struct in_addr),
				   f->sin_family);
	else
		return (inet_ntoa(f->sin_addr));

	if (hp == 0)
		return (inet_ntoa(f->sin_addr));

	if ((p = index(hp->h_name, '.')) && strcmp(p + 1, LocalDomain) == 0)
		*p = '\0';
	return (hp->h_name);
}
