#ifndef	lint
#endif

/*
 * This module actually implements the the bulk of the NTP protocol processing.
 * It contains a minimum of machine and operating system dependencies (or at
 * least that's the idea).  Setup of UDP sockets, timers, etc is done in the
 * ntpd.c module, while arithmetic conversion routines are in ntpsubs.c
 */

/*
 * Revision 2.2  90/09/20  09:21:00
 * Print Input timestamp so that it follow ump_pkt outupt.
 * Add various checks for bogus timestamps and delays.
 * Parenthesize ?: expression properly.
 * 
 * Revision 2.1  90/08/07  19:22:57
 * Start with clean version to sync test and dev trees.
 * 
 * Revision 1.4  90/05/24  16:18:43
 * Cause logstats to be printed out if debug mode is on.
 * 
 * Revision 1.3  89/12/22  20:31:43
 * hp/ux specific (initial port to it); Added <afs/param.h> and special include files for HPUX and misc other changes
 * 
 * Revision 1.2  89/12/11  14:25:31
 * Added code to support AIX 2.2.1.
 * 
 * Revision 1.1  89/05/24  12:02:06
 * Initial revision
 * 
 * Revision 3.4.1.12  89/05/18  18:25:04
 * Changes for reference clock feature in ntp_proto.c
 * 
 * Revision 3.4.1.11  89/05/03  23:51:30
 * Had my head on backwards with a reversed test in the clockhopper avoidance
 * code.  Need to switch to the first selected clock when its stratum is lower
 * than the current sys.peer.
 * 
 * Revision 3.4.1.10  89/05/03  19:03:02
 * Stupid typo - dereferenced unused variable in select_clock()
 * 
 * Revision 3.4.1.9  89/05/03  15:13:25
 * Add code to count number of peer switches and inhibited peer switches.  Clock
 * selection code has been updated to reflect 21 April 1989 draft of NTP spec.
 * 
 * Revision 3.4.1.8  89/04/10  15:57:59
 * New -l option for ntpd to enable logging for clock adjust messages.  Changed
 * our idea of a bogus packet in the packet procedure to include a packet received
 * before a poll is sent.  Fix stupid bug in delay computation having to do with
 * peer->precision.
 * 
 * Revision 3.4.1.7  89/04/08  10:36:53
 * The syslog message for peer selection had to be moved to account for the
 * anti-peer flapping code just installed.
 * 
 * Revision 3.4.1.6  89/04/07  19:07:10
 * Don't clear peer.reach register in the clear() procedure.  Code to prevent
 * flapping between two peers with very similar dispersions.
 * 
 * Revision 3.4.1.5  89/03/31  16:36:38
 * There is now a run-time option that can be specified in the configuration 
 * which specifies if we will synchronize to unconfigured hosts.  Fixes to
 * receive() logic state machine.
 * 
 * Revision 3.4.1.4  89/03/29  12:29:10
 * The variable 'mode' in the peer structure was renamed 'hmode'.  Add 
 * poll_update() calls in a few places per Mills.  The receive() procedure is
 * now table driven.  The poll_update procedure only randomized the timer
 * when the interval changes.  If we lose synchronization, don't zap sys.stratum.
 * Clean up the sanity_check() routine a bit.
 * 
 * Revision 3.4.1.3  89/03/22  18:32:31
 * patch3: Use new RCS headers.
 * 
 * Revision 3.4.1.2  89/03/22  18:02:22
 * Add some fiddles for BROADCAST NTP mode.  In the receive procedure, set the
 * reachability shift register of peers that are configured, even if we won't
 * synchronized to them.  Fix adjustment of delay in the process_packet()
 * routine.  Repair byteswapping problem.
 * 
 * 
 * Revision 3.4.1.1  89/03/20  00:10:06
 * patch1: sys.refid could have garbage left if the peer we're synchronized to
 * patch1: is lost.
 * 
 * Revision 3.4  89/03/17  18:37:05
 * Latest test release.
 * 
 * Revision 3.3.1.1  89/03/17  18:26:02
 * Oh my, peer->hpoll wasn't being set in peer_update!
 * 
 * Revision 3.3  89/03/15  14:19:49
 * New baseline for next release.
 * 
 * Revision 3.2.1.2  89/03/15  13:54:41
 * Change use of "%lf" in format strings to use "%f" instead.
 * poll_update no longer returns a value, due to a change in the transmit
 * procedure; it is now declared as returning void.  Removed syslog
 * message "Dropping peer ...".  You still get messages for peers which
 * were configured when reachability is lost with them.  Clarification of
 * calling poll_update on sys.peer rather than on the host whose packet
 * we're processing when sys.peer changes.  poll_update has been updated
 * including randomizing peer.timer.
 * 
 * Revision 3.2.1.1  89/03/10  11:30:33
 * Remove computation of peer->timer that was present due to a bug in the NTP
 * spec.  Don't set sys.precision in the NTP protocol initialization; this has
 * bad side effects with the code that get tick from the kernel and the NTP
 * config file scanner.
 * 
 * Revision 3.2  89/03/07  18:24:54
 * New version of UNIX NTP daemon based on the 6 March 1989 draft of the new
 * NTP protocol specification.  This version has a bunch of bugs fixes and
 * new algorithms which were discussed on the NTP mailing list over the past
 * few weeks.
 * 
 * Revision 3.1.1.1  89/02/15  08:57:34
 * *** empty log message ***
 * 
 * 
 * Revision 3.1  89/01/30  14:43:10
 * Second UNIX NTP test release.
 * 
 * Revision 3.0  88/12/12  15:59:35
 * Test release of new UNIX NTP software.  This version should conform to the
 * revised NTP protocol specification.
 * 
 */

#include <afs/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#ifdef	AFS_SUN5_ENV
#include <sys/sysmacros.h>
#endif
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/resource.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>
#include <errno.h>
#if defined(AIX)
#include <sys/syslog.h>
#else
#include <syslog.h>
#endif /* defined(AIX) */

#include "ntp.h"

int peer_switches, peer_sw_inhibited;

struct ntp_peer dummy_peer;
extern double WayTooBig;
extern afs_uint32 clock_watchdog;
#ifdef	DEBUG
extern int debug;
extern void dump_pkt();
#endif
extern int trusting, logstats;
extern struct sysdata sys;
extern struct list peer_list;
extern struct ntp_peer *check_peer();
extern struct servent *servp;
extern char *malloc(), *ntoa();
extern double drift_comp, compliance;	/* logical clock variables */
extern double s_fixed_to_double(), ul_fixed_to_double();
extern void make_new_peer(), double_to_s_fixed(), tstamp(), demobilize();
	

#ifdef	REFCLOCK
void	refclock_input();
#endif

void	process_packet(), clock_update(), clear(), clock_filter(),
	receive(), select_clock(), poll_update();

/* 3.4. Event Processing */

/* 3.4.1. Transmit Procedure */
void
transmit(peer)
	struct ntp_peer *peer;
{
	struct timeval txtv;
	static struct ntpdata ntpframe;
	struct ntpdata *pkt = &ntpframe;
	int i;

	pkt->status = sys.leap | NTPVERSION_1 | peer->hmode;
	pkt->stratum = sys.stratum;
	pkt->ppoll = peer->hpoll;
	pkt->precision = sys.precision;
	pkt->distance = sys.distance;
	pkt->dispersion = sys.dispersion;
	pkt->refid = sys.refid;
	pkt->reftime = sys.reftime;
	pkt->org = peer->org;
	pkt->rec = peer->rec;
	(void) gettimeofday(&txtv, (struct timezone *) 0);
#if	0
	if (peer->flags & PEER_FL_AUTHENABLE) {
		/* add encryption time into the timestamp */
		tstamp(&pkt->xmt, &txtv);
		/* call encrypt() procedure */
		pkt->keyid = ???;
		pkt->mac = ???;
	} else {
		pkt->mac[0] = pkt->mac[1] = 0;
		pkt->keyid = 0;			/* XXX */
		tstamp(&pkt->xmt, &txtv);
	}
#else
	tstamp(&pkt->xmt, &txtv);
#endif

	peer->xmt = pkt->xmt;

	if ((peer->flags & (PEER_FL_BCAST|PEER_FL_REFCLOCK)) == 0) {
		/* select correct socket to send reply on */
		if (sendto(addrs[(peer->sock < 0 ? 0 : peer->sock)].fd,
			   (char *) pkt, sizeof(ntpframe),
			   0, (struct sockaddr *) &peer->src,
			   sizeof(peer->src)) < 0) {
			syslog(LOG_ERR, "sendto: %s  %m",
			       ntoa(peer->src.sin_addr));
			return;
		}
#ifdef	REFCLOCK
	} else if (peer->flags & PEER_FL_REFCLOCK) {
		/* Special version of code below, adjusted for refclocks */


		peer->pkt_sent++;
		i = peer->reach;	/* save a copy */

		peer->reach = (peer->reach << 1) & NTP_WINDOW_SHIFT_MASK;

		if (i && peer->reach == 0) {
			syslog(LOG_INFO, "Lost reachability with %.4s",
			       (char *)&peer->refid);
#ifdef	DEBUG
			if (debug)
				printf("Lost reachability with %.4s\n",
				       (char *)&peer->refid);
#endif
		}

		if (peer->reach == 0)
			clear(peer);

		if (peer->valid < 2)
			peer->valid++;
		else {
			clock_filter(peer, 0.0, 0.0);	/* call with invalid values */
			select_clock();		/* and try to reselect clock */
		}

		peer->timer = 1<<NTP_MINPOLL;	/* poll refclocks frequently */

		refclock_input(peer, pkt);
		return;
#endif /* REFCLOCK */
	} else {
#ifdef	BROADCAST_NTP
		if (sendto(addrs[peer->sock].fd,
			   (char *) pkt, sizeof(ntpframe),
			   0, &peer->src, sizeof(peer->src)) < 0) {
			syslog(LOG_ERR, "bcast sendto: %s  %m",
			       ntoa(peer->src.sin_addr));
			return;
		}
#else
		return;
#endif
	}

#ifdef	DEBUG
	if (debug > 5) {
		printf("\nSent ");
		dump_pkt(&peer->src, pkt, (struct ntp_peer *)NULL);
	}
#endif
	peer->pkt_sent++;
	i = peer->reach;	/* save a copy */

	peer->reach = (peer->reach << 1) & NTP_WINDOW_SHIFT_MASK;

	if ((peer->reach == 0) && 
	    ((peer->flags & PEER_FL_CONFIG) == 0) &&
	    (peer != &dummy_peer)) {
		demobilize(&peer_list, peer);
		return;
	}

	if (i && peer->reach == 0) {
		syslog(LOG_INFO, "Lost reachability with %s",
			       ntoa(peer->src.sin_addr));
#ifdef	DEBUG
		if (debug)
			printf("Lost reachability with %s",
			       ntoa(peer->src.sin_addr));
#endif
	}

	if (peer->reach == 0) {
		clear(peer);
		peer->sock = -1;	/* since he fell off the end of the
					   earth, don't depend on local address
					   any longer */
	}

	if (peer->valid < 2)
		peer->valid++;
	else {
		clock_filter(peer, 0.0, 0.0);	/* call with invalid values */
		select_clock();		/* and try to reselect clock */
		if (sys.peer != NULL)
			poll_update(sys.peer, NTP_MINPOLL);
	}

	peer->timer = 1<<(MAX(MIN(peer->ppoll, MIN(peer->hpoll, NTP_MAXPOLL)),
			       NTP_MINPOLL));

	if (peer->estdisp > PEER_THRESHOLD)
		poll_update(peer, peer->hpoll - 1);
	else
		poll_update(peer, peer->hpoll + 1);
}

#ifdef REFCLOCK
void
refclock_input(peer, pkt)
	struct ntpdata *pkt;
	struct ntp_peer *peer;
{
	struct timeval *tvp;
	struct timeval *otvp;

	if (read_clock(peer->sock, &tvp, &otvp))
		return;

	tstamp(&pkt->rec, tvp);
	pkt->xmt = pkt->rec;
	pkt->reftime = pkt->rec;
	tstamp(&pkt->org, otvp);
	peer->xmt = pkt->org;
	pkt->refid = peer->refid;
	pkt->status &= ~ALARM;
	pkt->stratum = peer->stratum;
	pkt->ppoll = 0xff;
	pkt->precision = peer->precision;
	double_to_s_fixed(&pkt->distance, 0.0);
	double_to_s_fixed(&pkt->dispersion, 0.0);
#ifdef	DEBUG
	if (debug > 5) {
		printf("\nFaking packet ");
		dump_pkt(&peer->src, pkt, (struct ntp_peer *)NULL);
	}
#endif
	receive((struct sockaddr_in *)peer, pkt, otvp, -1);
	return;
}
#endif /* REFCLOCK */

/* 3.4.2. Receive Procedure */
void
receive(dst, pkt, tvp, sock)
	struct sockaddr_in *dst;
	struct ntpdata *pkt;
	struct timeval *tvp;
	int sock;
{
	struct ntp_peer *peer;
	int peer_mode;

#define	ACT_ERROR	1
#define	ACT_RECV	2
#define	ACT_XMIT	3
#define	ACT_PKT		4

	static char actions[5][5] = {

 /*      Sym Act   Sym Pas    Client     Server     Broadcast  |Host /       */
 /*      --------   --------  --------   ---------  ---------  |    / Peer   */
 /*                                                            ------------  */
	{ACT_PKT,  ACT_PKT,   ACT_RECV,  ACT_XMIT,  ACT_XMIT},	/* Sym Act   */
	{ACT_PKT,  ACT_ERROR, ACT_RECV,  ACT_ERROR, ACT_ERROR},	/* Sym Pas   */
	{ACT_XMIT, ACT_XMIT,  ACT_ERROR, ACT_XMIT,  ACT_XMIT},	/* Client    */
	{ACT_PKT,  ACT_ERROR, ACT_RECV,  ACT_ERROR, ACT_ERROR},	/* Server    */
	{ACT_PKT,  ACT_ERROR, ACT_RECV,  ACT_ERROR, ACT_ERROR}};/* Broadcast */

	/* if we're only going to support NTP Version 2 then this stuff
	   isn't necessary, right? */

	if ((peer_mode = pkt->status & MODEMASK) == 0 && dst) {
		/* packet from an older NTP implementation.  Synthesize the
		   correct mode.  The mapping goes like this:

		   pkt source port      pkt dst port	Mode
		   ---------------	------------	----
		   NTP Port		NTP Port	symmetric active
		   NTP Port		not NTP Port	server
		   not NTP Port		NTP Port	client
		   not NTP Port		not NTP Port	<not possible>

		   Now, since we only are processing packets with the
		   destination being NTP Port, it reduces to the two cases:

		   pkt source port      pkt dst port	Mode
		   ---------------	------------	----
		   NTP Port		NTP Port	symmetric active
		   not NTP Port		NTP Port	client		 */

		if (dst->sin_port == servp->s_port)
			peer_mode = MODE_SYM_ACT;
		else
			peer_mode = MODE_CLIENT;
	}

	if (peer_mode == MODE_CLIENT) {
		/*
		 * Special case: Use the dummy peer item that we keep around
		 * just for this type of thing
		 */
		peer = &dummy_peer;
		make_new_peer(peer);
		peer->src = *dst;
		peer->sock = sock;
		peer->hmode = MODE_SYM_PAS;
		peer->reach = 0;
		clear(peer);
#ifdef	REFCLOCK
	} else if (sock == -1) {
		/* we're begin called by refclock_input(), get peer ptr */
		peer = (struct ntp_peer *)dst;
#endif
	} else
		peer = check_peer(dst, sock);

	if (peer == NULL) {
		peer = (struct ntp_peer *) malloc(sizeof(struct ntp_peer));
		if (peer == NULL) {
			syslog(LOG_ERR, "peer malloc: %m");
			return;
		}
		make_new_peer(peer);
		peer->src = *dst;
		peer->sock = sock;	/* remember which socket we heard 
					   this from */
		peer->hmode = MODE_SYM_PAS;
		peer->reach = 0;
		clear(peer);
		/*
		 *  If we decide to consider any random NTP peer that might
		 *  come as a peer we might sync to, then set the PEER_FL_SYNC
		 *  flag in the peer structure.
		 *
		 *  Alternatively, we could change the hmode to MODE_SERVER, 
		 *  but then the peer state wouldn't be persistant.
		 */
		if (trusting)
			peer->flags |= PEER_FL_SYNC;

		enqueue(&peer_list, peer);
	}

	/* 
	 *  "pre-configured" peers are initially assigned a socket index of
	 *  -1, which means we don't know which interface we'll use to talk
	 *  to them.  Once the first reply comes back, we'll update the
	 *  peer structure
	 */
	if (peer->sock == -1)
		peer->sock = sock;

#ifdef	BROADCAST_NTP
	/*
	 *  Input frame matched a funny broadcast peer;  these peers only
	 *  exist to periodically generate broadcasts.  If an input packet
	 *  matched, it means that it looked like it *came* from the broadcast
	 *  address.  This is clearly bogus.
	 */
	if (peer->flags & PEER_FL_BCAST) {
#ifdef	DEBUG
		if (debug > 1)
			printf("receive: input frame for broadcast peer?\n");
#endif
		return;
	}
#endif	/* BROADCAST_NTP */

#if	0
	if ((peer->flags & PEER_FL_AUTHENABLE) &&
	    pkt->mac) {
		/* verify computed crypto-checksum */
	}
#endif

	if (peer_mode < MODE_SYM_ACT || peer_mode > MODE_BROADCAST) {
		syslog(LOG_DEBUG, "Bogus peer_mode %d from %s", peer_mode,
		       ntoa(dst->sin_addr));
#ifdef	DEBUG
		if (debug > 3) abort();
#endif
		return;
	}

	if (peer->hmode < MODE_SYM_ACT || peer->hmode > MODE_BROADCAST) {
		syslog(LOG_ERR, "Bogus hmode %d for peer %s", peer->hmode,
		       ntoa(peer->src.sin_addr));
		abort();
	}

	switch (actions[peer_mode - 1][peer->hmode - 1]) {
	case ACT_RECV:
		if (!(((peer->flags & PEER_FL_CONFIG) == 0) &&
		      STRMCMP(pkt->stratum, >, sys.stratum))) {
			peer->reach |= 1;
			process_packet(dst, pkt, tvp, peer);
			break;
		}
		/* Note fall-through */
	case ACT_ERROR:
		if (((peer->flags & PEER_FL_CONFIG) == 0) &&
		    (peer != &dummy_peer))
			demobilize(&peer_list, peer);
		break;

	case ACT_PKT:
		if (!(((peer->flags & PEER_FL_CONFIG) == 0) &&
		      STRMCMP(pkt->stratum, >, sys.stratum))) {
			peer->reach |= 1;
			process_packet(dst, pkt, tvp, peer);
			break;
		}
		/* Note fall-through */
	case ACT_XMIT:
		process_packet(dst, pkt, tvp, peer);
		poll_update(peer, peer->ppoll);
		transmit(peer);
		break;

	default:
		abort();
	}
}

#undef	ACT_ERROR
#undef	ACT_RECV
#undef	ACT_XMIT
#undef	ACT_PKT


/* 3.4.3 Packet procedure */
void
process_packet(dst, pkt, tvp, peer)
	struct sockaddr_in *dst;
	struct ntpdata *pkt;
	struct timeval *tvp;
	struct ntp_peer *peer;
{
	double t1, t2, t3, t4, offset, delay;
	short duplicate, bogus;

	duplicate = (pkt->xmt.int_part == peer->org.int_part) &&
		    (pkt->xmt.fraction == peer->org.fraction);

	bogus = ((pkt->org.int_part != peer->xmt.int_part) ||
		 (pkt->org.fraction != peer->xmt.fraction))
		|| (peer->xmt.int_part == 0);

	peer->pkt_rcvd++;
	peer->leap = pkt->status & LEAPMASK;
	peer->stratum = pkt->stratum;
	peer->ppoll = pkt->ppoll;
	peer->precision = pkt->precision;
	peer->distance = pkt->distance;
	peer->dispersion = pkt->dispersion;
	peer->refid = pkt->refid;
	peer->reftime = pkt->reftime;
	peer->org = pkt->xmt;
	tstamp(&peer->rec, tvp);
#ifdef	DEBUG
	if (debug > 3)
	    printf("Input Timestamp is     %08lx.%08lx %f\n",
		   ntohl(peer->rec.int_part),
		   ntohl(peer->rec.fraction),
		   ul_fixed_to_double(&peer->rec));
#endif
	poll_update(peer, peer->hpoll);

	/* 
	 * may want to do something special here for Broadcast Mode peers to
	 * allow these through 
	 */
	if (bogus || duplicate || 
	    (pkt->org.int_part == 0 && pkt->org.fraction == 0) ||
	    (pkt->rec.int_part == 0 && pkt->rec.fraction == 0)) {
		peer->pkt_dropped++;
#ifdef	DEBUG
		if (debug > 3)
			printf("process_packet: dropped duplicate or bogus\n");
#endif
		return;
	}

	/*
	 *  Now compute local adjusts 
	 */
	t1 = ul_fixed_to_double(&pkt->org);
	t2 = ul_fixed_to_double(&pkt->rec);
	t3 = ul_fixed_to_double(&pkt->xmt);
	t4 = ul_fixed_to_double(&peer->rec);
#ifdef	DEBUG
	if ((t1 > t4) || (t2 > t3))
	    printf("process_packet: out of order timestamps %f %f %f %f\n",
		   t1, t2, t3, t4);
	if (debug > 3) {
	    double sys_p, peer_p;

	    sys_p = 1.0/(afs_uint32)(1L << -sys.precision);
	    if ((sys_p < 0.000001) || (sys_p > 0.1))
		printf("process_packet: bogus sys precision %f\n", sys_p);

	    if (peer->precision < 0 && -peer->precision < sizeof(afs_int32)*NBBY) {
		peer_p = 1.0/(afs_uint32)(1L << -peer->precision);
		if ((peer_p < 0.000001) || (peer_p > 0.1))
		    printf("process_packet: bogus peer precision %f\n",peer_p);
	    }
	    else peer_p = 0;
	    if ((t4 - t1)+sys_p < (t3 - t2)-peer_p)
		printf("process_packet: non-concentric interval %f %f %f %f\n",
		       t1, t2, t3, t4);
	}
#endif

	/* 
	 * although the delay computation looks different than the one in the
	 * specification, it is correct.  Think about it.
	 */
	delay = (t2 - t1) - (t3 - t4);
	offset = ((t2 - t1) + (t3 - t4)) / 2.0;

	delay += 1.0/(afs_uint32)(1L << -sys.precision)
#ifndef	REFCLOCK
		+ NTP_MAXSKW;
#else
		+ (peer->flags&PEER_FL_REFCLOCK ? NTP_REFMAXSKW : NTP_MAXSKW);
#endif
	if (peer->precision < 0 && -peer->precision < sizeof(afs_int32)*NBBY)
		delay += 1.0/(afs_uint32)(1L << -peer->precision);

	if (delay < 0.0) {
		peer->pkt_dropped++;
#ifdef	DEBUG
		if (debug > 3)
			printf("process_packet: negative delay %f, timestamps: %f %f %f %f\n", delay, t1, t2, t3, t4);
#endif
		return;
	}

#ifndef	REFCLOCK
	delay = MAX(delay, NTP_MINDIST);
#else
	delay = MAX(delay, (peer->flags & PEER_FL_REFCLOCK) ?
		    NTP_REFMINDIST : NTP_MINDIST);
#endif

	peer->valid = 0;
	clock_filter(peer, delay, offset);  /* invoke clock filter procedure */
#ifdef	DEBUG
	if (debug) {
		printf("host: %s : %f : %f : %f : %f : %f : %o\n",
		       dst ? ntoa(dst->sin_addr) : "refclock",
		       delay, offset,
		       peer->estdelay, peer->estoffset, peer->estdisp,
		       peer->reach);
	}
#endif
	clock_update(peer);		/* call clock update procedure */
}

/* 3.4.4 Primary clock procedure */
/*
 *  We don't have a primary clock.
 *
 *  TODO:
 *
 *  ``When a  primary clock is connected to the host, it is convient to
 *    incorporate its information into the database as if the clock was
 *    represented as an ordinary peer.  The clock can be polled once a
 *    minute or so and the returned timecheck used to produce a new update
 *    for the logical clock.''
 */


/* 3.4.5 Clock update procedure */

void
clock_update(peer)
	struct ntp_peer *peer;
{
	double temp;
	extern int adj_logical();

	select_clock();
	if (sys.peer != NULL)
		poll_update(sys.peer, NTP_MINPOLL);

	/*
	 * Did we just sync to this peer?
	 */
	if ((peer == sys.peer) && (sys.hold == 0)) {
		char buf[200];

		/*
		 *  Update the local system variables
		 */
		sys.leap = peer->leap;
#ifndef	REFCLOCK
		sys.stratum = peer->stratum + 1;
		sys.refid = peer->src.sin_addr.s_addr;
#else
		if (peer->flags & PEER_FL_REFCLOCK) {
			/* once we re-map the stratums so that stratum 0 is
			   better than stratum 1, some of this foolishness
			   can go away */
			sys.stratum = peer->stratum;
			sys.refid = peer->refid;
		} else {
			sys.stratum = peer->stratum + 1;
			sys.refid = peer->src.sin_addr.s_addr;
		}
#endif

		temp = s_fixed_to_double(&peer->distance) + peer->estdelay;
		double_to_s_fixed(&sys.distance, temp);

		temp = s_fixed_to_double(&peer->dispersion) + peer->estdisp;
		double_to_s_fixed(&sys.dispersion, temp);

		sys.reftime = peer->rec;

#ifdef	DEBUG
		if (debug > 3)
			printf("clock_update: synced to peer, adj clock\n");
#endif

		/*
		 * Sanity check: is computed offset insane?
		 */
		if (peer->estoffset > WayTooBig ||
		    peer->estoffset < -WayTooBig) {
			syslog(LOG_ERR, "Clock is too far off %f sec. [%s]",
			       peer->estoffset, ntoa(peer->src.sin_addr));
#ifdef	DEBUG
			if (debug)
				printf("Clock is too far off %f sec. [%s] (max %f)\n",
				       peer->estoffset,
				       ntoa(peer->src.sin_addr), WayTooBig);
#endif	/*DEBUG*/
			return;
		}

		clock_watchdog = 0;	/* reset watchdog timer */
		if (adj_logical(peer->estoffset) > 0) {
			register struct ntp_peer *p = peer_list.head;
			/* did you know syslog only took 4 parameters? */
			sprintf(buf,
			        "adjust: STEP %s st %d off %f drft %f cmpl %f",
				inet_ntoa(peer->src.sin_addr), peer->stratum,
				peer->estoffset, drift_comp, compliance);
			syslog(LOG_INFO, buf);
#ifdef	DEBUG
			if (debug)
				printf("Clockset from %s stratum %d offset %f\n",
				       inet_ntoa(peer->src.sin_addr),
				       peer->stratum, peer->estoffset);

#endif
			while (p) {
				clear(p);
				p = p->next;
			}
			sys.hold = PEER_SHIFT * (1 << NTP_MINPOLL);
#ifdef	DEBUG
			if (debug > 3)
				printf("clock_updates: STEP ADJ\n");
#endif
		} else {
			if (logstats 
#ifdef DEBUG
				|| (debug > 1)
#endif
				) {
				sprintf(buf,
			        "adjust: SLEW %s st %d off %f drft %f cmpl %f",
					inet_ntoa(peer->src.sin_addr),
					peer->stratum,
					peer->estoffset, drift_comp,
					compliance);
				if (logstats) syslog(LOG_INFO, buf);
#ifdef DEBUG
				if (debug > 1) printf ("%s\n", buf);
#endif
			}
		}
	}
}

/* 3.4.6 Initialization procedure */

void
initialize()
{
	sys.leap = ALARM;	/* indicate unsynchronized */
	sys.stratum = 0;
	sys.precision = 0;	/* may be specified in the config file;
				   if not, gets set in init_kern_vars() */
#if	0	/* under construction */
	sys.keyid = 0;
	sys.keys = ??;
#endif
	sys.distance.int_part = sys.distance.fraction = 0; 
	sys.dispersion.int_part = sys.dispersion.fraction = 0; 
	sys.refid = 0;
	sys.reftime.int_part = sys.reftime.fraction = 0;
	sys.hold = 0;
	sys.peer = NULL;
}

/* 3.4.7 Clear Procedure */
void
clear(peer)
	register struct ntp_peer *peer;
{
	register int i;

#ifdef	DEBUG
	if (debug > 3)
		printf("clear: emptied filter for %s\n",
		       ntoa(peer->src.sin_addr));
#endif
	peer->hpoll = NTP_MINPOLL;
	peer->estdisp = PEER_MAXDISP;
	for (i = 0; i < NTP_WINDOW; i++)
		peer->filter.offset[i] = 0.0;
	peer->filter.samples = 0;	/* Implementation specific */
	peer->valid = 0;
	peer->org.int_part = peer->org.fraction = 0;
	peer->rec.int_part = peer->rec.fraction = 0;
	peer->xmt.int_part = peer->xmt.fraction = 0;
	poll_update(peer, NTP_MINPOLL);
	select_clock();
	if (sys.peer != NULL)
		poll_update(sys.peer, NTP_MINPOLL);
}


/* 3.4.8 Poll Update Procedure */
void
poll_update(peer, new_hpoll)
	register struct ntp_peer *peer;
	int new_hpoll;
{
	int interval;

	peer->hpoll = MAX(NTP_MINPOLL, MIN(NTP_MAXPOLL, new_hpoll));

#if	XTAL	/* if crystal controlled clock */
	if (peer == sys.peer)
#endif
		peer->hpoll = NTP_MINPOLL;

	interval = 1 << (MAX(MIN(peer->ppoll, MIN(peer->hpoll, NTP_MAXPOLL)),
		       NTP_MINPOLL));

#ifdef	REFCLOCK
	if (peer->flags & PEER_FL_REFCLOCK)
		interval = 1 << NTP_MINPOLL;
#endif
	if (interval == peer->timer)
		return;

	/* only randomize when poll interval changes */
	if (interval < peer->timer)
		peer->timer = interval;

	/*
	 * "Rand uses a multiplicative congruential random number gen-
	 * erator with period 2**32 to return successive pseudo-random
	 * numbers in the range from 0 to (2**31)-1"
	 */
	interval = (double)interval * 
		((double)rand()/(double)((afs_uint32)(1L<<31) - 1));

#ifdef	DEBUG
	if (debug > 3)
		printf("poll_update: timer %d, poll=%d\n", peer->timer,
		       interval);
#endif
}


/* 3.4.9 Authentication Procedures */
#if	0
encrypt() {}
decrypt() {}
#endif

/* 4.1 Clock Filter Procedure */
/*
 *  The previous incarnation of this code made the assumption that
 *  the value of PEER_FILTER was a power of two and used shifting.
 *  This version has been generalized, so that experimenting with
 *  different PEER_FILTER values should be much easier.
 */

void
clock_filter(peer, new_delay, new_offset)
	register struct ntp_peer *peer;
	double new_delay, new_offset;
{
	double offset[PEER_SHIFT], delay[PEER_SHIFT];
	register double temp, d, w;
	register int i, j, samples;

	if (peer->filter.samples < PEER_SHIFT)
		peer->filter.samples++;
	/*
	 *  Too bad C doesn't have a barrel shifter...
	 */
	for (i = PEER_SHIFT - 1; i; i--) {
		peer->filter.offset[i] = peer->filter.offset[i - 1];
		peer->filter.delay[i] = peer->filter.delay[i - 1];
	}
	peer->filter.offset[0] = new_offset;
	peer->filter.delay[0] = new_delay;

	samples = 0;
	/*
	 *  Now sort the valid (non-zero delay) samples into a temporary
	 *  list by delay.
	 *
	 *  First, build the temp list...
	 */
	for (i = 0; i < peer->filter.samples; i++) {
		if (peer->filter.delay[i] != 0.0) {
			offset[samples] = peer->filter.offset[i];
			delay[samples++] = peer->filter.delay[i];
		}
	}
	/* ..and now sort it. */
	if (samples) {
		for (i = 0; i < samples - 1; i++) {
			for (j = i + 1; j < samples; j++) {
				if (delay[i] > delay[j]) {
					temp = delay[i];
					delay[i] = delay[j];
					delay[j] = temp;
					temp = offset[i];
					offset[i] = offset[j];
					offset[j] = temp;
				}
			}
		}
		/* samples are now sorted by delay */

		peer->estdelay = delay[0];
		peer->estoffset = offset[0];
	}

	temp = 0.0;
	w = 1.0;

	for (i = 0; i < PEER_SHIFT; i++) {
		if (i >= samples)
			d = PEER_MAXDISP;
		else {
			if ((d = offset[i] - offset[0]) < 0)
				d = -d;
			if (d > PEER_MAXDISP)
				d = PEER_MAXDISP;
		}
		temp += d * w;
		/* compute  PEER_FILTER**i  as we go along */
		w *= PEER_FILTER;
	}
	peer->estdisp = temp;
#ifdef	DEBUG
	if (debug > 3)
		printf("clock_filter: estdelay %f, estoffset %f, estdisp %f\n",
		       peer->estdelay, peer->estoffset, peer->estdisp);
#endif
}

/* 4.2 Clock Select Procedure */
void
select_clock() {
	struct ntp_peer *ptmp, *peer = peer_list.head;
	struct sel_lst {
		struct ntp_peer *peer;
		double distance;
		double precision;
	} sel_lst[X_NTP_CANDIDATES];
	int i, j, stratums, candidates;
	int sanity_check();
	double falsetick(), dtmp;

	candidates = 0;
	stratums = 0;

	while (peer != NULL && candidates < X_NTP_CANDIDATES) {
		/*
		 * Check if this is a candidate for "sys.peer" 
		 */
		peer->flags &= ~(PEER_FL_SANE | PEER_FL_CANDIDATE);
		if(sanity_check(peer)) {
			sel_lst[candidates].peer = peer;
			sel_lst[candidates].distance = peer->estdisp + 
				s_fixed_to_double(&peer->dispersion);
			peer->flags |= PEER_FL_SANE;
			candidates++;
		}
		peer = peer->next;
	}
#ifdef	DEBUG
	if (debug > 3)
		printf("select_clock: step1 %d candidates\n", candidates);
#endif
	/*
	 *  If no candidates passed the sanity check, then give up.
	 */
	if (!candidates) {
		if (sys.peer != NULL) {
			syslog(LOG_INFO, "Lost NTP peer %s",
			       inet_ntoa(sys.peer->src.sin_addr));
#ifdef	DEBUG
			if (debug)
				printf("Lost NTP peer %s\n",
				       inet_ntoa(sys.peer->src.sin_addr));
#endif
		}
#ifdef	DEBUG
		if (debug > 3)
			printf("select_clock: no candidates\n");
#endif
		sys.peer = NULL;
		/*
		 * leave sys.stratum and sys.refid intact after losing 
		 * reachability to all clocks.  After 24 hours, we'll
		 * set the alarm condition if we didn't get any clock
		 * updates.
		 */
		return;
	}

	/* 
	 *  Sort the list.  We assume that sanity_check() above trashed any
	 *  peers which were stratum 0, so we can safely compare stratums
	 *  below.  Sort the list by stratum.  Where stratums are equal, the
	 *  peer with the lowest (peer.estdisp + peer.dispersion) is preferred.
	 */
	for (i = 0; i < candidates - 1; i++) {
		for (j = i + 1; j < candidates; j++) {
			if ((sel_lst[i].peer->stratum > sel_lst[j].peer->stratum) ||
			    ((sel_lst[i].peer->stratum == sel_lst[j].peer->stratum)
			     && (sel_lst[i].distance > sel_lst[j].distance))) {
				ptmp = sel_lst[i].peer;
				dtmp = sel_lst[i].distance;
				sel_lst[i].peer = sel_lst[j].peer;
				sel_lst[i].distance = sel_lst[j].distance;
				sel_lst[j].peer = ptmp;
				sel_lst[j].distance = dtmp;
			}
		}
	}

#ifdef	DEBUG
	if (debug > 3)
		printf("select_clock: step2 %d candidates\n", candidates);
#endif

	/* truncate the list at NTP_MAXLIST peers */
	if (candidates > NTP_MAXLIST)
		candidates = NTP_MAXLIST;

#ifdef	DEBUG
	if (debug > 3)
		printf("select_clock: step3 %d candidates\n", candidates);
#endif

	/* truncate list where number of different strata exceeds NTP_MAXSTRA */
	for (stratums = 0, i = 1; i < candidates; i++) {
		if (sel_lst[i - 1].peer->stratum != sel_lst[i].peer->stratum) {
			if (++stratums > NTP_MAXSTRA) {
#ifdef	DEBUG
				if (debug > 2)
					printf("select_clock: truncated to %d peers\n", i);
#endif
				candidates = i;

				break;
			}
		}
	}
#ifdef	DEBUG
	if (debug > 3)
		printf("select_clock: step4 %d candidates\n", candidates);
#endif
	/*
	 * Kick out falsetickers
	 */
	/* now, re-sort the list by peer.stratum and peer.estdelay */
	for (i = 0; i < candidates - 1; i++) {
		for (j = i + 1; j < candidates; j++) {
			if ((sel_lst[i].peer->stratum > sel_lst[j].peer->stratum) ||
			    ((sel_lst[i].peer->stratum == sel_lst[j].peer->stratum)
			     && (sel_lst[i].peer->estdelay >
				 sel_lst[j].peer->estdelay))) {
				ptmp = sel_lst[i].peer;
				sel_lst[i].peer = sel_lst[j].peer;
				sel_lst[j].peer = ptmp;
			}
		}
	}
	while (candidates > 1) {
		double maxdispersion = 0.0, dispersion, weight;
		double min_precision_thres = 10e20, precision_thres;
		short worst = 0;	/* shut up GNU CC about unused var */
#ifdef	DEBUG
		if (debug > 3)
			printf("select_clock: step5 %d candidates\n", candidates);
#endif
		for (i = 0; i < candidates; i++) {
			/* compute dispersion of candidate `i' relative to the
			   rest of the candidates */
			dispersion = 0.0;
			weight = 1.0;
			sel_lst[i].peer->flags |= PEER_FL_CANDIDATE;
			for (j = 0; j < candidates; j++) {
				dtmp = sel_lst[j].peer->estoffset -
					sel_lst[i].peer->estoffset;
				if (dtmp < 0)
					dtmp = -dtmp;
				dispersion += dtmp * weight;
				weight *= NTP_SELECT;
			}
			/* since we just happen to have this double floating
			   around.. */
			sel_lst[i].distance = dispersion;
			
			precision_thres = NTP_MAXSKW + 1.0/(1<<-sys.precision);
			if (sel_lst[i].peer->precision < 0 &&
			    -sel_lst[i].peer->precision < sizeof(afs_int32)*NBBY)
				precision_thres +=
					1.0/(1<<-sel_lst[i].peer->precision);

			sel_lst[i].precision = precision_thres;

			if (dispersion >= maxdispersion) {
				maxdispersion = dispersion;
				worst = i;
			}
			if (precision_thres < min_precision_thres) {
				min_precision_thres = precision_thres;
			}
#ifdef	DEBUG
			if (debug > 4) {
				printf(" peer %s => disp %f prec_th %f\n",
				       ntoa(sel_lst[i].peer->src.sin_addr),
				       dispersion, precision_thres);
			}
#endif
		}
		/*
		 *  Now check to see if the max dispersion is greater than
		 *  the min dispersion limit.  If so, crank again, otherwise
		 *  bail out.
		 */
		if (! (maxdispersion > min_precision_thres)) {
#ifdef	DEBUG
			if (debug > 4)
				printf(" %d left valid\n", candidates);
#endif
			break;
		}
			
#ifdef	DEBUG
		if (debug > 4)
			printf(" peer %s => TOSS\n",
			       ntoa(sel_lst[worst].peer->src.sin_addr));
#endif
		/*
		 *  now, we need to trash the peer with the worst dispersion
		 *  and interate until there is only one candidate peer left.
		 */
		if (worst != candidates - 1) {
			sel_lst[worst].peer->flags &= ~PEER_FL_CANDIDATE;
			for (i = worst, j = worst + 1; j < candidates; )
				sel_lst[i++].peer = sel_lst[j++].peer;
		}
		candidates--;
		/* one more time.. */
	}
#ifdef	DEBUG
	if (debug > 3)
		printf("select_clock: step6 %d candidates\n", candidates);
#endif

	/*
	 *  Check to see if current peer is on the list of candidate peers.  If
	 *  don't change sys.peer.  Note that if the first selected clock is
	 *  at a lower stratum, don't even bother; we're going to want to
	 *  switch to it.
	 */
	if (sys.peer != NULL && 
	    (sys.peer->stratum <= sel_lst[0].peer->stratum)) {
		for (i = 0; i < candidates; i++) {
			if (sys.peer == sel_lst[i].peer) {
				/*
				 * The clock we're currently synchronized to
				 * is among the candidate peers.  Don't switch.
				 */
				if (i != 0) {
					/*
					 *  Count instances where the best 
					 *  candidate is different from the
					 *  current clock, thus inhibiting
					 *  clockhopping.
					 */
					peer_sw_inhibited++;
				}
				return;
			}
		}
	}

	/*
	 *  The currently selected peer (if any) isn't on the candidate list.
	 *  Grab the first one and let it be.
	 */

	if (sys.peer != sel_lst[0].peer) {
		if (sys.peer != NULL)
			syslog(LOG_INFO, "clock: select peer %s stratum %d was %s stratum %d",
			       ntoa(sel_lst[0].peer->src.sin_addr),
			       sel_lst[0].peer->stratum,
			       ntoa(sys.peer->src.sin_addr), sys.peer->stratum);
		else
			syslog(LOG_INFO, "clock: select peer %s stratum %d was UNSYNCED",
			       ntoa(sel_lst[0].peer->src.sin_addr),
			       sel_lst[0].peer->stratum);
		
#ifdef	DEBUG
		if (debug > 2)
			printf("clock: select peer %s stratum %d of %d cand\n",
			       ntoa(sel_lst[0].peer->src.sin_addr),
			       sel_lst[0].peer->stratum, candidates);
#endif
		sys.peer = sel_lst[0].peer;
		peer_switches++;
	}
}

int
sanity_check(peer)
	struct ntp_peer *peer;
{
#ifdef	DEBUG
	if (debug > 7)
		printf("Checking peer %s stratum %d\n",
		       inet_ntoa(peer->src.sin_addr), peer->stratum);
#endif
	/* Sanity check 0. ?? */
	if (!(peer->flags & PEER_FL_SYNC))
		return(0);

	/* Sanity check 1. */
	if (peer->stratum <= 0 || peer->stratum >= NTP_INFIN)
		return(0);

	/* Sanity check 2.
	   if peer.stratum is greater than one (synchronized via NTP),
	   peer.refid must not match peer.dstadr */

	if (peer->stratum > 1) {
		register int i;
		for (i = 1; i < nintf; i++)
			if (addrs[i].sin.sin_addr.s_addr == peer->refid)
				return (0);
	}

	/* Sanity check 3.
	   Both peer.estdelay and
	   peer.estdisp to be less than NTP_MAXWGT, which insures that the
	   filter register at least half full, yet avoids using data from
	   very noisy associations or broken implementations.  	*/
	if (peer->estdisp > (float)NTP_MAXWGT || 
	    peer->estdelay > (float)NTP_MAXWGT)
		return(0);

	/*  Sanity check 4.
	    The peer clock must be synchronized... and the interval since
	    the peer clock was last updated satisfy

	    peer.org - peer.reftime < NTP.MAXAGE
	 */
	if (peer->leap == ALARM ||
	    (ul_fixed_to_double(&peer->org)
	     - ul_fixed_to_double(&peer->reftime)) >= NTP_MAXAGE)
		return(0);

#ifdef	DEBUG
	if (debug > 7)
		printf("That one is certainly qualified %s\n",
		       inet_ntoa(peer->src.sin_addr));
#endif
	return(1);
}
