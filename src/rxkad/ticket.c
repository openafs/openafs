/* Copyright (C) 1990 Transarc Corporation - All rights reserved */
/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/
/*
 * Revision 2.3  90/08/31  16:19:53
 * Move permit_xprt.h.
 * 
 * Revision 2.2  90/08/20  10:21:25
 * Include permit_xprt.h.
 * Cleanup; prune log, flush andrew style ticket functions.
 * 
 * Revision 2.1  90/08/07  19:33:44
 * Start with clean version to sync test and dev trees.
 * */

#if defined(UKERNEL)
#include "../afs/param.h"
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../afs/stds.h"
#include "../rx/xdr.h"
#include "../rx/rx.h"
#include "../des/des.h"
#include "../afs/lifetimes.h"
#include "../afs/rxkad.h"

#include "../afs/permit_xprt.h"
#else /* defined(UKERNEL) */
#include <afs/param.h>
#include <afs/stds.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <rx/xdr.h>
#include <rx/rx.h>
#include <des.h>
#include "lifetimes.h"
#include "rxkad.h"

#include "../permit_xprt.h"
#endif /* defined(UKERNEL) */


extern afs_int32 ktohl();
extern afs_uint32 life_to_time();
extern unsigned char time_to_life();

static int decode_athena_ticket();
static int assemble_athena_ticket();

#define ANDREWFLAGSVALUE (0x80)
#define TICKET_LABEL "TicketEnd"

/* This is called to interpret a ticket.  It is assumed that the necessary keys
   have been added so that the key version number in the ticket will indicate a
   valid key for decrypting the ticket.  The various fields inside the ticket
   are copied into the return arguments.  An error code indicate some problem
   interpreting the ticket and the values of the output parameters are
   undefined. */

int tkt_DecodeTicket (asecret, ticketLen, key,
		      name, inst, cell, sessionKey, host, start, end)
  char		*asecret;
  afs_int32		 ticketLen;
  struct ktc_encryptionKey *key;
  char		*name;
  char		*inst;
  char		*cell;
  char		*sessionKey;
  afs_int32		*host;
  afs_int32		*start;
  afs_int32		*end;
{   char	   clear_ticket[MAXKTCTICKETLEN];
    char	  *ticket;
    Key_schedule   schedule;
    /* unsigned char  flags; */
    int		   code;

    if (ticketLen == 0) return RXKADBADTICKET; /* no ticket */
    if ((ticketLen < MINKTCTICKETLEN) || /* minimum legal ticket size */
	((ticketLen) % 8 != 0))		/* enc. part must be (0 mod 8) bytes */
	return RXKADBADTICKET;

    if (key_sched (key, schedule)) return RXKADBADKEY;

    ticket = clear_ticket;
    pcbc_encrypt (asecret, ticket, ticketLen, schedule, key, DECRYPT);

    /* flags = *ticket; */		/* get the first byte: the flags */
#if 0
    if (flags == ANDREWFLAGSVALUE) {
	code = decode_andrew_ticket (ticket, ticketLen, name, inst, cell,
				     host, sessionKey, start, end);
	if (code) {
	    code = decode_athena_ticket (ticket, ticketLen, name, inst, cell,
					 host, sessionKey, start, end);
	    flags = 0;
	}
    }
    else {
	code = decode_athena_ticket (ticket, ticketLen, name, inst, cell,
				     host, sessionKey, start, end);
	if (code) {
	    code = decode_andrew_ticket (ticket, ticketLen, name, inst, cell,
					 host, sessionKey, start, end);
	    flags = ANDREWFLAGSVALUE;
	}
    }
#else
    code = decode_athena_ticket
	(ticket, ticketLen, name, inst, cell, host, sessionKey, start, end);
    /* flags = 0; */

#endif
    if (code) return RXKADBADTICKET;
    if (tkt_CheckTimes (*start, *end, time(0)) < -1) return RXKADBADTICKET;

    return 0;
}

/* This makes a Kerberos ticket */

int tkt_MakeTicket (ticket, ticketLen, key, name, inst, cell,
		    start, end, sessionKey, host, sname, sinst)
  char		*ticket;		/* ticket is constructed here */
  int		*ticketLen;		/* output length of finished ticket */
  struct ktc_encryptionKey *key;	/* key ticket should be sealed with */
  char		*name;			/* user of this ticket */
  char		*inst;
  char		*cell;			/* cell of authentication */
  afs_uint32	 start,end;		/* life of ticket */
  struct ktc_encryptionKey *sessionKey;	/* session key invented for ticket */
  afs_uint32	 host;			/* caller's host address */
  char		*sname;			/* server */
  char		*sinst;
{   int		 code;
    Key_schedule schedule;

    *ticketLen = 0;			/* in case we return early */
    code = assemble_athena_ticket (ticket, ticketLen, name, inst, cell,
				   host, sessionKey, start, end, sname, sinst);
    *ticketLen = round_up_to_ebs(*ticketLen); /* round up */
    if (code) return -1;

    /* encrypt ticket */
    if (code = key_sched (key, schedule)) {
	printf ("In tkt_MakeTicket: key_sched returned %d\n", code);
	return RXKADBADKEY;
    }
    pcbc_encrypt (ticket, ticket, *ticketLen, schedule, key, ENCRYPT);
    return 0;
}

#define getstr(name,min) \
    slen = strlen(ticket); \
    if ((slen < min) || (slen >= MAXKTCNAMELEN)) return -1; \
    strcpy (name, ticket); \
    ticket += slen+1

static int decode_athena_ticket (ticket, ticketLen, name, inst, realm,
				  host, sessionKey, start, end)
  char *ticket;
  int   ticketLen;
  char *name;
  char *inst;
  char *realm;
  afs_int32 *host;
  struct ktc_encryptionKey *sessionKey;
  afs_uint32 *start;
  afs_uint32 *end;
{   char *ticketBeg = ticket;
    char  flags;
    int	  slen;
    int	  tlen;
    unsigned char  lifetime;
    char  sname[MAXKTCNAMELEN];		/* these aren't used, */
    char  sinst[MAXKTCNAMELEN];		/* but are in the ticket */

    flags = *ticket++;
    getstr (name, 1);
    getstr (inst, 0);
    getstr (realm, 0);

    bcopy (ticket, host, sizeof (*host));
    ticket += sizeof(*host);
    *host = ktohl (flags, *host);

    bcopy (ticket, sessionKey, sizeof (struct ktc_encryptionKey));
    ticket += sizeof (struct ktc_encryptionKey);

    lifetime = *ticket++;
    bcopy (ticket, start, sizeof (*start));
    ticket += sizeof(*start);
    *start = ktohl (flags, *start);
    *end = life_to_time (*start, lifetime);

    getstr (sname, 1);
    getstr (sinst, 0);

    tlen = ticket - ticketBeg;
    if ((round_up_to_ebs(tlen) != ticketLen) && (ticketLen != 56)) return -1;
    return 0;
}

#define putstr(name,min) \
    slen = strlen(name); \
    if ((slen < min) || (slen >= MAXKTCNAMELEN)) return -1; \
    strcpy (ticket, name); \
    ticket += slen+1
#define putint(num) num = htonl(num);\
		    bcopy (&num, ticket, sizeof(num));\
		    ticket += sizeof(num)

static int assemble_athena_ticket (ticket, ticketLen, name, inst, realm,
				    host, sessionKey, start, end, sname, sinst)
  char *ticket;
  int  *ticketLen;
  char *name;
  char *inst;
  char *realm;
  afs_int32  host;
  struct ktc_encryptionKey *sessionKey;
  afs_uint32 start;
  afs_uint32 end;
  char *sname;
  char *sinst;
{   char *ticketBeg = ticket;
    int	  slen;
    unsigned char  life;

    *ticket++ = 0;			/* flags, always send net-byte-order */
    putstr (name, 1);
    putstr (inst, 0);
    putstr (realm, 0);
    putint (host);

    bcopy (sessionKey, ticket, sizeof(struct ktc_encryptionKey));
    ticket += sizeof(struct ktc_encryptionKey);

    life = time_to_life (start, end);
    if (life == 0) return -1;
    *ticket++ = life;

    putint (start);
    putstr (sname, 1);
    putstr (sinst, 0);

    *ticketLen = ticket - ticketBeg;
    return 0;
}

/* This is just a routine that checks the consistency of ticket lifetimes.  It
   returns three values: */
/* -2 means the times are inconsistent or ticket has expired
   -1 means the ticket has recently expired.
    0 means the times are consistent but start time is in the (near) future.
    1 means the start time is in the past and the end time is infinity.
    2 means the start time is past and the end time is in the future
	    and the lifetime is within the legal limit.
 */

int tkt_CheckTimes (start, end, now)
  afs_uint32 start;
  afs_uint32 end;
  afs_uint32 now;
{   int active;

    if (start >= end) return -2;	/* zero or negative lifetime */
    if (start > now+KTC_TIME_UNCERTAINTY+MAXKTCTICKETLIFETIME)
	return -2;			/* starts too far in the future? */
    if ((start != 0) && (end != NEVERDATE) &&
	(end-start > MAXKTCTICKETLIFETIME)) return -2; /* too long a life */
    if ((end != NEVERDATE) && (end+KTC_TIME_UNCERTAINTY < now)) { /* expired */
	if ((start != 0) && (now - start > MAXKTCTICKETLIFETIME + 24*60*60))
	    return -2;
	else return -1;			/* expired only recently */
    }
    if ((start == 0) || (start-KTC_TIME_UNCERTAINTY <= now)) active = 1;
    else active = 0;			/* start time not yet arrived */

    if ((start == 0) || (end == NEVERDATE))
	return active; /* no expiration time */
    return active*2;			/* ticket valid */
}

afs_int32 ktohl (flags, l)
  char flags;
  afs_int32 l;
{
    if (flags & 1) {
	unsigned char *lp = (unsigned char *)&l;
	afs_int32	       hl;
	hl = *lp + (*(lp+1) << 8) + (*(lp+2) << 16) + (*(lp+3) << 24);
	return hl;
    }
    return ntohl(l);
}

/* life_to_time - takes a start time and a Kerberos standard lifetime char and
 * returns the corresponding end time.  There are four simple cases to be
 * handled.  The first is a life of 0xff, meaning no expiration, and results in
 * an end time of 0xffffffff.  The second is when life is less than the values
 * covered by the table.  In this case, the end time is the start time plus the
 * number of 5 minute intervals specified by life.  The third case returns
 * start plus the MAXTKTLIFETIME if life is greater than TKTLIFEMAXFIXED.  The
 * last case, uses the life value (minus TKTLIFEMINFIXED) as an index into the
 * table to extract the lifetime in seconds, which is added to start to produce
 * the end time. */

afs_uint32 life_to_time (start, life)
  afs_uint32 start;
  unsigned char life;
{   int realLife;

    if (life == TKTLIFENOEXPIRE) return NEVERDATE;
    if (life < TKTLIFEMINFIXED) return start + life*5*60;
    if (life > TKTLIFEMAXFIXED) return start + MAXTKTLIFETIME;
    realLife = tkt_lifetimes[life - TKTLIFEMINFIXED];
    return start + realLife;
}

/* time_to_life - takes start and end times for the ticket and returns a
 * Kerberos standard lifetime char possibily using the tkt_lifetimes table for
 * lifetimes above 127*5minutes.  First, the special case of (end ==
 * 0xffffffff) is handled to mean no expiration.  Then negative lifetimes and
 * those greater than the maximum ticket lifetime are rejected.  Then lifetimes
 * less than the first table entry are handled by rounding the requested
 * lifetime *up* to the next 5 minute interval.  The final step is to search
 * the table for the smallest entry *greater than or equal* to the requested
 * entry.  The actual code is prepared to handle the case where the table is
 * unordered but that it an unnecessary frill. */

unsigned char time_to_life (start, end)
  afs_uint32 start;
  afs_uint32 end;
{   int lifetime = end-start;
    int best, best_i;
    int i;

    if (end == NEVERDATE) return TKTLIFENOEXPIRE;
    if ((lifetime > MAXKTCTICKETLIFETIME) || (lifetime <= 0)) return 0;
    if (lifetime < tkt_lifetimes[0]) return (lifetime + 5*60-1) / (5*60);
    best_i = -1;
    best = MAXKTCTICKETLIFETIME;
    for (i=0; i<TKTLIFENUMFIXED; i++)
	if (tkt_lifetimes[i] >= lifetime) {
	    int diff = tkt_lifetimes[i]-lifetime;
	    if (diff < best) {
		best = diff;
		best_i = i;
	    }}
    if (best_i < 0) return 0;
    return best_i+TKTLIFEMINFIXED;
}
