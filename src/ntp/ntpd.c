#ifndef	lint
#endif /* lint */

/*
 * Revision 1.2  1993/04/11  17:35:40
 * define NOSWAP option on sgi
 *
 * Revision 1.1  1992/09/21  16:35:59
 *
 * Revision 2.6  91/08/13  12:28:10
 * Expect HP-UX to need large BOGUS drift comp value.
 * 
 * Revision 2.5  90/12/03  14:19:34
 * Add some miscellaneous debugging and checking stmts.
 * In query_mode, peer->sock needed better array bounds checking.
 * 
 * Revision 2.4  90/10/08  15:44:45
 * Add -f switch that prevents forking.
 * Also fix syslog message to use %s instead of %d!
 * 
 * Revision 2.3  90/09/27  16:57:07
 * Bring afs.dev in sync with afs.test
 * 
 * Revision 2.2  90/09/19  16:32:05
 * Add parameter to set dosynctodr kernel variable.  Also subroutinize the
 *   kernel setting code to make this easy.
 * Call MeasurePrecision to get real precision.
 * On HP set daemon to run at real time priority.
 * Output floating point version of timestamps in dump_pkt.
 * Also on HP ignore zero length packets.
 * Call syslog w/ correct parameters.
 * 
 * Revision 2.1  90/08/07  19:23:11
 * Start with clean version to sync test and dev trees.
 * 
 * Revision 1.22  90/06/13  20:09:36
 * rs_aix31
 * 
 * Revision 1.21  90/05/24  16:17:24
 * Moved timeout interval bias into ntp_adjust.
 * Changed bogus drift compensation from 2.0 to 10.0.
 * 
 * Revision 1.20  90/05/21  13:48:11
 * Various changed for AIX:
 * 1. #define BSD_REMAP_SIGNAL_TO_SIGVEC so signal works right.
 * 2. skew the CLOCK_ADJ so it doesn't happen same time every second.
 * 3. Up the bogus drift_comp value from 1.0 to 2.0.
 * 
 * Revision 1.19  89/12/22  20:32:11
 * hp/ux specific (initial port to it); Added <afs/param.h> and special include files for HPUX and misc other changes
 * 
 * Revision 1.18  89/12/11  14:25:51
 * Added code to support AIX 2.2.1.
 * 
 * Revision 1.17  89/05/24  12:26:23
 * Latest May 18, Version 4.3 release from UMD.
 * 
 * Revision 3.4.1.9  89/05/18  18:30:17
 * Changes in ntpd.c for reference clock support.  Also, a few diddles to
 * accomodate the NeXT computer system that has a slightly different nlist.h
 * 
 * Revision 3.4.1.8  89/05/03  15:16:17
 * Add code to save the value of the drift compensation register to a file every
 * hour.  Add additional configuration file directives which can specify the same
 * information as on the command line.
 * 
 * Revision 3.4.1.7  89/04/10  15:58:45
 * Add -l option to enable logging of clock adjust messages.
 * 
 * Revision 3.4.1.6  89/04/07  19:09:04
 * Added NOSWAP code for Ultrix systems to lock NTP process in memory.  Deleted
 * unused variable in ntpd.c
 * 
 * Revision 3.4.1.5  89/03/31  16:37:49
 * Add support for "trusting" directive in NTP configuration file.  It allows 
 * you to specify at run time if non-configured peers will be synced to.
 * 
 * Revision 3.4.1.4  89/03/29  12:30:46
 * peer->mode has been renamed peer->hmode.  Drop PEER_FL_SYNC since the
 * PEER_FL_CONFIG flag means much the same thing.
 * 
 * Revision 3.4.1.3  89/03/22  18:29:41
 * patch3: Use new RCS headers.
 * 
 * Revision 3.4.1.2  89/03/22  18:03:17
 * The peer->refid field was being htonl()'ed when it was already in network
 * byte order.
 * 
 * Revision 3.4.1.1  89/03/20  00:12:10
 * patch1: Diddle syslog messages a bit.  Handle case of udp/ntp not being
 * patch1: defined in /etc/services.  Compute default value for tickadj if
 * patch1: the change-kernel-tickadj flag is set, but no tickadj directive
 * patch1: is present in the configuration file.
 * 
 * Revision 3.4  89/03/17  18:37:11
 * Latest test release.
 * 
 * Revision 3.3.1.1  89/03/17  18:26:32
 * 1
 * 
 * Revision 3.3  89/03/15  14:19:56
 * New baseline for next release.
 * 
 * Revision 3.2.1.2  89/03/15  13:59:50
 * Initialize random number generator.  The ntpdc query_mode() routine has been
 * revised to send more peers per packet, a count of the total number of peers
 * which will be transmited, the number of packets to be transmitted, and a 
 * sequence number for each packet.  There is a new version number for the
 * ntpdc query packets, which is now symbolically defined in ntp.h
 * 
 * Revision 3.2.1.1  89/03/10  12:27:41
 * Removed reference to HUGE, and replaced it by a suitable large number.  Added
 * some #ifdef DEBUG .. #endif around some debug code that was missing.  Display
 * patchlevel along with version.
 * 
 * Revision 3.2  89/03/07  18:26:30
 * New version of the UNIX NTP daemon based on the 6 March 1989 draft of the
 * new NTP protcol spec.  A bunch of cosmetic changes.  The peer list is
 * now doublely linked, and a subroutine (enqueue()) replaces the ENQUEUE
 * macro used previously.
 * 
 * Revision 3.1.1.1  89/02/15  08:58:46
 * Bugfixes to released version.
 * 
 * 
 * Revision 3.1  89/01/30  14:43:14
 * Second UNIX NTP test release.
 * 
 * Revision 3.0  88/12/12  15:56:38
 * Test release of new UNIX NTP software.  This version should conform to the
 * revised NTP protocol specification.
 * 
 */

#include <afs/param.h>
#if defined(AFS_SGI_ENV)
#define NOSWAP 1
#endif
#include <stdio.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#ifdef	AFS_SUN5_ENV
#include <signal.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>
#ifdef	AFS_SUN5_ENV
#define BSD_COMP
#endif
#include <sys/ioctl.h>
#include <sys/resource.h>
#ifdef	AFS_SUN5_ENV
#include <fcntl.h>
#endif
#include <sys/file.h>
#ifdef NOSWAP
#include <sys/lock.h>
#endif

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>
#include <errno.h>
#include <syslog.h>
#ifndef	LOG_NTP
#define	LOG_NTP	LOG_DAEMON
#endif
#ifdef	AFS_SUN5_ENV
#include <libelf.h>
#endif
#include <nlist.h>

#ifdef AFS_AIX32_ENV
#include <sys/select.h>
#include <sys/signal.h>
#endif

#ifdef	AFS_HPUX_ENV
#include <signal.h>
#endif

#include "ntp.h"
#include "patchlevel.h"

#define	TRUE	1
#define	FALSE	0

struct sockaddr_in dst_sock = {AF_INET};

struct servent *servp;
struct list peer_list;

struct itimerval it;
struct itimerval *itp = &it;
struct timeval tv;
char *prog_name;

char *conf = NTPINITFILE;
char *driftcomp_file = NTPDRIFTCOMP;
static int drift_fd = -1;

#ifdef	DEBUG
int debug = 0;
#endif

int	tickadj = 0;
int	dotickadj = 0;
int	dosynctodr = 0;

#ifdef	NOSWAP
int	noswap = 0;
#endif

int doset = 1;
int ticked;
int selfds;
int trusting = 1;
int logstats;

double WayTooBig = WAYTOOBIG;
afs_uint32 clock_watchdog;

struct ntpdata ntpframe;
struct sysdata sys;

extern int errno;
extern char *malloc(), *ntoa();
extern double s_fixed_to_double(), ul_fixed_to_double();

void finish(), timeout(), tock(), make_new_peer(), init_ntp(), initialize(),
	hourly();
extern void transmit(), process_packet(), clock_update(),
	clear(), clock_filter(), select_clock();

extern void init_logical_clock();

#if defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)
#define SETTICKADJ
#endif

#if !defined(AFS_SUN5_ENV)
#define INIT_KERN_VARS
#endif

#if defined(INIT_KERN_VARS)
void init_kern_vars();
#endif	/* INIT_KERN_VARS */

#include "AFS_component_version_number.c"

main(argc, argv)
	int argc;
	char *argv[];
{
	struct sockaddr_in *dst = &dst_sock;
	struct ntpdata *pkt = &ntpframe;
	fd_set readfds, tmpmask;
	int dstlen = sizeof(struct sockaddr_in);
	int cc;
	int dontFork = 0;
	register int i;
	extern char *optarg;
	extern int atoi();

#if	defined(DEBUG) && defined(SIGUSR1) && defined(SIGUSR2)
	void incdebug(), decdebug();
#endif

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
	initialize();		/* call NTP protocol initialization first,
				   then allow others to override default
				   values */
	prog_name = argv[0];
	while ((cc = getopt(argc, argv, "a:c:dD:lstrnf")) != EOF) {
		switch (cc) {
		case 'a':
			if (strcmp(optarg, "any") == 0)
				WayTooBig = 10e15;
			else
				WayTooBig = atof(optarg);
			break;

		case 'd':
#ifdef	DEBUG
			debug++;
#else
			fprintf(stderr, "%s: not compiled with DEBUG\n",
				prog_name);
#endif
			break;

		case 'D':
#ifdef	DEBUG
			debug = atoi(optarg);
#else
			fprintf(stderr, "%s: not compiled with DEBUG\n",
				prog_name);
#endif
			break;

		case 's':
			doset = 0;
			break;

		case 't':
#ifdef	SETTICKADJ
			dotickadj++;
#else
			fprintf(stderr, "%s: not compiled to set tickadj\n",
				prog_name);
#endif
			break;

		case 'r':
#ifdef	SETTICKADJ
			dosynctodr++;
#else
			fprintf(stderr, "%s: not compiled to set kernel variables\n",
				prog_name);
#endif
			break;

		case 'n':
#ifdef	NOSWAP
			noswap = 1;
#else
			fprintf(stderr, "%s: not compiled for noswap\n",
				prog_name);
#endif
			break;

		case 'l':
			logstats = 1;
			break;

		case 'c':
			conf = optarg;
			break;

		case 'f':
			dontFork = 1;
			break;

		default:
			fprintf(stderr, "ntpd: -%c: unknown option\n", cc);
			break;
		}
	}

#ifdef	DEBUG
	if (!debug && !dontFork)
#else
 	if (!dontFork)
#endif
	    {
		if (fork())
			exit(0);

		{
			int s;
			for (s = getdtablesize(); s >= 0; s--)
				(void) close(s);
			(void) open("/", 0);
			(void) dup2(0, 1);
			(void) dup2(0, 2);
			(void) setpgrp(0, getpid());
			s = open("/dev/tty", 2);
			if (s >= 0) {
#ifndef	AFS_HPUX_ENV
				(void) ioctl(s, (afs_uint32) TIOCNOTTY, (char *) 0);
#endif
				(void) close(s);
			}
		}
	}

	openlog("ntpd", LOG_PID | LOG_NDELAY, LOG_NTP);
#ifdef	DEBUG
	if (debug)
		setlogmask(LOG_UPTO(LOG_DEBUG));
	else
#endif	/* DEBUG */
		setlogmask(LOG_UPTO(LOG_INFO));

	syslog(LOG_NOTICE, "%s version $Revision: 1.1 $", prog_name);
	syslog(LOG_NOTICE, "patchlevel %d", PATCHLEVEL);

#ifdef	DEBUG
	if (debug)
		printf("%s version $Revision: 1.1 $ patchlevel %d\n",
		       prog_name, PATCHLEVEL);
#endif
#if defined(AFS_AIX_ENV)
	(void) nice(-10);
#else
#if defined(AFS_HPUX_ENV)
	(void) rtprio (0, 90);
#else
	(void) setpriority(PRIO_PROCESS, 0, -10);
#endif
#endif

#ifdef	NOSWAP
	if (noswap)
		if (plock(PROCLOCK) != 0)  {
			syslog(LOG_ERR, "plock() failed: %m");
#ifdef	DEBUG
			if (debug)
				perror("plock() failed");
#endif
		}
#endif

	servp = getservbyname("ntp", "udp");
	if (servp == NULL) {
		syslog(LOG_CRIT, "udp/ntp: service unknown, using default %d",
		       NTP_PORT);
		(void) create_sockets(htons(NTP_PORT));
	} else
		(void) create_sockets(servp->s_port);


	peer_list.head = peer_list.tail = NULL;
	peer_list.members = 0;

	init_ntp(conf);
#if defined(INIT_KERN_VARS)
	init_kern_vars();
#endif	/* INIT_KERN_VARS */
	init_logical_clock();

	/*
	 * Attempt to open for writing the file for storing the drift comp
	 * register.  File must already exist for snapshots to be taken.
	 */
	if ((i = open(driftcomp_file, O_WRONLY|O_CREAT, 0644)) >= 0) {
		drift_fd = i;
	}
	(void) gettimeofday(&tv, (struct timezone *) 0);
	srand(tv.tv_sec);

	FD_ZERO(&tmpmask);
	for (i = 0; i < nintf; i++) {
		FD_SET(addrs[i].fd, &tmpmask);
#ifdef	DEBUG
		if (debug>2) {
			if (addrs[i].if_flags & IFF_BROADCAST)
				printf("Addr %d: %s fd %d %s broadcast %s\n",
				       i, addrs[i].name, addrs[i].fd,
				       ntoa(addrs[i].sin.sin_addr),
				       ntoa(addrs[i].bcast.sin_addr));
			else
				printf("Addr %d: %s fd %d %s\n", i,
				       addrs[i].name, addrs[i].fd,
				       ntoa(addrs[i].sin.sin_addr));
		}
#endif
	}

	(void) signal(SIGINT, finish);
	(void) signal(SIGTERM, finish);
	(void) signal(SIGALRM, tock);
#if	defined(DEBUG) && defined(SIGUSR1) && defined(SIGUSR2)
	(void) signal(SIGUSR1, incdebug);
	(void) signal(SIGUSR2, decdebug);
#endif
	itp->it_interval.tv_sec = (1<<CLOCK_ADJ);
	itp->it_interval.tv_usec = 0;
	itp->it_value.tv_sec = 1;
	itp->it_value.tv_usec = 0;

	/*
	 * Find highest fd in use.  This might save a few microseconds in
	 * the select system call.
	 */
	for (selfds = FD_SETSIZE - 1; selfds; selfds--)
		if (FD_ISSET(selfds, &tmpmask))
			break;
#ifdef	DEBUG
	if (debug > 2)
		printf("Highest fd in use is %d\n", selfds);
	if (!selfds) abort();
#endif
	selfds++;

	(void) setitimer(ITIMER_REAL, itp, (struct itimerval *) 0);

	for (;;) {		/* go into a finite but hopefully very long
				 * loop */
		int nfds;

		readfds = tmpmask;
		nfds = select(selfds, &readfds, (fd_set *) 0, (fd_set *) 0,
						(struct timeval *) 0);
		(void) gettimeofday(&tv, (struct timezone *) 0);

		for(i = 0; i < nintf && nfds; i++) {
			if (!FD_ISSET(addrs[i].fd, &readfds))
				continue;
			addrs[i].uses++;
			dstlen = sizeof(struct sockaddr_in);
			if ((cc = 
			     recvfrom(addrs[i].fd, (char *) pkt, 
				      sizeof(ntpframe), 0, 
				      (struct sockaddr *) dst, &dstlen)) < 0) {

				if (errno != EWOULDBLOCK) {
					syslog(LOG_ERR, "recvfrom: %m");
#ifdef	DEBUG
					if(debug > 2)
						perror("recvfrom");
#endif
				}
				continue;
			}

			/* hpux seems to generate these zero's randomly */
			if (cc == 0) continue;

			if (cc < sizeof(*pkt)) {
#ifdef	DEBUG
				if (debug)
					printf("Runt packet from %s, got %d bytes, should be %d\n",
					       ntoa(dst->sin_addr),
					       cc, sizeof(*pkt));
#endif
				continue;
			} else if (cc > sizeof(*pkt)) {
#ifdef	DEBUG
				if (debug)
					printf("too many chars returned by recvfrom.  Host %s, got %d bytes, should be %d\n",
					       ntoa(dst->sin_addr),
					       cc, sizeof(*pkt));
#endif
				continue;
			}

			if (pkt->stratum == INFO_QUERY || 
			    pkt->stratum == INFO_REPLY) {
				query_mode(dst, pkt, i);
				continue;
			}
#ifdef	DEBUG
			if (debug > 3) {
				printf("\nInput ");
				dump_pkt(dst, pkt, NULL);
			}
#endif
			if ((pkt->status & VERSIONMASK) != NTPVERSION_1)
				continue;

			receive(dst, pkt, &tv, i);
		}
		if (ticked) {
			ticked = 0;
			timeout();
		}
	}			/* end of forever loop */
}

struct ntp_peer *
check_peer(dst, sock)
	struct sockaddr_in *dst;
	int sock;
{
	register struct ntp_peer *peer = peer_list.head;

	while (peer != NULL) {
		if ((peer->src.sin_addr.s_addr == dst->sin_addr.s_addr) &&
		    (peer->src.sin_port == dst->sin_port) &&
		    ((peer->sock == sock) || (peer->sock == -1)))
			return peer;
		peer = peer->next;
	}
	return ((struct ntp_peer *) NULL);
}

#ifdef	DEBUG
dump_pkt(dst, pkt, peer)
	struct sockaddr_in *dst;
	struct ntpdata *pkt;
	struct ntp_peer *peer;
{
	struct in_addr clock_host;

	printf("Packet: [%s](%d)\n", inet_ntoa(dst->sin_addr),
	       (int) htons(dst->sin_port));
	printf("Leap %d, version %d, mode %d, poll %d, precision %d stratum %d",
	       (pkt->status & LEAPMASK) >> 6, (pkt->status & VERSIONMASK) >> 3,
	       pkt->status & MODEMASK, pkt->ppoll, pkt->precision,
	       pkt->stratum);
	switch (pkt->stratum) {
	case 0:
	case 1:
		printf(" (%.4s)\n", (char *)&pkt->refid);
		break;
	default:
		clock_host.s_addr = (afs_uint32) pkt->refid;
		printf(" [%s]\n", inet_ntoa(clock_host));
		break;
	}
	printf("Synch Dist is %04X.%04X  Synch Dispersion is %04X.%04X\n",
	       ntohs((u_short) pkt->distance.int_part),
	       ntohs((u_short) pkt->distance.fraction),
	       ntohs((u_short) pkt->dispersion.int_part),
	       ntohs((u_short) pkt->dispersion.fraction));
	printf("Reference Timestamp is %08lx.%08lx %f\n",
	       ntohl(pkt->reftime.int_part),
	       ntohl(pkt->reftime.fraction),
	       ul_fixed_to_double(&pkt->reftime));
	printf("Originate Timestamp is %08lx.%08lx %f\n",
	       ntohl(pkt->org.int_part),
	       ntohl(pkt->org.fraction),
	       ul_fixed_to_double(&pkt->org));
	printf("Receive Timestamp is   %08lx.%08lx %f\n",
	       ntohl(pkt->rec.int_part),
	       ntohl(pkt->rec.fraction),
	       ul_fixed_to_double(&pkt->rec));
	printf("Transmit Timestamp is  %08lx.%08lx %f\n",
	       ntohl(pkt->xmt.int_part),
	       ntohl(pkt->xmt.fraction),
	       ul_fixed_to_double(&pkt->xmt));
	if(peer != NULL)
		printf("Input Timestamp is     %08lx.%08lx %f\n",
		       ntohl(peer->rec.int_part),
		       ntohl(peer->rec.fraction),
		       ul_fixed_to_double(&peer->rec));
	putchar('\n');
}
#endif

void
make_new_peer(peer)
	struct ntp_peer *peer;
{
	int i;

	/*
	 * initialize peer data fields 
	 */
	peer->src.sin_family = AF_INET;
	peer->src.sin_port = 0;
	peer->src.sin_addr.s_addr = 0;
	peer->hmode = MODE_SYM_PAS;	/* default: symmetric passive mode */
	peer->flags = 0;
	peer->timer = 1 << NTP_MINPOLL;
	peer->stopwatch = 0;
	peer->hpoll = NTP_MINPOLL;
	double_to_s_fixed(&peer->dispersion, PEER_MAXDISP);
	peer->reach = 0;
	peer->estoffset = 0.0;
	peer->estdelay = 0.0;
	peer->org.int_part = peer->org.fraction = 0;
	peer->rec.int_part = peer->rec.fraction = 0;
	peer->filter.samples = 0;
	for (i = 0; i < NTP_WINDOW; i++) {
		peer->filter.offset[i] = 0.0;
		peer->filter.delay[i] = 0.0;
	}
	peer->pkt_sent = 0;
	peer->pkt_rcvd = 0;
	peer->pkt_dropped = 0;
}

/*
 *  This procedure is called to delete a peer from our list of peers.
 */
void
demobilize(l, peer)
	struct list *l;
	struct ntp_peer *peer;
{
	extern struct ntp_peer dummy_peer;

	if (peer == &dummy_peer)
#ifdef	DEBUG
		abort();
#else
		return;
#endif

#ifdef	DEBUG
	if ((peer->next == NULL && peer->prev == NULL) ||
	    l->tail == NULL || l->head == NULL)
		abort();
#endif

	/* delete only peer in list? */
	if (l->head == l->tail) {
#ifdef	DEBUG
		if (l->head != peer) abort();
#endif
		l->head = l->tail = NULL;
		goto dropit;
	}

	/* delete first peer? */
	if (l->head == peer) {
		l->head = peer->next;
		l->head->prev = NULL;
		goto dropit;
	}

	/* delete last peer? */
	if (l->tail == peer) {
		l->tail = peer->prev;
		l->tail->next = NULL;
		goto dropit;
	}

	/* drop peer in middle */
	peer->prev->next = peer->next;
	peer->next->prev = peer->prev;

 dropit:
#ifdef	DEBUG
	/* just some sanity checking */
	if ((l->members <= 0) || 
	    (l->members && l->tail == NULL) ||
	    (l->members == 0 && l->tail != NULL)) {
		syslog(LOG_ERR, "List hosed (demobilize)");
		abort();
	}
	peer->next = peer->prev = NULL;
#endif
	free((char *) peer);
	l->members--;

	return;
}

enqueue(l, peer)
	register struct list *l;
	struct ntp_peer *peer;
{
	l->members++;
	if (l->tail == NULL) {
		/* insertion into empty list */
		l->tail = l->head = peer;
		peer->next = peer->prev = NULL;
		return;
	}

	/* insert at end of list */
	l->tail->next = peer;
	peer->next = NULL;
	peer->prev = l->tail;
	l->tail = peer;
}

/* XXX */
/*
 *  Trivial signal handler.
 */
void 
tock(x) {
	ticked = 1;

	/* re-enable self (we're not BSD any more) */
	(void) signal(SIGALRM, tock);
}

void
timeout()
{
	static int periodic = 0;
	register struct ntp_peer *peer = peer_list.head, *next;
#ifndef	XADJTIME2
	extern void adj_host_clock();

	adj_host_clock();
#endif
	/*
	 * Count down sys.hold if necessary.
	 */
	if (sys.hold) {
		if (sys.hold <= (1<<CLOCK_ADJ))
			sys.hold = 0;
		else
			sys.hold -= (1<<CLOCK_ADJ);
	}
	/*
	 * If interval has expired blast off an NTP to that host.
	 */
	while (peer != NULL) {
#ifdef	DEBUG
		if (peer->next == NULL && peer != peer_list.tail) {
			printf("Broken peer list\n");
			syslog(LOG_ERR, "Broken peer list");
			abort();
		}
#endif
		next = peer->next;
		if (peer->reach != 0 || peer->hmode != MODE_SERVER) {
			peer->stopwatch +=(1<<CLOCK_ADJ);
			if (peer->timer <= peer->stopwatch) {
				transmit(peer);
				peer->stopwatch = 0;
			}
		}
		peer = next;
	}

	periodic += (1<<CLOCK_ADJ);
	if (periodic >= 60*60) {
		periodic = 0;
		hourly();
	}

	clock_watchdog += (1 << CLOCK_ADJ);
	if (clock_watchdog >= NTP_MAXAGE) {
		/* woof, woof - barking dogs bite! */
		sys.leap = ALARM;
		if (clock_watchdog < NTP_MAXAGE + (1 << CLOCK_ADJ)) {
			syslog(LOG_ERR,
			       "logical clock adjust timeout (%d seconds)",
			       NTP_MAXAGE);
#ifdef	DEBUG
			if (debug)
			 printf("logical clock adjust timeout (%d seconds)\n",
				NTP_MAXAGE);
#endif
		}
	}

#ifdef	DEBUG
	if (debug)
		(void) fflush(stdout);
#endif
}


/*
 * init_ntp() reads NTP daemon configuration information from disk file.
 */
void
init_ntp(config)
	char *config;
{
	struct sockaddr_in sin;
	char ref_clock[5];
	char name[81];
	FILE *fp;
	int error = FALSE, c;
	struct ntp_peer *peer;
	int precision;
	int stratum;
	int i;
	int debuglevel;
	int stagger = 0;
	double j;
	extern double drift_comp;

	bzero((char *) &sin, sizeof(sin));
	fp = fopen(config, "r");
	if (fp == NULL) {
		fprintf(stderr,"Problem opening NTP initialization file %s\n",
			config);
		syslog(LOG_ERR,"Problem opening NTP initialization file %s",
			config);
		exit(1);
	}

	while (fscanf(fp, "%s", name) != EOF) {	/* read first word of line
						 * and compare to key words */
		if (strcmp(name, "maxpeers") == 0) {
			if (fscanf(fp, "%d", &sys.maxpeers) != 1)
				error = TRUE;
		} else if (strcmp(name, "trusting") == 0) {
			if (fscanf(fp, "%s", name) != 1)
				error = TRUE;
			else {
				if (*name == 'Y' || *name == 'y') {
					trusting = 1;
				} else if (*name == 'N' || *name == 'n') {
					trusting = 0;
				} else
					trusting = atoi(name);
			}
		} else if (strcmp(name, "logclock") == 0) {
			if (fscanf(fp, "%s", name) != 1)
				error = TRUE;
			else {
				if (*name == 'Y' || *name == 'y') {
					logstats = 1;
				} else if (*name == 'N' || *name == 'n') {
					logstats = 0;
				} else
					logstats = atoi(name);
			}
		} else if (strcmp(name, "driftfile") == 0) {
			if (fscanf(fp, "%s", name) != 1)
				error = TRUE;
			else {
				if (driftcomp_file = malloc(strlen(name)+1))
					strcpy(driftcomp_file, name);
			}
		} else if (strcmp(name, "waytoobig") == 0 ||
			   strcmp(name, "setthreshold") == 0) {
			if (fscanf(fp, "%s", name) != 1)
				error = TRUE;
			else {
				if (strcmp(name, "any") == 0)
					WayTooBig = 10e15;
				else
					WayTooBig = atof(name);
			}
		} else if (strncmp(name, "debuglevel", 5) == 0) {
			if (fscanf(fp, "%d", &debuglevel) != 1)
				error = TRUE;
#ifdef	DEBUG
			else debug += debuglevel;
#endif
		} else if (strcmp(name, "stratum") == 0) {
			fprintf(stderr, "Obsolete command 'stratum'\n");
			error = TRUE;
		} else if (strcmp(name, "precision") == 0) {
			if (fscanf(fp, "%d", &precision) != 1)
				error = TRUE;
			else sys.precision = (char) precision;
#ifdef	SETTICKADJ
		} else if (strcmp(name, "tickadj") == 0) {
			if (fscanf(fp, "%d", &i) != 1)
				error = TRUE;
			else tickadj = i;
		} else if (strcmp(name, "settickadj") == 0) {
			if (fscanf(fp, "%s", name) != 1)
				error = TRUE;
			else {
				if (*name == 'Y' || *name == 'y') {
					dotickadj = 1;
				} else if (*name == 'N' || *name == 'n') {
					dotickadj = 0;
				} else
					dotickadj = atoi(name);
			}
		} else if (strcmp(name, "setdosynctodr") == 0) {
			if (fscanf(fp, "%s", name) != 1)
				error = TRUE;
			else {
				if (*name == 'Y' || *name == 'y') {
					dosynctodr = 1;
				} else if (*name == 'N' || *name == 'n') {
					dosynctodr = 0;
				} else
					dosynctodr = atoi(name);
			}
#endif
#ifdef	NOSWAP
		} else if (strcmp(name, "noswap") == 0) {
			noswap = 1;
#endif
#ifdef	BROADCAST_NTP
		} else if (strcmp(name, "broadcast") == 0) {
			if (fscanf(fp, "%s", name) != 1) {
				error = TRUE;
				goto skipit;
			}
			for (i = 0; i < nintf; i++)
				if (strcmp(addrs[i].name, name) == 0)
					break;
			if (i == nintf) {
				syslog(LOG_ERR, "config file: %s not a known interface");
				error = TRUE;
				goto skipit;
			}
			if ((addrs[i].if_flags & IFF_BROADCAST) == 0) {
				syslog(LOG_ERR, "config file: %s doesn't support broadcast", name);
				error = TRUE;
				goto skipit;
			}
			if (peer = check_peer(&addrs[i].bcast, -1)) {
				syslog(LOG_ERR, "config file: duplicate broadcast for %s",
				       name);
				error = TRUE;
				goto skipit;
			}
			peer = (struct ntp_peer *) malloc(sizeof(struct ntp_peer));
			if (peer == NULL) {
				error = TRUE;
				syslog(LOG_ERR, "No memory");
				goto skipit;
			}
			make_new_peer(peer);
			peer->flags = PEER_FL_BCAST;
			peer->hmode = MODE_BROADCAST;
			peer->src = addrs[i].bcast;
			peer->sock = i;
#endif	/* BROADCAST_NTP */
		} else if ((strcmp(name, "peer") == 0) || 
			   (strcmp(name, "passive") == 0) ||
			   (strcmp(name, "server") == 0)) {
			int mode = 0;

			if (strcmp(name, "peer") == 0) {
				mode = MODE_SYM_ACT;
			} else if (strcmp(name, "server") == 0) {
				mode = MODE_CLIENT;
			} else if (strcmp(name, "passive") == 0) {
				mode = MODE_SYM_PAS;
			} else {
				printf("can't happen\n");
				abort();
			}
			if (fscanf(fp, "%s", name) != 1)
				error = TRUE;
#ifdef REFCLOCK
			else if (name[0] == '/') {
				int stratum, precision;
				char clk_type[20];

				if (fscanf(fp, "%4s", ref_clock) != 1) {
					error = TRUE;
					syslog(LOG_ERR, "reference id missing");
					goto skipit;
				}
				if (fscanf(fp, "%4d", &stratum) != 1) {
					error = TRUE;
					syslog(LOG_ERR, "reference stratum missing");
					goto skipit;
				}
				if (fscanf(fp, "%4d", &precision) != 1) {
					error = TRUE;
					syslog(LOG_ERR, "reference precision missing");
					goto skipit;
				}
				if (fscanf(fp, "%19s", clk_type) != 1) {
					error = TRUE;
					syslog(LOG_ERR, "reference type missing");
					goto skipit;
				}

				if((i = init_clock(name, clk_type)) < 0) {
					/* If we could not initialize clock line */
#ifdef DEBUG
					if (debug)
						printf("Could not init reference source %s (type %s)\n",
							name, clk_type);
					else
#endif /* DEBUG */
						syslog(LOG_ERR, "Could not init reference source %s (type %s)",
							name, clk_type);
					error = TRUE;
					goto skipit;
				}
				peer = (struct ntp_peer *)
					malloc(sizeof(struct ntp_peer));
				if (peer == NULL) {
					close(i);
					error = TRUE;
					goto skipit;
				}
				make_new_peer(peer);
				ref_clock[4] = 0;
				(void) strncpy((char *) &peer->refid,
						ref_clock, 4);
				peer->flags = PEER_FL_CONFIG|PEER_FL_REFCLOCK;
				peer->hmode = MODE_SYM_ACT;
				peer->stopwatch = stagger;
				stagger += (1<<CLOCK_ADJ);
				peer->flags |= PEER_FL_SYNC;
				peer->sock = i;
				peer->stratum = stratum;
				peer->precision = precision;
				clear(peer);
				enqueue(&peer_list, peer);
#ifdef DEBUG
				if (debug > 1)
					printf("Peer %s mode %d refid %.4s stratum %d precision %d\n",
					       name,
					       peer->hmode,
					       (char *)&peer->refid,
					       stratum, precision);
#endif
				transmit(peer);	/* head start for REFCLOCK */
			}
#endif /* REFCLOCK */
			else if (GetHostName(name, &sin) == 0)
				syslog(LOG_ERR, "%s: unknown host", name);
			else {
				for (i=0; i<nintf; i++)
					if (addrs[i].sin.sin_addr.s_addr ==
					    sin.sin_addr.s_addr)
						goto skipit;

				if (servp)
					sin.sin_port = servp->s_port;
				else
					sin.sin_port = htons(NTP_PORT);

				peer = check_peer(&sin, -1);
				if (peer == NULL) {
					peer = (struct ntp_peer *)
						malloc(sizeof(struct ntp_peer));
					if (peer == NULL)
						error = TRUE;
					else {
						make_new_peer(peer);
						peer->flags = PEER_FL_CONFIG;
						switch (mode) {
						case MODE_SYM_ACT:	/* "peer" */
							peer->hmode = MODE_SYM_ACT;
							peer->stopwatch = stagger;
							stagger += (1<<CLOCK_ADJ);
							peer->flags |= PEER_FL_SYNC;
							break;
						case MODE_CLIENT:	/* "server" */
							peer->hmode = MODE_CLIENT;
							peer->stopwatch = stagger;
							stagger += (1<<CLOCK_ADJ);
							peer->flags |= PEER_FL_SYNC;
							break;
						case MODE_SYM_PAS:	/* "passive" */
							peer->hmode = MODE_SYM_PAS;
							peer->flags |= PEER_FL_SYNC;
							break;
						default:
							printf("can't happen\n");
							abort();
						}
						peer->src = sin;
						peer->sock = -1;
						clear(peer);
						enqueue(&peer_list, peer);
#ifdef	DEBUG
						if (debug > 1)
							printf("Peer %s/%d af %d mode %d\n",
							       inet_ntoa(peer->src.sin_addr),
							       ntohs(peer->src.sin_port),
							       peer->src.sin_family,
							       peer->hmode);
#endif
					}
				} else {
					syslog(LOG_WARNING,
				     "Duplicate peer %s in in config file",
					       inet_ntoa(sin.sin_addr));
#ifdef	DEBUG
					if(debug)
						printf("Duplicate peer %s in in config file\n",
						       inet_ntoa(sin.sin_addr));
#endif
				}
			}
			skipit:;
		} else if( *name != '#' ) {
			syslog(LOG_ERR, "config file: %s not recognized", name);
#ifdef DEBUG
			if(debug)
				printf("unrecognized option in config file: %s\n", name);
#endif
		}
		do
			c = fgetc(fp);
		while (c != '\n' && c != EOF);	/* next line */
	}			/* end while */
	if (error) {
		fprintf(stderr, "init_ntp: %s: initialization error\n", config);
		syslog(LOG_ERR, "init_ntp: %s: initialization error", config);

		exit(1);
	}
	/*
	 *  Read saved drift compensation register value.
	 */
#if defined(AFS_SUN_ENV) || defined(AFS_HPUX_ENV)
#define BOGUS_DRIFT_COMPENSATION 10.0
#else
#define BOGUS_DRIFT_COMPENSATION 1.0
#endif
	if ((fp = fopen(driftcomp_file, "r")) != NULL) {
		if (fscanf(fp, "%lf", &j) == 1 &&
		    j > -BOGUS_DRIFT_COMPENSATION &&
		    j < BOGUS_DRIFT_COMPENSATION) {
			drift_comp = j;
			syslog(LOG_INFO,
			       "Drift compensation value initialized to %f", j);
		} else {
			fprintf(stderr,
				"init_ntp: bad drift compensation value\n");
			syslog(LOG_ERR,
			       "init_ntp: bad drift compensation value\n");
		}
		fclose(fp);
	}
}

int kern_tickadj, kern_hz, kern_tick, kern_dosynctodr;

#ifdef	SETTICKADJ
int SetKernel (kmem, nl, newValue)
  int kmem;
  struct nlist *nl;
{
    static char	*memory = "/dev/kmem";
    if (lseek(kmem, (long)nl->n_value, L_SET) == -1) {
	syslog(LOG_ERR, "%s: lseek fails: %m", memory);
	perror ("setting kernel variable");
	return errno;
    }
    if (write(kmem, &newValue, sizeof(int)) != sizeof(int)) {
	syslog(LOG_ERR, "%s: kernel variable assignment fails: %m", memory);
	printf("SetKernel variable %s fails\n", nl->n_name);
	perror ("setting kernel variable");
	return errno;
    } 
    syslog(LOG_INFO, "System %s SET to %d", nl->n_name, newValue);
    printf("System %s SET to %d\n", nl->n_name, newValue);
}
#endif

#if defined(INIT_KERN_VARS)
void
init_kern_vars()
{
	int kmem;
	static char	*memory = "/dev/kmem";
	static struct nlist nl[] = {
#ifndef	NeXT
		{"_tickadj"},
		{"_hz"},
		{"_tick"},
#ifdef AFS_SUN_ENV
		{"_dosynctodr"},
#endif
		{""},
#else
		{{"_tickadj"}},
		{{"_hz"}},
		{{"_tick"}},
		{{""}},
#endif
	};
	static int *kern_vars[] =
	    {&kern_tickadj, &kern_hz, &kern_tick, &kern_dosynctodr};
	int i;
	kmem = open(memory, O_RDONLY);
	if (kmem < 0) {
		syslog(LOG_ERR, "Can't open %s for reading: %m", memory);
#ifdef	DEBUG
		if (debug)
			perror(memory);
#endif
		return;
	}

	nlist("/vmunix", nl);

	for (i = 0; i < (sizeof(kern_vars)/sizeof(kern_vars[0])); i++) {
		long where;

		if ((where = nl[i].n_value) == 0) {
			syslog(LOG_ERR, "Unknown kernal var %s",
#ifdef	NeXT
			       nl[i].n_un.n_name
#else
			       nl[i].n_name
#endif
			       );
			continue;
		}
		if (lseek(kmem, where, L_SET) == -1) {
			syslog(LOG_ERR, "lseek for %s fails: %m",
#ifdef	NeXT
			       nl[i].n_un.n_name
#else
			       nl[i].n_name
#endif
			       );
			continue;
		}
		if (read(kmem, kern_vars[i], sizeof(int)) != sizeof(int)) {
			syslog(LOG_ERR, "read for %s fails: %m",
#ifdef	NeXT
			       nl[i].n_un.n_name
#else
			       nl[i].n_name
#endif
			       );

			*kern_vars[i] = 0;
		}
	}

	/*
	 *  If desired value of tickadj is not specified in the configuration
	 *  file, compute a "reasonable" value here, based on the assumption 
	 *  that we don't have to slew more than 2ms every 4 seconds.
	 *
	 *  TODO: the 500 needs to be parameterized.
	 */
	if (tickadj == 0 && kern_hz)
		tickadj = 500/kern_hz;

#ifdef	DEBUG
	if (debug) {
		printf("kernel vars: tickadj = %d, hz = %d, tick = %d\n",
		       kern_tickadj, kern_hz, kern_tick);
#ifdef AFS_SUN_ENV
		if (kern_dosynctodr) printf ("dosynctodr is on\n");
#endif
		printf("desired tickadj = %d, dotickadj = %d\n", tickadj,
		       dotickadj);
	}
#endif
	close(kmem);

#ifdef	SETTICKADJ
	{   int set_tickadj, set_dosynctodr;
	    int code;
	    set_tickadj = (dotickadj && tickadj && (tickadj != kern_tickadj)
			   /* do some plausibility checks on the value... */
			   && (kern_tickadj > 0) && (kern_tickadj < 20000));
	    set_dosynctodr = (dosynctodr && (kern_dosynctodr == 1));
	    if (set_tickadj || set_dosynctodr) {
		if ((kmem = open(memory, O_RDWR)) >= 0) {
		    if (set_tickadj) {
			code = SetKernel (kmem, &nl[0], tickadj);
			if (code == 0) kern_tickadj = tickadj;
		    }
		    if (set_dosynctodr) {
			code = SetKernel (kmem, &nl[3], 0);
			if (code == 0) kern_dosynctodr = 0;
		    }
		    close (kmem);
		} else {
		    syslog(LOG_ERR, "Can't open %s: %m", memory);
		    printf("Can't open %s\n", memory);
		}
	    }
	}
#endif	/* SETTICKADJ */

	/*
	 *  If we have successfully discovered `hz' from the kernel, then we
	 *  can set sys.precision, if it has not already been specified.  If
	 *  no value of `hz' is available, then use default (-6)
	 */
	if (sys.precision == 0) {
	    char msg[128];
	    int interval = 0;
	    sys.precision = MeasurePrecision(&interval);
	    if (sys.precision) {
		sprintf (msg, "sys.precision set to %d from measured minimum clock interval of %d usec",
			 sys.precision, interval);
	    } else {
		if (kern_hz <= 64)
		    sys.precision = -6;
		else if (kern_hz <= 128)
		    sys.precision = -7;
		else if (kern_hz <= 256)
		    sys.precision = -8;
		else if (kern_hz <= 512)
		    sys.precision = -9;
		else if (kern_hz <= 1024)
		    sys.precision = -10;
		else sys.precision = -11;
		sprintf (msg,
			 "sys.precision set to %d from sys clock of %d HZ",
			 sys.precision, kern_hz);
	    }
	    syslog(LOG_INFO, msg);
	    printf ("%s\n", msg);
	}
}
#endif	/* INIT_KERN_VARS */


/*
 * Given host or net name or internet address in dot notation assign the
 * internet address in byte format. source is ../routed/startup.c with minor
 * changes to detect syntax errors. 
 *
 * We now try to interpret the name as in address before we go off and bother
 * the domain name servers.
 *
 * Unfortunately the library routine inet_addr() does not detect mal formed
 * addresses that have characters or byte values > 255. 
 */

GetHostName(name, sin)
	char *name;
	struct sockaddr_in *sin;
{
	afs_int32 HostAddr;
	struct hostent *hp;

	if ((HostAddr = inet_addr(name)) != -1) {
		sin->sin_addr.s_addr = (afs_uint32) HostAddr;
		sin->sin_family = AF_INET;
		return (1);
	}

	if (hp = gethostbyname(name)) {
		if (hp->h_addrtype != AF_INET)
			return (0);
		bcopy((char *) hp->h_addr, (char *) &sin->sin_addr,
		      hp->h_length);
		sin->sin_family = hp->h_addrtype;
		return (1);
	}
	return (0);
}

#define	PKTBUF_SIZE	536

/* number of clocks per packet */
#define	N_NTP_PKTS \
      ((PKTBUF_SIZE - sizeof(struct ntpinfo))/(sizeof(struct clockinfo)))

query_mode(dst, ntp, sock)
	struct sockaddr_in *dst;
	struct ntpdata *ntp;
	int sock;		/* which socket packet arrived on */
{
	char packet[PKTBUF_SIZE];
	register struct ntpinfo *nip = (struct ntpinfo *) packet;
	register struct ntp_peer *peer = peer_list.head;
	struct clockinfo *cip;
	int seq = 0;
	int i;

	if (ntp->stratum != INFO_QUERY)
		return;
	nip->version = NTPDC_VERSION;
	nip->type = INFO_REPLY;
	nip->seq = 0;
	nip->npkts = peer_list.members/N_NTP_PKTS;
	if (peer_list.members % N_NTP_PKTS)
		nip->npkts++;
	nip->peers = peer_list.members;
	nip->count = 0;
	cip = (struct clockinfo *)&nip[1];

	while (peer != NULL) {
		cip->net_address = peer->src.sin_addr.s_addr;
		if ((peer->sock < 0) || (peer->sock >= nintf))
			cip->my_address = htonl(0);
		else
			cip->my_address = addrs[peer->sock].sin.sin_addr.s_addr;
		cip->port = peer->src.sin_port;	/* already in network order */
		cip->flags = htons(peer->flags);
		if (sys.peer == peer)
			cip->flags |= htons(PEER_FL_SELECTED);
		cip->pkt_sent = htonl(peer->pkt_sent);
		cip->pkt_rcvd = htonl(peer->pkt_rcvd);
		cip->pkt_dropped = htonl(peer->pkt_dropped);
		cip->timer = htonl(peer->timer);
		cip->leap = peer->leap;
		cip->stratum = peer->stratum;
		cip->ppoll = peer->ppoll;
		cip->precision = peer->precision;
		cip->hpoll = peer->hpoll;
		cip->reach = htons(peer->reach & NTP_WINDOW_SHIFT_MASK);
		cip->estdisp = htonl((afs_int32) (peer->estdisp * 1000.0));
		cip->estdelay = htonl((afs_int32) (peer->estdelay * 1000.0));
		cip->estoffset = htonl((afs_int32) (peer->estoffset * 1000.0));
		cip->refid = peer->refid;
		cip->reftime.int_part = htonl(peer->reftime.int_part);
		cip->reftime.fraction = htonl(peer->reftime.fraction);

		cip->info_filter.index = htons(peer->filter.samples);
		for (i = 0; i < PEER_SHIFT; i++) {
			cip->info_filter.offset[i] =
				htonl((afs_int32)(peer->filter.offset[i] * 1000.0));
			cip->info_filter.delay[i] =
				htonl((afs_int32)(peer->filter.delay[i] * 1000.0));
		}
		cip++;
		if (nip->count++ >= N_NTP_PKTS - 1) {
			nip->seq =seq++;
			if ((sendto(addrs[sock].fd, (char *) packet, 
				    sizeof(packet), 0,
				    (struct sockaddr *) dst,
				    sizeof(struct sockaddr_in))) < 0) {
				syslog(LOG_ERR, "sendto: %s  %m",
				       inet_ntoa(dst->sin_addr));
			}
			nip->type = INFO_REPLY;
			nip->count = 0;
			cip = (struct clockinfo *)&nip[1];
		}
		peer = peer->next;
	}
	if (nip->count) {
		nip->seq = seq;
		if ((sendto(addrs[sock].fd, (char *) packet, sizeof(packet), 0,
			    (struct sockaddr *) dst,
			    sizeof(struct sockaddr_in))) < 0) {
			syslog(LOG_ERR, "sendto: %s  %m", ntoa(dst->sin_addr));
		}
	}
}

/* every hour, dump some useful information to the log */
void
hourly() {
	char buf[200];
	register int p = 0;
	static double drifts[5] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
	static int drift_count = 0;
	extern double drift_comp, compliance;
	extern int peer_switches, peer_sw_inhibited;

	(void) sprintf(buf, "stats: dc %f comp %f peersw %d inh %d",
		       drift_comp, compliance, peer_switches,
		       peer_sw_inhibited);

	if (sys.peer == NULL) {
		strcat(buf, " UNSYNC");
#ifdef	REFCLOCK
	} else if (sys.peer->flags & PEER_FL_REFCLOCK) {
		p = strlen(buf);
		(void) sprintf(buf + p, " off %f SYNC %.4s %d",
			       sys.peer->estoffset,
			       (char *)&sys.peer->refid,
			       sys.peer->stratum);
#endif
	} else {
		p = strlen(buf);
		(void) sprintf(buf + p, " off %f SYNC %s %d",
			       sys.peer->estoffset,
			       ntoa(sys.peer->src.sin_addr),
			       sys.peer->stratum);
	}
	syslog(LOG_INFO, buf);
#ifdef	DEBUG
	if (debug)
		puts(buf);
#endif
	/*
	 *  If the drift compensation snapshot file is open, then write
	 *  the current value to it.  Since there's only one block in the
	 *  file, and no one else is reading it, we'll just keep the file
	 *  open and write to it.
	 */
	if (drift_fd >= 0) {
		drifts[drift_count % 5] = drift_comp;
		/* works out to be 70 bytes */
		(void) sprintf(buf,
		     "%+12.10f %+12.10f %+12.10f %+12.10f %+12.10f %4d\n",
			       drifts[drift_count % 5],
			       drifts[(drift_count+4) % 5],
			       drifts[(drift_count+3) % 5],
			       drifts[(drift_count+2) % 5],
			       drifts[(drift_count+1) % 5],
			       drift_count + 1);

		(void) lseek(drift_fd, 0L, L_SET);
		if (write(drift_fd, buf, strlen(buf)) < 0) {
			syslog(LOG_ERR, "Error writing drift comp file: %m");
		}
		drift_count++;
	}
}

#if	defined(DEBUG) && defined(SIGUSR1) && defined(SIGUSR2)
void
incdebug(x)
{
	/* re-enable self (we're not BSD any more) */
	(void) signal(SIGUSR1, incdebug);

	if (debug == 255)
		return;
	debug++;
	printf("DEBUG LEVEL %d\n", debug);
#ifdef	LOG_DAEMON
	(void) setlogmask(LOG_UPTO(LOG_DEBUG));
#endif
	syslog(LOG_DEBUG, "DEBUG LEVEL %d", debug);
}

void
decdebug(x)
{
	/* re-enable self (we're not BSD any more) */
	(void) signal(SIGUSR2, decdebug);

	if (debug == 0)
		return;
	debug--;
	printf("DEBUG LEVEL %d\n", debug);
	syslog(LOG_DEBUG, "DEBUG LEVEL %d", debug);
#ifdef	LOG_DAEMON
	if (debug == 0)
		(void) setlogmask(LOG_UPTO(LOG_INFO));
#endif
}
#endif

void
finish(sig)
	int sig;
{
	syslog(LOG_NOTICE, "terminated: (sig %d)", sig);
#ifdef	DEBUG
	if (debug)
		printf("ntpd terminated (sig %d)\n", sig);
#endif
	exit(1);
}

#ifdef	REFCLOCK
struct refclock {
	int fd;
	int (*reader)();
	struct refclock *next;
} *refclocks = NULL;

int init_clock_local(), read_clock_local();
#ifdef PSTI
int init_clock_psti(), read_clock_psti();
#endif /* PSTI */

init_clock(name, type)
char *name, *type;
{
	struct refclock *r;
	int (*reader)();
	int cfd;

	if (strcmp(type, "local") == 0) {
		reader = read_clock_local;
		cfd = init_clock_local(name);
	}
#ifdef PSTI
	else if (strcmp(type, "psti") == 0) {
		reader = read_clock_psti;
		cfd = init_clock_psti(name);
	}
#endif /* PSTI */
	else {
#ifdef DEBUG
		if (debug) printf("Unknown reference type\n"); else
#endif
		syslog(LOG_ERR, "Unknown reference clock type (%s)\n", type);
		return(-1);
	}
	if (cfd >= 0) {
		r = (struct refclock *)malloc(sizeof(struct refclock));
		r->fd = cfd;
		r->reader = reader;
		r->next = refclocks;
		refclocks = r;
	}
	return(cfd);
}

read_clock(cfd, tvpp, otvpp)
int cfd;
struct timeval **tvpp, **otvpp;
{
	struct refclock *r;

	for (r = refclocks; r; r = r->next)
		if(r->fd == cfd)
			return((r->reader)(cfd, tvpp, otvpp));
	return(1); /* Can't happen */
}
#endif
