/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include "rxkad_locl.h"

/* RCSID("$Id$"); */

/* Ticket lifetime.  This defines the table used to lookup lifetime for the
   fixed part of rande of the one byte lifetime field.  Values less than 0x80
   are intrpreted as the number of 5 minute intervals.  Values from 0x80 to
   0xBF should be looked up in this table.  The value of 0x80 is the same using
   both methods: 10 and two-thirds hours .  The lifetime of 0xBF is 30 days.
   The intervening values of have a fixed ratio of roughly 1.06914.  The value
   oxFF is defined to mean a ticket has no expiration time.  This should be
   used advisedly since individual servers may impose defacto upperbounds on
   ticket lifetimes. */

#define TKTLIFENUMFIXED 64
#define TKTLIFEMINFIXED 0x80
#define TKTLIFEMAXFIXED 0xBF
#define TKTLIFENOEXPIRE 0xFF
#define MAXTKTLIFETIME	(30*24*3600)	/* 30 days */

static const int tkt_lifetimes[TKTLIFENUMFIXED] = {
    38400,				/* 10.67 hours, 0.44 days */ 
    41055,				/* 11.40 hours, 0.48 days */ 
    43894,				/* 12.19 hours, 0.51 days */ 
    46929,				/* 13.04 hours, 0.54 days */ 
    50174,				/* 13.94 hours, 0.58 days */ 
    53643,				/* 14.90 hours, 0.62 days */ 
    57352,				/* 15.93 hours, 0.66 days */ 
    61318,				/* 17.03 hours, 0.71 days */ 
    65558,				/* 18.21 hours, 0.76 days */ 
    70091,				/* 19.47 hours, 0.81 days */ 
    74937,				/* 20.82 hours, 0.87 days */ 
    80119,				/* 22.26 hours, 0.93 days */ 
    85658,				/* 23.79 hours, 0.99 days */ 
    91581,				/* 25.44 hours, 1.06 days */ 
    97914,				/* 27.20 hours, 1.13 days */ 
    104684,				/* 29.08 hours, 1.21 days */ 
    111922,				/* 31.09 hours, 1.30 days */ 
    119661,				/* 33.24 hours, 1.38 days */ 
    127935,				/* 35.54 hours, 1.48 days */ 
    136781,				/* 37.99 hours, 1.58 days */ 
    146239,				/* 40.62 hours, 1.69 days */ 
    156350,				/* 43.43 hours, 1.81 days */ 
    167161,				/* 46.43 hours, 1.93 days */ 
    178720,				/* 49.64 hours, 2.07 days */ 
    191077,				/* 53.08 hours, 2.21 days */ 
    204289,				/* 56.75 hours, 2.36 days */ 
    218415,				/* 60.67 hours, 2.53 days */ 
    233517,				/* 64.87 hours, 2.70 days */ 
    249664,				/* 69.35 hours, 2.89 days */ 
    266926,				/* 74.15 hours, 3.09 days */ 
    285383,				/* 79.27 hours, 3.30 days */ 
    305116,				/* 84.75 hours, 3.53 days */ 
    326213,				/* 90.61 hours, 3.78 days */ 
    348769,				/* 96.88 hours, 4.04 days */ 
    372885,				/* 103.58 hours, 4.32 days */ 
    398668,				/* 110.74 hours, 4.61 days */ 
    426234,				/* 118.40 hours, 4.93 days */ 
    455705,				/* 126.58 hours, 5.27 days */ 
    487215,				/* 135.34 hours, 5.64 days */ 
    520904,				/* 144.70 hours, 6.03 days */ 
    556921,				/* 154.70 hours, 6.45 days */ 
    595430,				/* 165.40 hours, 6.89 days */ 
    636601,				/* 176.83 hours, 7.37 days */ 
    680618,				/* 189.06 hours, 7.88 days */ 
    727680,				/* 202.13 hours, 8.42 days */ 
    777995,				/* 216.11 hours, 9.00 days */ 
    831789,				/* 231.05 hours, 9.63 days */ 
    889303,				/* 247.03 hours, 10.29 days */ 
    950794,				/* 264.11 hours, 11.00 days */ 
    1016537,				/* 282.37 hours, 11.77 days */ 
    1086825,				/* 301.90 hours, 12.58 days */ 
    1161973,				/* 322.77 hours, 13.45 days */ 
    1242318,				/* 345.09 hours, 14.38 days */ 
    1328218,				/* 368.95 hours, 15.37 days */ 
    1420057,				/* 394.46 hours, 16.44 days */ 
    1518247,				/* 421.74 hours, 17.57 days */ 
    1623226,				/* 450.90 hours, 18.79 days */ 
    1735464,				/* 482.07 hours, 20.09 days */ 
    1855462,				/* 515.41 hours, 21.48 days */ 
    1983758,				/* 551.04 hours, 22.96 days */ 
    2120925,				/* 589.15 hours, 24.55 days */ 
    2267576,				/* 629.88 hours, 26.25 days */ 
    2424367,				/* 673.44 hours, 28.06 days */ 
    2592000};				/* 720.00 hours, 30.00 days */ 


#ifndef NEVERDATE
#define NEVERDATE 0xffffffff
#endif


afs_uint32
life_to_time(afs_uint32 start, int life_)
{
    int realLife;

    if (life_ == TKTLIFENOEXPIRE) return NEVERDATE;
    if (life_ < TKTLIFEMINFIXED) return start + life_*5*60;
    if (life_ > TKTLIFEMAXFIXED) return start + MAXTKTLIFETIME;
    realLife = tkt_lifetimes[life_ - TKTLIFEMINFIXED];
    return start + realLife;
}

int
time_to_life(afs_uint32 start, afs_uint32 end)
{
    int lifetime = end-start;
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

/* function returns:
 *
 * -2 if zero or negative lifetime, or start time is more than now plus time
 * uncertainty plus max ticket lifetime, or if there is an end time, it's
 * before now minus uncertainty, the start time is non-zero, and now minus
 * the start time is greater than the max ticket lifetime plus 24 hours
 *
 * -1 if there is an end time, it's before now minus uncertainty, and the
 * start time is not non-zero or now minus the start time is not greater
 * than the max ticket lifetime plus 24 hours
 *
 * 0 if the times are consistent (not covered by above) but start time is 
 * less than now plus uncertainty
 *
 * 1 if the start time is in the past and the end time is infinity.
 *
 * 2 if the start time is past and the end time is in the future
 * and the lifetime is within the legal limit.
 */

int
tkt_CheckTimes(afs_uint32 begin, afs_uint32 end, afs_uint32 now)
{
    if (end <= begin
	|| begin > now + KTC_TIME_UNCERTAINTY + MAXKTCTICKETLIFETIME
	|| (end
	    && end < now - KTC_TIME_UNCERTAINTY
	    && now - begin > MAXKTCTICKETLIFETIME + MAXKTCTICKETLIFETIME))
	return -2;
    if (end
	&& end < now - KTC_TIME_UNCERTAINTY
	&& (begin == 0 || now - begin <= 2 * MAXKTCTICKETLIFETIME))
	return -1;
    if (begin < now + KTC_TIME_UNCERTAINTY)
	return 0;
    if (begin < now && end == 0)
	return 1;
    if (begin < now
	&& end > now
	&& (end - begin) < MAXKTCTICKETLIFETIME)
	return 2;
    return 2;
}

#define getstr(name,min) \
    slen = strlen(ticket); \
    if ((slen < min) || (slen >= MAXKTCNAMELEN)) return -1; \
    strcpy (name, ticket); \
    ticket += slen+1


static int
decode_athena_ticket (ticket, ticketLen, name, inst, realm,
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
{
    char *ticketBeg = ticket;
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

    memcpy(host, ticket, sizeof (*host));
    ticket += sizeof(*host);
    *host = ktohl (flags, *host);

    memcpy(sessionKey, ticket, sizeof (struct ktc_encryptionKey));
    ticket += sizeof (struct ktc_encryptionKey);

    lifetime = *ticket++;
    memcpy(start, ticket, sizeof (*start));
    ticket += sizeof(*start);
    *start = ktohl (flags, *start);
    *end = life_to_time (*start, lifetime);

    /* XXX check sname, sinst */
    getstr (sname, 1);
    getstr (sinst, 0);

    tlen = ticket - ticketBeg;

    if ((round_up_to_ebs(tlen) != ticketLen) && (ticketLen != 56))
	return -1;
    return 0;
}

int
decomp_ticket(char *data,
	      int length,
	      char *pname,
	      char *pinstance,
	      char *prealm,
	      afs_int32 *paddress,
	      unsigned char *session,
	      afs_int32 *start,
	      afs_int32 *end,
	      struct ktc_encryptionKey *key,
	      des_key_schedule schedule)
{
    int ret;
    char clear_ticket[MAXKTCTICKETLEN], *ticket;

    if (length > MAXKTCTICKETLEN)
	length = MAXKTCTICKETLEN;

    if (key_sched (key, schedule)) 
	return RXKADBADKEY;
    ticket = clear_ticket;

    pcbc_encrypt (data, ticket, length, schedule, key, DECRYPT);

    ret = decode_athena_ticket (ticket, length, pname, pinstance,
				prealm, paddress, session, 
				start, end);

    return ret;
}

#define putstr(name,min) \
    slen = strlen(name); \
    if ((slen < min) || (slen >= MAXKTCNAMELEN)) return -1; \
    strcpy (ticket, name); \
    ticket += slen+1
#define putint(num) num = htonl(num);\
		    memcpy(ticket, &num, sizeof(num));\
		    ticket += sizeof(num)

static int
assemble_athena_ticket(ticket, ticketLen, name, inst, realm,
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

    memcpy(ticket, sessionKey, sizeof(struct ktc_encryptionKey));
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

int tkt_MakeTicket (ticket, ticketLen, key, name, inst, cell,
		    start, end, sessionKey, host, sname, sinst)
  char		*ticket;		/* ticket is constructed here */
  int		*ticketLen;		/* output length of finished ticket */
  struct ktc_encryptionKey    *key;	/* key ticket should be sealed with */
  char		*name;			/* user of this ticket */
  char		*inst;
  char		*cell;			/* cell of authentication */
  afs_uint32	 start,end;		/* life of ticket */
  struct ktc_encryptionKey *sessionKey;	/* session key invented for ticket */
  afs_uint32	 host;			/* caller's host address */
  char		*sname;			/* server */
  char		*sinst;
{
    int		 code;
    des_key_schedule schedule;

    *ticketLen = 0;			/* in case we return early */

    code = assemble_athena_ticket (ticket, ticketLen, name, inst, cell,
				   host, sessionKey, start, end, sname, sinst);

    *ticketLen = round_up_to_ebs(*ticketLen); /* round up */
    if (code)
	return -1;

    /* encrypt ticket */
    if (code = key_sched (key, schedule))
	return RXKADBADKEY;

    pcbc_encrypt (ticket, ticket, *ticketLen, schedule, key, ENCRYPT);
    return 0;
}

int
tkt_DecodeTicket (ticket, ticketLen, key,
		  name, inst, cell, sessionKey, host, start, end)
  char		*ticket;
  afs_int32		 ticketLen;
  struct ktc_encryptionKey *key;
  char		*name;
  char		*inst;
  char		*cell;
  char		*sessionKey;
  afs_int32		*host;
  afs_int32		*start;
  afs_int32		*end;
{
    des_key_schedule schedule;
    int		   code;

    if (ticketLen == 0) 
	return RXKADBADTICKET; /* no ticket */
    if ((ticketLen < MINKTCTICKETLEN) || /* minimum legal ticket size */
	((ticketLen) % 8 != 0))		/* enc. part must be (0 mod 8) bytes */
	return RXKADBADTICKET;

    code = decomp_ticket(ticket, ticketLen, name, inst, cell, 
			 host, sessionKey, start, end, key, schedule);

    if (code) 
	return RXKADBADTICKET;

    if (tkt_CheckTimes (*start, *end, time(0)) < -1) 
	return RXKADBADTICKET;

    return 0;
}

afs_int32
ktohl(char flags, afs_int32 l)
{
    if (flags & 1) {
	unsigned char *lp = (unsigned char *)&l;
	afs_int32	       hl;
	hl = *lp + (*(lp+1) << 8) + (*(lp+2) << 16) + (*(lp+3) << 24);
	return hl;
    }
    return ntohl(l);
}
