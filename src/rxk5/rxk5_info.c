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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <afs/stds.h>
#include <afsconfig.h>
#include <rx/rx.h>
#include <rx/xdr.h>
#ifdef USING_SHISHI
#include <shishi.h>
#else
#ifdef USING_SSL
#include "k5ssl.h"
#else
#if HAVE_PARSE_UNITS_H
#include "parse_units.h"
#endif
#include <krb5.h>
#endif
#endif
#include <assert.h>
#include <assert.h>
#include <com_err.h>

#include "rxk5imp.h"
#include "rxk5c.h"
#include "rxk5errors.h"

extern int rxk5_s_Close();

int
rxk5_GetServerInfo2(struct rx_connection *conn,
    int *level,
    int *expires,
    char **client,
    char **server,
    int *kvno,
    int *enctype)
{
    struct rxk5_sconn *sconn = (struct rxk5_sconn*)conn->securityData;

    if (conn->securityObject->ops->op_Close != rxk5_s_Close
	    || !sconn || !(sconn->flags & AUTHENTICATED) || !sconn->client) {
	return RXK5NOAUTH;
    }
    if (level)
	*level = sconn->level;
    if (expires)
	*expires = sconn->expires;
    if (client)
	*client = sconn->client;
    if (server)
	*server = sconn->server;
    if (kvno)
	*kvno = sconn->kvno;
    if (enctype)
#ifdef USING_SHISHI
	*enctype = shishi_key_type(sconn->hiskey->sk);
#else
#ifdef USING_HEIMDAL
	*enctype = sconn->hiskey->keytype;
#else
	*enctype = sconn->hiskey->enctype;
#endif
#endif
    return 0;
}


int
rxk5_GetServerInfo(struct rx_connection *conn,
    int *level,
    int *expires,
    char **princ,
    int *kvno,
    int *enctype)
{
    return rxk5_GetServerInfo2(conn, level, expires, princ, NULL, kvno, enctype);
}
