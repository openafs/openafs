/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	lint
#endif /* lint */
/*
 * Revision 2.1  90/08/07  19:22:00
 * Start with clean version to sync test and dev trees.
 * 
 * Revision 1.9  90/06/20  20:10:20
 * dropped an `#endif' when I merged.
 * 
 * Revision 1.8  90/06/13  20:04:39
 * rs_aix31.
 * 
 * Revision 1.7  89/12/22  20:29:41
 * hp/ux specific (initial port to it); Added <afs/param.h> and special include files for HPUX and misc other changes
 * 
 * Revision 1.6  89/12/11  14:25:09
 * Added code to support AIX 2.2.1.
 * 
 * Revision 1.5  89/05/24  12:24:59
 * Latest May 18, Version 4.3 release from UMD.
 * 
 * Revision 3.4.1.6  89/05/18  18:21:29
 * A few cosmetic changes in ntp.c for the case when udp/ntp is not in the
 * /etc/services file.
 * 
 * Revision 3.4.1.5  89/05/03  15:09:53
 * Fix minor problem in ntp.c to get sin_family set in the proper place.
 * 
 * Revision 3.4.1.4  89/04/07  18:04:49
 * Removed unused variables from ntp.c program.
 * 
 * Revision 3.4.1.3  89/03/22  18:29:22
 * patch3: Use new RCS headers.
 * 
 * Revision 3.4.1.2  89/03/22  17:51:13
 * Use a connect UDP socket so we can pick up ICMP error messages.
 * 
 * Revision 3.4.1.1  89/03/20  00:02:32
 * patch1: Shorten timeout interval and clean up timeout message.
 * 
 * Revision 3.4  89/03/17  18:36:54
 * Latest test release.
 * 
 * Revision 3.3.1.1  89/03/17  18:23:14
 * Fix code that sets time.
 * 
 * Revision 3.3  89/03/15  14:19:30
 * New baseline for next release.
 * 
 * Revision 3.2.1.2  89/03/15  13:45:13
 * Fix use of NTP_PORT when getservbyname() fails.  Use "%f" in printf format
 * strings rather than "%lf".
 * 
 * 
 * Revision 3.2.1.1  89/03/10  11:26:23
 * Add (primitive) facility to set the time from an NTP timer server,
 * much the same way as various UDP/TIME based programs do.  The
 * default output format is a one line listing of delay, offset and time.  The
 * previous, longer format can be had by using the -v option.
 * 
 * Revision 3.2  89/03/07  18:20:55
 * Cleaned up displays of NTP header fields, also dumping in useful formats
 * (floating point and ctime strings) as well as hex fields.
 * 
 * Revision 3.1.1.1  89/02/15  08:53:30
 * Bug fixes to last released version.
 * 
 * 
 * Revision 3.1  89/01/30  14:43:05
 * Second UNIX NTP test release.
 * 
 * Revision 3.0  88/12/12  15:56:10
 * Test release of new UNIX NTP software.  This version should conform to the
 * revised NTP protocol specification.
 * 
 */

/*
 * This program expects a list of host names.  It will send off a
 * network time protocol packet and print out the replies on the
 * terminal.
 * 
 * Example:
 *
 *  % ntp umd1.umd.edu
 *  Packet from: [128.8.10.1]
 *  Leap 0, version 1, mode Server, poll 6, precision -10 stratum 1 (WWVB)
 *  Synch Distance is 0000.1999  0.099991
 *  Synch Dispersion is 0000.0000  0.000000
 *  Reference Timestamp is a7bea6c3.88b40000 Tue Mar  7 14:06:43 1989
 *  Originate Timestamp is a7bea6d7.d7e6e652 Tue Mar  7 14:07:03 1989
 *  Receive Timestamp is   a7bea6d7.cf1a0000 Tue Mar  7 14:07:03 1989
 *  Transmit Timestamp is  a7bea6d8.0ccc0000 Tue Mar  7 14:07:04 1989
 *  Input Timestamp is     a7bea6d8.1a77e5ea Tue Mar  7 14:07:04 1989
 *  umd1: delay:0.019028 offset:-0.043890
 *  Tue Mar  7 14:07:04 1989
 *
 */

#include <afs/param.h>
#include <stdio.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>

#ifdef AFS_AIX32_ENV
#include <sys/select.h>
#endif

#include <errno.h>
#include "ntp.h"

char *modename[8] = {
	"Unspecified",
	"Symmetric Active",
	"Symmetric Passive",
	"Client",
	"Server",
	"Broadcast",
	"Reserved-1",
	"Reserved-2"
	};

#define RETRY_COUNT	2	/* number of times we want to retry */
#define TIME_OUT        10	/* time to wait for reply, in secs */


struct sockaddr_in sin = {AF_INET};
struct sockaddr_in dst = {AF_INET};
struct servent *sp;
extern double ul_fixed_to_double(), s_fixed_to_double();
extern int errno;
int set, verbose, force;
int debug;
extern int optind;

#include "AFS_component_version_number.c"

main(argc, argv)
	int argc;
	char *argv[];
{
	struct hostent *hp;
	struct in_addr clock_host;
	struct l_fixedpt in_timestamp;
	static struct ntpdata ntp_data;
	struct ntpdata *pkt = &ntp_data;
	struct timeval tp, timeout;
	int host, n, retry, s;
	fd_set readfds;
	int dstlen = sizeof(dst);
	double t1, t2, t3, t4, offset, delay;
	char ref_clock[5];
	time_t net_time;

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
	ref_clock[4] = '\0';
	timeout.tv_sec = TIME_OUT;
	timeout.tv_usec = 0;
	retry = RETRY_COUNT;

	sp = getservbyname("ntp", "udp");
	if (sp == NULL) {
		fprintf(stderr, "udp/ntp: service unknown; using default %d\n",
			NTP_PORT);
		dst.sin_port = htons(NTP_PORT);
	} else
		dst.sin_port = sp->s_port;

	dst.sin_family = AF_INET;
	while ((n = getopt(argc, argv, "vsf")) != EOF) {
		switch (n) {
		case 'v':
			verbose = 1;
			break;
		case 's':
			set = 1;
			break;
		case 'f':
			force = 1;
			break;
		}
	}
	for (host = optind; host < argc; ++host) {
		afs_int32	HostAddr;

		if (argv[host] == NULL)
			continue;

		hp = NULL;
		HostAddr = inet_addr(argv[host]);
		dst.sin_addr.s_addr = (afs_uint32) HostAddr;
		if (HostAddr == -1) {
			hp = gethostbyname(argv[host]);
			if (hp == NULL) {
				fprintf(stderr, "\nNo such host: %s\n",
					argv[host]);
				continue;
			}
			bcopy(hp->h_addr, (char *) &dst.sin_addr,hp->h_length);
		}

		bzero((char *)pkt, sizeof(ntp_data));

		pkt->status = NTPVERSION_1 | NO_WARNING | MODE_CLIENT;
		pkt->stratum = UNSPECIFIED;
		pkt->ppoll = 0;

		if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("ntp socket");
			exit(1);
		}

		FD_ZERO(&readfds);
		FD_SET(s, &readfds);	/* since it's always modified on ret */

		if (connect(s, (struct sockaddr *)&dst, dstlen)) {
			perror("connect");
			exit(1);
		}

		/*
		 * Needed to fill in the time stamp fields
		 */
		(void) gettimeofday(&tp, (struct timezone *) 0);
		tstamp(&pkt->xmt, &tp);

		if (send(s, (char *) pkt, sizeof(ntp_data), 0) < 0) {
			perror("send");
			exit(1);
		}

		/*
		 * Wait for the reply by watching the file descriptor 
		 */
		if ((n = select(FD_SETSIZE, (fd_set *) & readfds, (fd_set *) 0,
				(fd_set *) 0, &timeout)) < 0) {
			perror("ntp select");
			exit(1);
		}

		if (n == 0) {
			fprintf(stderr,"*Timeout*\n");
			if (--retry)
				--host;
			else {
				fprintf(stderr,"Host %s is not responding\n",
				       argv[host]);
				retry = RETRY_COUNT;
			}
			continue;
		}
		if ((recvfrom(s, (char *) pkt, sizeof(ntp_data), 0,
			      (struct sockaddr *) &sin, &dstlen)) < 0) {
			perror("recvfrom");
			exit(1);
		}
		(void) gettimeofday(&tp, (struct timezone *) 0);
		tstamp(&in_timestamp, &tp);

		close(s);
		if (verbose) {
			printf("Packet from: [%s]\n", inet_ntoa(sin.sin_addr));
			printf("Leap %d, version %d, mode %s, poll %d, precision %d stratum %d",
			       (pkt->status & LEAPMASK) >> 6, 
			       (pkt->status & VERSIONMASK) >> 3,
			       modename[pkt->status & MODEMASK],
			       pkt->ppoll, pkt->precision, pkt->stratum);
			switch (pkt->stratum) {
			case 0:
			case 1:
				(void) strncpy(ref_clock, (char *) &pkt->refid, 4);
				ref_clock[4] = '\0';
				printf(" (%s)\n", ref_clock);
				break;
			default:
				clock_host.s_addr = (afs_uint32) pkt->refid;
				printf(" [%s]\n", inet_ntoa(clock_host));
				break;
			}
			printf("Synch Distance is %04X.%04x  %f\n",
			       ntohs(pkt->distance.int_part),
			       ntohs(pkt->distance.fraction),
			       s_fixed_to_double(&pkt->distance));

			printf("Synch Dispersion is %04X.%04x  %f\n",
			       ntohs(pkt->dispersion.int_part),
			       ntohs(pkt->dispersion.fraction),
			       s_fixed_to_double(&pkt->dispersion));

			net_time = ntohl(pkt->reftime.int_part) - JAN_1970;
			printf("Reference Timestamp is %08lx.%08lx %s",
			       ntohl(pkt->reftime.int_part),
			       ntohl(pkt->reftime.fraction),
			       ctime(&net_time));

			net_time = ntohl(pkt->org.int_part) - JAN_1970;
			printf("Originate Timestamp is %08lx.%08lx %s",
			       ntohl(pkt->org.int_part),
			       ntohl(pkt->org.fraction),
			       ctime(&net_time));

			net_time = ntohl(pkt->rec.int_part) - JAN_1970;
			printf("Receive Timestamp is   %08lx.%08lx %s",
			       ntohl(pkt->rec.int_part),
			       ntohl(pkt->rec.fraction),
			       ctime(&net_time));

			net_time = ntohl(pkt->xmt.int_part) - JAN_1970;
			printf("Transmit Timestamp is  %08lx.%08lx %s",
			       ntohl(pkt->xmt.int_part),
			       ntohl(pkt->xmt.fraction),
			       ctime(&net_time));
		}
		t1 = ul_fixed_to_double(&pkt->org);
		t2 = ul_fixed_to_double(&pkt->rec);
		t3 = ul_fixed_to_double(&pkt->xmt);
		t4 = ul_fixed_to_double(&in_timestamp);

		net_time = ntohl(in_timestamp.int_part) - JAN_1970;
		if (verbose) 
			printf("Input Timestamp is     %08lx.%08lx %s",
			       ntohl(in_timestamp.int_part),
			       ntohl(in_timestamp.fraction), ctime(&net_time));

		delay = (t4 - t1) - (t3 - t2);
		offset = (t2 - t1) + (t3 - t4);
		offset = offset / 2.0;
		printf("%.20s: delay:%f offset:%f  ",
		       hp ? hp->h_name : argv[host],
		       delay, offset);
		net_time = ntohl(pkt->xmt.int_part) - JAN_1970 + delay;
		fputs(ctime(&net_time), stdout);
		(void)fflush(stdout);

		if (!set)
			continue;

		if ((offset < 0 ? -offset  : offset) > WAYTOOBIG && !force) {
			fprintf(stderr, "Offset too large - use -f option to force clock set.\n");
			continue;
		}

		if (pkt->status & LEAPMASK == ALARM) {
			fprintf(stderr, "Can't set time from %s - unsynchronized\n",
				argv[host]);
			continue;
		}

		/* set the clock */
		gettimeofday(&tp, (struct timezone *) 0);
		offset += tp.tv_sec;
		offset += tp.tv_usec / 1000000.0;
		tp.tv_sec = offset;
		tp.tv_usec = (offset - tp.tv_sec) * 1000000.0;

		if (settimeofday(&tp, (struct timezone *) 0)) {
			perror("Can't set time (settimeofday)");
		} else
			set = 0;
	}			/* end of for each host */
	exit(0);
}				/* end of main */
