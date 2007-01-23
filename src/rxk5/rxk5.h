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

#if 0
#if !defined(KERNEL) || defined(UKERNEL)
#ifdef USING_SSL
#include "k5ssl.h"
#else
#ifdef USING_SHISHI
#include <shishi.h>
#else
#ifdef private
#undef private
#if HAVE_PARSE_UNITS_H
#include "parse_units.h"
#endif
#endif
#include <krb5.h>
#endif
#endif
#else
#include <k5ssl.h>
#endif
#endif

#define RXK5_MAXKTCTICKETLEN 12000
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

int rxk5_GetServerInfo();
struct rx_securityClass * rxk5_NewClientSecurityObject();
struct rx_securityClass * rxk5_NewServerSecurityObject();
int rxk5_default_get_key();

#ifdef USING_SHISHI
#ifdef SHISHI_VERSION
Shishi * rxk5_get_context();
#endif
#endif
#if defined(USING_MIT) || defined(USING_HEIMDAL) || defined(USING_SSL)
#ifdef KRB5_TC_MATCH_KTYPE
krb5_context rxk5_get_context();
#endif
#endif
