/*
 * Copyright (c) 2005
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works 
 * and redistribute this software and such derivative works 
 * for any purpose, so long as the name of the University of 
 * Michigan is not used in any advertising or publicity 
 * pertaining to the use or distribution of this software 
 * without specific, written prior authorization.  If the 
 * above copyright notice or any other identification of the 
 * University of Michigan is included in any copy of any 
 * portion of this software, then the disclaimer below must 
 * also be included.
 *
 * This software is provided as is, without representation 
 * from the University of Michigan as to its fitness for any 
 * purpose, and without warranty by the University of 
 * Michigan of any kind, either express or implied, including 
 * without limitation the implied warranties of 
 * merchantability and fitness for a particular purpose.  The 
 * regents of the University of Michigan shall not be liable 
 * for any damages, including special, indirect, incidental, or 
 * consequential damages, with respect to any claim arising 
 * out of or in connection with the use of the software, even 
 * if it has been or is hereafter advised of the possibility of 
 * such damages.
 */

#ifndef RXK5_H
#define RXK5_H

/* don't include krb5.h here.  rxk5c.{cs,ss,xdr}.c
 * doesn't include afsconfig.h so this header file must
 * not assume it knows how to include krb5.h, therefore
 * it must do something "safe" if krb5.h wasn't included
 * before this.  See below for "failsafe" types.
 */

#define RXK5_MAXKTCTICKETLEN 12000
/* Note: rxk5_clear isn't available unless rxk5 is compiled specially. */
#define rxk5_clear 0	/* identity only (insecure) */
#define rxk5_auth 1	/* integrity checked */
#define rxk5_crypt 2	/* integrity & privacy */
#if 0
struct rxk5_stats {
    int connections[3];
    int destroyObject;
    int destroyClient;
    int destroyUnused;
    int destroyUnauth;
    int destroyConn[3];
    int expired;
    int challengesSent;
    int challenges[3];
    int responses[3];
    int preparePackets[6];
    int checkPackets[6];
    int bytesEncrypted[2];
    int bytesDecrypted[2];
    int fc_encrypts[2];
    int fc_key_scheds;
    int des_encrypts[2];
    int des_key_scheds;
    int des_randoms;
    int clientObjects;
    int serverObjects;
    int spares[8];
};
#endif

/* if krb5.h wasn't included before this file,
 * the proper types might not be available.
 * Here are "fail-safe" types for when this happens.
 */
#ifdef USING_SHISHI
#ifdef SHISHI_VERSION
#define RXK5_K5_CONTEXT	Shishi *
#define RXK5_K5_PRINCIPAL char *
#define RXK5_K5_CREDS Shishi_tkt *
#endif
#endif
#if defined(USING_MIT) || defined(USING_HEIMDAL) || defined(USING_K5SSL)
#ifdef KRB5_TC_MATCH_KTYPE
#define RXK5_K5_CONTEXT krb5_context
#define RXK5_K5_PRINCIPAL krb5_principal
#define RXK5_K5_CREDS krb5_creds *
#define RXK5_K5_KEYBLOCK krb5_keyblock *
#endif
#endif
#ifndef RXK5_K5_CONTEXT
#define RXK5_K5_CONTEXT void *
#endif
#ifndef RXK5_K5_PRINCIPAL
#define RXK5_K5_PRINCIPAL void *
#endif
#ifndef RXK5_K5_CREDS
#define RXK5_K5_CREDS void *
#endif
#ifndef RXK5_K5_KEYBLOCK
#define RXK5_K5_KEYBLOCK void *
#endif

RXK5_K5_CONTEXT rxk5_get_context(RXK5_K5_CONTEXT);
int rxk5_GetServerInfo2(struct rx_connection *, int *, int *, char **, char **,
    int *, int *);
struct rx_securityClass * rxk5_NewClientSecurityObject(int, RXK5_K5_CREDS,
    RXK5_K5_CONTEXT);
struct rx_securityClass * rxk5_NewServerSecurityObject(int, void *, int (*)(void *,
	RXK5_K5_CONTEXT, RXK5_K5_PRINCIPAL, int, int, RXK5_K5_KEYBLOCK),
    int (*)(char *, char *, int, int, int), RXK5_K5_CONTEXT);
int rxk5_default_get_key(void *, RXK5_K5_CONTEXT, RXK5_K5_PRINCIPAL, int, int,
    RXK5_K5_KEYBLOCK);

#undef RXK5_K5_KEYBLOCK
#undef RXK5_K5_CONTEXT
#undef RXK5_K5_PRINCIPAL
#undef RXK5_K5_CREDS

#endif /* RXK5_H */
